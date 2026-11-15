/** Claude: diagnostic DXE driver for Xiaomi Pad 6 (pipa).
  On the first ReadyToBoot event, exports UFS LUN0 (/dev/block/sda equivalent) over USB Mass Storage
  using Qualcomm UsbMsdDxe, shows a green screen + text, and loops forever.
  Volume Up -> stop USB and cold reset. No OS is ever booted by this image.
  Logic adapted from Project-Silicium Mu-Silicium QcomPkg/Applications/MassStorage (BSD-2-Clause-Patent). **/
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/BlockIo.h>
#include <Protocol/EFIUsbMsd.h>
#include <Guid/EventGroup.h>

typedef struct {
  VENDOR_DEVICE_PATH       Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} LU_DEVICE_PATH;

STATIC LU_DEVICE_PATH mLu0Path = {
  { { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } }, { 0 } },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { END_DEVICE_PATH_LENGTH, 0 } }
};

STATIC EFI_EVENT mReadyToBootEvent;
STATIC BOOLEAN   mStarted = FALSE;

STATIC VOID
Screen (EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, UINT8 R, UINT8 G, UINT8 B, CHAR8 *Msg1, CHAR8 *Msg2)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Bg, Fg;
  UINTN W, H;
  if (Gop == NULL) return;
  W = Gop->Mode->Info->HorizontalResolution;
  H = Gop->Mode->Info->VerticalResolution;
  Bg.Red = R; Bg.Green = G; Bg.Blue = B; Bg.Reserved = 0;
  Fg.Red = Fg.Green = Fg.Blue = 255; Fg.Reserved = 0;
  Gop->Blt (Gop, &Bg, EfiBltVideoFill, 0, 0, 0, 0, W, H, 0);
  if (Msg1 != NULL) AsciiPrintXY (40, H / 3,     &Fg, &Bg, Msg1);
  if (Msg2 != NULL) AsciiPrintXY (40, H / 3 + 40, &Fg, &Bg, Msg2);
}

STATIC VOID
EFIAPI
OnReadyToBoot (IN EFI_EVENT Event, IN VOID *Context)
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
  EFI_USB_MSD_PROTOCOL         *Msd = NULL;
  EFI_BLOCK_IO_PROTOCOL        *BlkIo = NULL;
  EFI_DEVICE_PATH_PROTOCOL     *Dp;
  EFI_HANDLE                    Handle = NULL;
  EFI_INPUT_KEY                 Key;

  if (mStarted) return;
  mStarted = TRUE;
  gBS->CloseEvent (Event);
  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);

  Status = gBS->LocateProtocol (&gEfiUsbMsdProtocolGuid, NULL, (VOID **)&Msd);
  if (EFI_ERROR (Status)) { Screen (Gop, 160, 0, 0, "MSC ERROR: UsbMsd protocol not found", NULL); CpuDeadLoop (); }

  CopyGuid (&mLu0Path.Vendor.Guid, &gEfiUfsLU0Guid);
  Dp = (EFI_DEVICE_PATH_PROTOCOL *)&mLu0Path;
  Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &Dp, &Handle);
  if (!EFI_ERROR (Status)) Status = gBS->HandleProtocol (Handle, &gEfiBlockIoProtocolGuid, (VOID **)&BlkIo);
  if (EFI_ERROR (Status)) { Screen (Gop, 160, 0, 0, "MSC ERROR: UFS LUN0 BlockIo not found", NULL); CpuDeadLoop (); }

  Status = Msd->AssignBlkIoHandle (Msd, BlkIo, 0);
  if (EFI_ERROR (Status)) { Screen (Gop, 160, 0, 0, "MSC ERROR: AssignBlkIoHandle failed", NULL); CpuDeadLoop (); }

  Status = Msd->StartDevice (Msd);
  if (EFI_ERROR (Status)) { Screen (Gop, 160, 0, 0, "MSC ERROR: StartDevice failed", NULL); CpuDeadLoop (); }

  Screen (Gop, 0, 110, 0, "USB MASS STORAGE ACTIVE - UFS LUN0 (sda)", "Volume Up = stop and reboot");

  while (TRUE) {
    Msd->EventHandler (Msd);
    if (gST->ConIn != NULL && !EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key)) && Key.ScanCode == SCAN_UP) break;
  }
  Msd->StopDevice (Msd);
  Msd->AssignBlkIoHandle (Msd, NULL, 0);
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
}

EFI_STATUS
EFIAPI
AutoMassStorageEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnReadyToBoot, NULL, &gEfiEventReadyToBootGuid, &mReadyToBootEvent);
}
