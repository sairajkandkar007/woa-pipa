/** @file
  PcieInitDxe - bring up PCIe0 on Xiaomi Pad 6 (pipa, SM8250) so Windows' pci.sys can enumerate
  the QCA6390 Wi-Fi (17cb:1101) through the Kona MCFG (ECAM at 0x60000000, segment 0).

  The stock Android UEFI has no PcieDxe. This driver does what Linux does at boot:
    - clocks / GDSC / PHY reset through the Xiaomi ClockDxe (EFI_CLOCK_PROTOCOL, names as in its strings)
    - QMP gen3x1 PHY init      (drivers/phy/qualcomm/phy-qcom-qmp-pcie.c, sm8250 tables, see PhyTable.h)
    - PARF/controller init     (drivers/pci/controller/dwc/pcie-qcom.c, qcom_pcie_init_2_7_0 / 1_9_0 ops)
    - DesignWare RC setup      (pcie-designware-host.c: bus numbers, class, BAR, iATU outbound)
    - PERST# on gpio79, CLKREQ# gpio80 (pci_e0), WAKE# gpio81
  For ECAM, iATU region 0 maps 0x60100000-0x601FFFFF to config space of bus 1 in CFG shift mode
  (ECAM offset -> BDF), region 1 maps the memory window 0x60200000-0x61FEFFFF 1:1.

  Results are left in hardware (link status, bus-1 vendor ID) and in the variable PipaPcieStatus
  (vendor GUID below) so Windows/KD can read them.
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/EFIClock.h>
#include <Protocol/AcpiTable.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

typedef struct { UINT16 Off; UINT8 Val; } PHY_REG;
#include "PhyTable.h"

#define PARF_BASE      0x01C00000ULL
#define PHY_BASE       0x01C06000ULL
#define DBI_BASE       0x60000000ULL
#define ELBI_BASE      0x60000F20ULL
#define ATU_BASE       0x60001000ULL
#define CFG_BASE       0x60100000ULL
#define MEM_BASE       0x60200000ULL
#define MEM_LIMIT      0x61FEFFFFULL
#define GCC_BASE       0x00100000ULL
#define TLMM_NORTH     0x0F900000ULL

/* PARF (pcie-qcom.c) */
#define PARF_SYS_CTRL                  0x00
#define PARF_PM_CTRL                   0x20
#define PARF_PHY_CTRL                  0x40
#define PARF_MHI_CLOCK_RESET_CTRL      0x174
#define PARF_AXI_MSTR_WR_ADDR_HALT_V2  0x1A8
#define PARF_LTSSM                     0x1B0
#define PARF_DBI_BASE_ADDR_V2          0x350
#define PARF_DBI_BASE_ADDR_V2_HI       0x354
#define PARF_ATU_BASE_ADDR             0x634
#define PARF_ATU_BASE_ADDR_HI          0x638
#define PARF_DEVICE_TYPE               0x1000
#define DEVICE_TYPE_RC                 0x4

/* DWC */
#define DBI_MISC_CONTROL_1             0x8BC
#define DBI_PORT_LINK_CONTROL          0x710
#define DBI_PL_DEBUG0                  0x728
#define ATU_REGION(i)                  (ATU_BASE + (i) * 0x200)
#define ATU_CTRL1  0x00
#define ATU_CTRL2  0x04
#define ATU_LBASE  0x08
#define ATU_UBASE  0x0C
#define ATU_LIMIT  0x10
#define ATU_LTGT   0x14
#define ATU_UTGT   0x18
#define ATU_ULIMIT 0x20
#define ATU_TYPE_MEM  0x0
#define ATU_TYPE_CFG0 0x4
#define ATU_ENABLE    BIT31
#define ATU_CFG_SHIFT BIT28

/* GCC block resets (gcc-sm8250.c) */
#define GCC_PCIE_0_BCR       0x6B000
#define GCC_PCIE_0_PHY_BCR   0x6C01C

#define PIPA_PCIE_VAR_GUID \
  { 0x7a5c3e10, 0x6390, 0x4c1f, { 0x9a, 0x2b, 0x50, 0x69, 0x70, 0x61, 0x00, 0x01 } }
STATIC EFI_GUID mVarGuid = PIPA_PCIE_VAR_GUID;

typedef struct {
  UINT32 Step;          /* last step reached */
  UINT32 PhyStatus;     /* PCS_STATUS1 after start */
  UINT32 LinkStatus;    /* PCIe cap Link Status|Control (dword at cap+0x10) */
  UINT32 Ltssm;         /* PL_DEBUG0 */
  UINT32 Bus1Id;        /* vendor/device read through ECAM bus 1 */
  UINT32 ClockErrors;   /* bitmask of clocks/power domains that failed */
} PIPA_PCIE_STATUS;

STATIC EFI_CLOCK_PROTOCOL *mClock;
STATIC VOID SaveStatus (VOID);
STATIC PIPA_PCIE_STATUS    mSt;

STATIC VOID Tlmm (UINT32 Pin, UINT32 Ctl, INT32 Out)
{
  UINTN B = (UINTN)(TLMM_NORTH + 0x1000ULL * Pin);
  if (Out >= 0) {
    MmioWrite32 (B + 4, Out ? 2 : 0);
  }
  MmioWrite32 (B, Ctl);
}

STATIC VOID ClkOn (CONST CHAR8 *Name, UINT32 Hz, UINT32 Bit)
{
  UINTN Id; UINT32 R; EFI_STATUS S;
  S = mClock->GetClockID (mClock, Name, &Id);
  if (!EFI_ERROR (S) && Hz) {
    S = mClock->SetClockFreqHz (mClock, Id, Hz, EFI_CLOCK_FREQUENCY_HZ_CLOSEST, &R);
  }
  if (!EFI_ERROR (S)) {
    S = mClock->EnableClock (mClock, Id);
  }
  if (EFI_ERROR (S)) {
    mSt.ClockErrors |= (1u << Bit);
    DEBUG ((EFI_D_ERROR, "PcieInit: clock %a failed %r\n", Name, S));
  }
}

STATIC VOID GccReset (UINT32 Off)
{
  MmioOr32 ((UINTN)(GCC_BASE + Off), 1);
  gBS->Stall (1000);
  MmioAnd32 ((UINTN)(GCC_BASE + Off), ~1u);
  gBS->Stall (1000);
}

STATIC UINT32 FindPcieCap (VOID)
{
  UINT32 P = MmioRead8 ((UINTN)(DBI_BASE + 0x34)), N = 0;
  while (P && N++ < 48) {
    if (MmioRead8 ((UINTN)(DBI_BASE + P)) == 0x10) {
      return P;
    }
    P = MmioRead8 ((UINTN)(DBI_BASE + P + 1));
  }
  return 0;
}

STATIC VOID AtuOutbound (UINT32 Idx, UINT32 Type, UINT32 Ctrl2Extra, UINT64 Base, UINT64 Limit, UINT64 Target)
{
  UINTN R = (UINTN)ATU_REGION (Idx);
  MmioWrite32 (R + ATU_LBASE,  (UINT32)Base);
  MmioWrite32 (R + ATU_UBASE,  (UINT32)(Base >> 32));
  MmioWrite32 (R + ATU_LIMIT,  (UINT32)Limit);
  MmioWrite32 (R + ATU_ULIMIT, (UINT32)(Limit >> 32));
  MmioWrite32 (R + ATU_LTGT,   (UINT32)Target);
  MmioWrite32 (R + ATU_UTGT,   (UINT32)(Target >> 32));
  MmioWrite32 (R + ATU_CTRL1,  Type);
  MmioWrite32 (R + ATU_CTRL2,  ATU_ENABLE | Ctrl2Extra);
  gBS->Stall (10);
}

STATIC VOID SaveStatus (VOID)
{
  gRT->SetVariable (L"PipaPcieStatus", &mVarGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof (mSt), &mSt);
}

#ifndef PCIE_STOP_AFTER
#define PCIE_STOP_AFTER 99
#endif
#define STOP_IF(n) do { DEBUG ((EFI_D_ERROR, "PcieInit: step %d done\n", (n))); if (PCIE_STOP_AFTER <= (n)) return; } while (0)
#define TRACE(x) DEBUG ((EFI_D_ERROR, "PcieInit: %a\n", x))

STATIC VOID PcieInit (VOID)
{
  UINTN  Id, i;
  UINT32 Cap, T;

  /* 1. pins: PERST# asserted (low), CLKREQ# = pci_e0, WAKE# input pull-up */
  Tlmm (79, 0x1 | (0 << 2) | (0 << 6) | BIT9, 0);  /* gpio, pull-down, 2 mA, output low */
  Tlmm (80, 0x3 | (1 << 2), -1);                    /* pci_e0, pull-up */
  Tlmm (81, 0x3, -1);                               /* gpio input, pull-up */
  mSt.Step = 1;
  STOP_IF (1);

  /* 2. power domain + clocks */
  if (EFI_ERROR (mClock->GetClockPowerDomainID (mClock, "pcie_0_gdsc", &Id)) ||
      EFI_ERROR (mClock->EnableClockPowerDomain (mClock, Id))) {
    mSt.ClockErrors |= BIT31;
    DEBUG ((EFI_D_ERROR, "PcieInit: pcie_0_gdsc failed\n"));
    return;
  }
  ClkOn ("gcc_pcie_0_aux_clk",          19200000, 0);
  ClkOn ("gcc_pcie_0_cfg_ahb_clk",      0,        1);
  ClkOn ("gcc_pcie_0_mstr_axi_clk",     0,        2);
  ClkOn ("gcc_pcie_0_slv_axi_clk",      0,        3);
  ClkOn ("gcc_pcie_0_slv_q2a_axi_clk",  0,        4);
  ClkOn ("gcc_aggre_noc_pcie_tbu_clk",  0,        5);
  ClkOn ("gcc_ddrss_pcie_sf_tbu_clk",   0,        6);
  ClkOn ("gcc_pcie_phy_aux_clk",        0,        7);
  ClkOn ("gcc_pcie_wifi_clkref_en",     0,        8);
  ClkOn ("gcc_pcie0_phy_refgen_clk",    100000000, 9);
  /* belt and braces: make sure the GDSC and the branches the controller/PHY registers need are really on,
     straight in GCC (gcc-sm8250.c), and wait for each branch to report running (CBCR bit31 CLK_OFF = 0) */
  {
    STATIC CONST UINT32 Cbcr[] = { 0x6b024 /* cfg_ahb */, 0x6b01c /* mstr_axi */, 0x6b014 /* slv_axi */,
                                   0x6b010 /* slv_q2a */, 0x6b028 /* aux */, 0x6f004 /* phy_aux */,
                                   0x6f02c /* refgen */, 0x9000c /* aggre tbu */, 0x8d058 /* ddrss sf tbu */,
                                   0x8c004 /* wifi clkref */ };
    UINTN g = (UINTN)(GCC_BASE + 0x6b004);
    MmioAnd32 (g, ~BIT0);                                                /* GDSCR SW_COLLAPSE off */
    for (T = 0; T < 1000 && !(MmioRead32 (g) & BIT31); T++) gBS->Stall (10);
    if (!(MmioRead32 (g) & BIT31)) mSt.ClockErrors |= BIT30;
    MmioOr32 ((UINTN)(GCC_BASE + 0x52008), BIT0 | BIT1 | BIT2 | BIT3 | BIT5); /* slv, mstr, cfg, aux, q2a votes */
    MmioOr32 ((UINTN)(GCC_BASE + 0x6f004), BIT0);
    MmioOr32 ((UINTN)(GCC_BASE + 0x6f02c), BIT0);
    MmioOr32 ((UINTN)(GCC_BASE + 0x9000c), BIT0);
    MmioOr32 ((UINTN)(GCC_BASE + 0x8d058), BIT0);
    MmioOr32 ((UINTN)(GCC_BASE + 0x8c004), BIT0);
    for (i = 0; i < ARRAY_SIZE (Cbcr); i++) {
      for (T = 0; T < 200 && (MmioRead32 ((UINTN)(GCC_BASE + Cbcr[i])) & BIT31); T++) gBS->Stall (10);
      if (MmioRead32 ((UINTN)(GCC_BASE + Cbcr[i])) & BIT31) mSt.ClockErrors |= (1u << (16 + i));
    }
    SaveStatus ();
    DEBUG ((EFI_D_ERROR, "PcieInit: gdscr %08x vote %08x cfg_ahb %08x mstr %08x slv %08x errs %08x\n",
            MmioRead32 (g), MmioRead32 ((UINTN)(GCC_BASE + 0x52008)), MmioRead32 ((UINTN)(GCC_BASE + 0x6b024)),
            MmioRead32 ((UINTN)(GCC_BASE + 0x6b01c)), MmioRead32 ((UINTN)(GCC_BASE + 0x6b014)), mSt.ClockErrors));
    if (mSt.ClockErrors & (BIT30 | BIT16)) {   /* GDSC or cfg_ahb not running: touching PARF would hang */
      DEBUG ((EFI_D_ERROR, "PcieInit: clocks not running (%x), giving up\n", mSt.ClockErrors));
      return;
    }
  }
  mSt.Step = 2;
  STOP_IF (2);

  /* 3. controller reset + PARF init (qcom_pcie_init_2_7_0) */
#ifndef PCIE_SKIP_BCR
  GccReset (GCC_PCIE_0_BCR);
#endif
  mSt.Step = 21;
  if (PCIE_STOP_AFTER == 21) return;
  TRACE ("read PARF_SYS_CTRL");
  DEBUG ((EFI_D_ERROR, "PcieInit: PARF_SYS_CTRL %08x\n", MmioRead32 ((UINTN)(PARF_BASE + PARF_SYS_CTRL))));
  TRACE ("write PARF_DEVICE_TYPE");
  MmioWrite32 ((UINTN)(PARF_BASE + PARF_DEVICE_TYPE), DEVICE_TYPE_RC);
  TRACE ("PARF ok");
  MmioAnd32 ((UINTN)(PARF_BASE + PARF_PHY_CTRL), ~BIT0);                 /* PHY_TEST_PWR_DOWN off */
  MmioWrite32 ((UINTN)(PARF_BASE + PARF_DBI_BASE_ADDR_V2),    (UINT32)DBI_BASE);
  MmioWrite32 ((UINTN)(PARF_BASE + PARF_DBI_BASE_ADDR_V2_HI), 0);
  MmioWrite32 ((UINTN)(PARF_BASE + PARF_ATU_BASE_ADDR),       (UINT32)ATU_BASE);
  MmioWrite32 ((UINTN)(PARF_BASE + PARF_ATU_BASE_ADDR_HI),    0);
  MmioAnd32 ((UINTN)(PARF_BASE + PARF_SYS_CTRL), ~BIT29);                /* MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN */
  MmioOr32 ((UINTN)(PARF_BASE + PARF_MHI_CLOCK_RESET_CTRL), BIT4);       /* BYPASS */
  MmioAnd32 ((UINTN)(PARF_BASE + PARF_PM_CTRL), ~BIT5);                  /* allow L1 */
  MmioOr32 ((UINTN)(PARF_BASE + PARF_AXI_MSTR_WR_ADDR_HALT_V2), BIT31);
  mSt.Step = 3;
  STOP_IF (3);

  /* 4. PHY: reset, tables, pipe clock, start (qmp_pcie_init + qmp_pcie_power_on) */
  GccReset (GCC_PCIE_0_PHY_BCR);
  MmioOr32 ((UINTN)(PHY_BASE + PHY_PCS_POWER_DOWN_CONTROL), BIT0 | BIT1); /* SW_PWRDN | REFCLK_DRV_DSBL */
  for (i = 0; i < ARRAY_SIZE (mPhyInit); i++) {
    MmioWrite32 ((UINTN)(PHY_BASE + mPhyInit[i].Off), mPhyInit[i].Val);
  }
  MmioAnd32 ((UINTN)(PHY_BASE + PHY_PCS_SW_RESET), ~BIT0);
  MmioOr32 ((UINTN)(PHY_BASE + PHY_PCS_START_CONTROL), BIT0 | BIT1);     /* SERDES_START | PCS_START */
  /* pipe clock comes from the PHY once it runs: vote the branch directly (Linux uses BRANCH_HALT_SKIP;
     ClockDxe's EnableClock would wait for a clock that only exists after PHY start) */
  MmioOr32 ((UINTN)(GCC_BASE + 0x52008), BIT4);
  gBS->Stall (1000);
  for (T = 0; T < 100 && (MmioRead32 ((UINTN)(PHY_BASE + PHY_PCS_PCS_STATUS1)) & BIT6); T++) {
    gBS->Stall (200);
  }
  mSt.PhyStatus = MmioRead32 ((UINTN)(PHY_BASE + PHY_PCS_PCS_STATUS1));
  if (mSt.PhyStatus & BIT6) {
    DEBUG ((EFI_D_ERROR, "PcieInit: PHY did not come up (status %x)\n", mSt.PhyStatus));
    return;
  }
  mSt.Step = 4;
  STOP_IF (4);

  /* 5. DWC root complex setup (dw_pcie_setup_rc subset) */
  MmioOr32 ((UINTN)(DBI_BASE + DBI_MISC_CONTROL_1), BIT0);               /* DBI RO write enable */
  MmioWrite32 ((UINTN)(DBI_BASE + 0x10), 0x00000004);                    /* BAR0: 64-bit mem, unused */
  MmioWrite32 ((UINTN)(DBI_BASE + 0x14), 0);
  MmioWrite32 ((UINTN)(DBI_BASE + 0x18), 0x00FF0100);                    /* primary 0, secondary 1, subordinate ff */
  MmioWrite16 ((UINTN)(DBI_BASE + 0x0A), 0x0604);                        /* class: PCI-PCI bridge */
  MmioOr32 ((UINTN)(DBI_BASE + 0x04), BIT0 | BIT1 | BIT2 | BIT8);        /* IO, MEM, bus master, SERR */
  Cap = FindPcieCap ();
  if (Cap) {                                                             /* qcom_pcie_clear_hpc / aspm L0s */
    MmioAnd32 ((UINTN)(DBI_BASE + Cap + 0x14), ~BIT6);                   /* SLTCAP.HPC */
    MmioAnd32 ((UINTN)(DBI_BASE + Cap + 0x0C), ~BIT10);                  /* LNKCAP ASPM L0s */
  }
  MmioAnd32 ((UINTN)(DBI_BASE + DBI_MISC_CONTROL_1), ~BIT0);
  /* shift mode forms BDF from address bits [27:12]; target 0x00100000 gives bus 1 whether the core
   uses the translated or untranslated address */
  AtuOutbound (0, ATU_TYPE_CFG0, ATU_CFG_SHIFT, CFG_BASE, CFG_BASE + 0xFFFFF, 0x00100000);
  AtuOutbound (1, ATU_TYPE_MEM, 0, MEM_BASE, MEM_LIMIT, MEM_BASE);
  mSt.Step = 5;
  STOP_IF (5);

  /* 6. release PERST# (>=100 ms after power), start link training, wait for DL_Active */
  gBS->Stall (100000);
  Tlmm (79, 0x1 | BIT9, 1);
  gBS->Stall (1000);
  MmioOr32 ((UINTN)(PARF_BASE + PARF_LTSSM), BIT8);
  for (T = 0; T < 100; T++) {                                            /* up to ~1 s */
    if (Cap && (MmioRead16 ((UINTN)(DBI_BASE + Cap + 0x12)) & BIT13)) {
      break;
    }
    gBS->Stall (10000);
  }
  mSt.Ltssm = MmioRead32 ((UINTN)(DBI_BASE + DBI_PL_DEBUG0));
  mSt.LinkStatus = Cap ? MmioRead32 ((UINTN)(DBI_BASE + Cap + 0x10)) : 0;
  if (!(mSt.LinkStatus & (BIT13 << 16))) {
    DEBUG ((EFI_D_ERROR, "PcieInit: link down (ltssm %x lnk %x)\n", mSt.Ltssm, mSt.LinkStatus));
    return;
  }
  mSt.Step = 6;
  STOP_IF (6);

  /* 7. who is on bus 1? (expect 0x110117CB) */
  gBS->Stall (100000);
  mSt.Bus1Id = MmioRead32 ((UINTN)CFG_BASE);
  mSt.Step = 7;
  DEBUG ((EFI_D_WARN, "PcieInit: link up, bus1 id %08x\n", mSt.Bus1Id));
}


/* MCFG is installed only when the link came up: with a dead controller, pci.sys probing the ECAM window
   would stall the bus. Segment 0, buses 0-1 at 0x60000000 (bus 0 = DWC DBI, bus 1 = iATU CFG window). */
#pragma pack(1)
typedef struct {
  EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER                    Header;
  EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE Seg0;
} PIPA_MCFG;
#pragma pack()

STATIC VOID InstallMcfg (VOID)
{
  EFI_ACPI_TABLE_PROTOCOL *Acpi; UINTN Key; EFI_STATUS S;
  PIPA_MCFG M;
  ZeroMem (&M, sizeof (M));
  M.Header.Header.Signature = EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE;
  M.Header.Header.Length = sizeof (M);
  M.Header.Header.Revision = 1;
  CopyMem (M.Header.Header.OemId, "QCOM  ", 6);
  M.Header.Header.OemTableId = SIGNATURE_64 ('Q', 'C', 'O', 'M', 'E', 'D', 'K', '2');
  M.Header.Header.OemRevision = 0x8250;
  M.Header.Header.CreatorId = SIGNATURE_32 ('P', 'I', 'P', 'A');
  M.Header.Header.CreatorRevision = 1;
  M.Seg0.BaseAddress = DBI_BASE;
  M.Seg0.PciSegmentGroupNumber = 0;
  M.Seg0.StartBusNumber = 0;
  M.Seg0.EndBusNumber = 1;
  S = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&Acpi);
  if (!EFI_ERROR (S)) S = Acpi->InstallAcpiTable (Acpi, &M, sizeof (M), &Key);
  DEBUG ((EFI_D_WARN, "PcieInit: MCFG %r\n", S));
}

EFI_STATUS
EFIAPI
PcieInitDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS S = gBS->LocateProtocol (&gEfiClockProtocolGuid, NULL, (VOID **)&mClock);
  if (EFI_ERROR (S)) {
    DEBUG ((EFI_D_ERROR, "PcieInit: no clock protocol %r\n", S));
    return EFI_SUCCESS;
  }
  PcieInit ();
  if (mSt.Step >= 6) {
    InstallMcfg ();
  }
  SaveStatus ();
  return EFI_SUCCESS;
}
