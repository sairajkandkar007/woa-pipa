# AutoMassStorageDxe (diagnostic, Mu-Silicium tag 2.3 era)

A small DXE driver that, on the first ReadyToBoot event, exports UFS LUN0 over USB Mass
Storage (Qualcomm `UsbMsdDxe` protocol), shows a green screen, and loops (Volume Up = reboot).
It never boots an OS. Logic is adapted from Mu-Silicium `QcomPkg/Applications/MassStorage`.

Used once to regain disk access when the XDA TWRP boot-looped. Current upstream builds
already include Mass Storage (hold **Volume Down** at the Silicium boot screen), so this is
only needed for 2.3-era firmware.

Integration into tag 2.3:
- copy `EFIUsbMsd.h` from current Mu-Silicium into `QcomPkg/Include/Protocol/`, add
  `gEfiUsbMsdProtocolGuid = { 0xC8591FAF, 0xDBCC, 0x479E, { 0x9E, 0xF2, 0xFD, 0x08, 0x5B, 0xC3, 0x7B, 0xC7 } }` to `QcomPkg.dec`
- add the INF to `QcomPkg.dsc.inc` components and to `pipaPkg/Include/DXE.inc` after `UsbMsdDxe`
- **required:** use current Device-Binaries' pipa `UsbMsdDxe.patched.efi` and `UsbConfigDxe.patched.efi`
  (the original 2.3 binaries fail with `StartDevice` errors).
