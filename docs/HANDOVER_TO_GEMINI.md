# Handover: Windows 11 on Xiaomi Pad 6 (pipa, SM8250) — state as of 2026-11-21 12:50 IST

Repo: github.com/nishant713/woa-pipa (source of truth for code). PC work folder:
`C:\Users\nisha\.gemini\antigravity\scratch\win11-pipa\` (`claude\` = scripts/images/logs, `qpep\` = build).
Detailed history: `docs/STATUS_LOG.md`. Read it before changing anything.

## 1. What works now
| Area | How |
|---|---|
| Boot to desktop | Mu-Silicium UEFI (our build) + our QualcommPEP SM8250 port (`pep/`) |
| UFS, I2C11, I2C15 | Kona `qci2c8250` + DSDT patches (`acpi/make_pipa_k.py`) |
| Backlight | own `drivers/ktz8866` (brightness fixed from INF, value 1009) |
| Touch | own `drivers/nt36532spi` (own GENI SPI on QUP0 SE4, NT36532 fw download, HID under mshidkmdf, polled) |
| Bluetooth | own `drivers/pipapwr`: powers QCA6390, own GENI UART on QUP0 SE6 at 3 Mbaud, downloads htbtfw20.tlv/htnv20.bin, child PDO `PIPA\QCA6390_UART` → inbox BthMini. Mouse works. |

## 2. Test loop (do not change without reason)
1. `claude\RUN_QPEP_CYCLE.bat`: downloads repo zip from GitHub → builds PEP + drivers (`qpep\qpep8250_build.ps1`) → tablet in UEFI Mass Storage → `claude\phase10_qpep.ps1` installs drivers offline → fastboot boots `claude\Mu-pipa-qpep8250.img` → WinDbg over KDNET (log in `claude\logs\windbg_*.log`).
   **Code must be pushed to GitHub first** (the build pulls the zip).
2. Only the UEFI changed: `claude\RUN_TEST_QPEP8250.bat`, or `claude\RUN_TEST_IMG.bat <image>` for any image.
3. UEFI is built on Linux (Mu-Silicium tree, `python3 build_uefi.py -d pipa`, `-r DEBUG` prints the DXE log on the tablet screen). DSDT: `acpi/make_pipa_k.py q.aml out.aml --pipa-qup --pipa-i2c15 --pipa-spi4 --pipa-bt --pipa-bklt --pipa-wpwr --pipa-btpwr --pipa-bt-own [--pipa-pcie]`, the output goes into `Silicium-ACPI/Platforms/Xiaomi/pipa_k/DSDT.aml`. Our UEFI-side changes are in `uefi/` (PcieInitDxe + patches 0001-0004).
4. After every image copy, check the md5.

## 3. Images in `claude\`
- `Mu-pipa-qpep8250.img` = **current good image** (md5 cfea0bd0: BT/touch/backlight, no PCIe). The cycle uses it.
- `Mu-pipa-qpep8250-bt.img` = same, backup.
- `Mu-pipa-pcie-dbg.img` (44100ebb) = DEBUG build with PcieInitDxe + memory-map fix + DSDT `--pipa-pcie`. Boots to Windows.
- `Mu-pipa-pcie-s*.img`, `-d.img` = old bisect images (s* hang: they predate the memory-map fix). Ignore or delete.

## 4. Wi-Fi: exact current situation
Hardware: QCA6390 Wi-Fi on PCIe0 (17cb:1101). WLAN_EN gpio20 / BT_EN gpio21 are driven by pipapwr.
The Kona BSP `qcwlan8250` is **not usable**: it is for an integrated WLAN (`\_SB.AMSS.QWLN`), not PCIe QCA6390.
Needed driver: Qualcomm "QCA639x Wi-Fi 6 DBS Network Adapter" **ARM64**, `PCI\VEN_17CB&DEV_1101`, from the Microsoft Update Catalog. The user was asked to put it in `claude\wlan_drv\`. Local only, never commit.

Done so far (in `Mu-pipa-pcie-dbg.img`):
- `uefi/PcieInitDxe` (in the Mu tree at `Platforms/Xiaomi/pipaPkg/Drivers/PcieInitDxe`, added to pipa.dsc/fdf) runs these steps:
  1. pins: PERST gpio79, CLKREQ gpio80, WAKE gpio81;
  2. GDSC and clocks, via ClockDxe and directly in GCC;
  3. PARF init;
  4. QMP PHY tables from Linux (`PhyTable.h`, generated);
  5. DWC RC setup and iATU (CFG shift mode, window 0x60100000 → bus 1; MEM 0x60200000-0x61FEFFFF);
  6. PERST release, LTSSM, link wait;
  7. install MCFG (segment 0, buses 0-1) **only if the link came up**.

  `PCIE_STOP_AFTER` is a compile-time bisect switch.
- Memory map (patch 0004): added `0x01C00000` (64 KB) and `0x60000000` (32 MB). **The earlier "no logo" hangs were a translation fault on PARF because these ranges were missing.**
- DSDT `--pipa-pcie`: PCI0 `_DEP` QPPX → PEP0, and the Kona PCI0 PEP TLMM gpio36 (= pipa display DVDD!) re-pointed to gpio80.

Last measurement, under Windows with the dbg image (`!dd [uc] ...`):
- PHY `0x1c06814 = 0x84` (bit 6 clear → **PHY is up**).
- DBI `0x60000xxx = FFFFFFFF`, bus-1 `0x60100000 = FFFFFFFF`.
- `ACPI\PNP0A08\0` started under pci, `!pcitree` shows no devices.

**Open question:** did the link come up in UEFI, with Windows' PEP switching PCIe0 off afterwards? Or did the controller never respond? The PCI0 PEP descriptor has D3 entries that switch off pcie_0_gdsc and the clocks.

**Next steps:**
1. Boot `Mu-pipa-pcie-dbg.img` and photograph the `PcieInit:` lines (step N done / link / bus1 id). Add a few seconds of `gBS->Stall` after the summary if they scroll too fast.
2. If the link was up in UEFI: stop the PEP from managing PCI0. For example, rename `\_SB.PCI0` in its 3 PEP descriptors (like `--ufs-nopep` does for UFS0), or remove the D3 entries.
3. If not up: check `mSt` (LTSSM in DBI 0x728, link status at the PCIe cap + 0x12). Then compare with Linux `pcie-qcom.c` (`qcom_pcie_init_2_7_0` / `ops_1_9_0`) and `pcie-designware-host.c`. Check that the L9A (PHY PLL 1.2 V) and L5A (0.88 V) rails are on.
4. Then: SMMU (PCIe SID 0x1c00 on apps_smmu) and MSI vs INTx (`_PRT` uses GSIV 0xB5-0xB8) may block the WLAN driver. Install the QCA639x ARM64 driver via phase10 (add it like the touch firmware/extra drivers).

## 5. Bluetooth notes
- 12:50: pushed a lag fix that answers the controller's IBS WAKE_IND (0xFD) with WAKE_ACK (0xFC) straight from the RX DPC; TX is now under a spinlock. **Not yet tested.** It needs a full cycle.
- Known issue: connecting a phone once froze BT and touch. A SCO-packet parser fix is pushed, also untested with a phone.
- Firmware: `claude\bt_fw\htbtfw20.tlv`, `htnv20.bin` (linux-firmware, local only), copied to `System32\drivers` by phase10.
- BD_ADDR is generated once and stored in the device registry (`BdAddrLo/Hi`).

## 5b. Bluetooth mouse lag: findings (13:30-14:35)
- Counters log (every ~2 s, in `RxPoll()`) shows **no ACL packets reach the host at all** for long stretches while the
  mouse moves on the surface. Then a burst arrives when the mouse is lifted. Our side is clean: parked 0, drop 0,
  rxfifo 0, IBS wake timeouts 0. So the controller is not receiving or forwarding the mouse's packets. It is not a host-driver queueing bug.
- Already tried, no change: immediate IBS WAKE_ACK, keeping the controller awake, CTS-ignored IBS bytes,
  WLAN_EN (gpio20) low via KD (coex ruled out).
- Next hypotheses:
  1. **L16A (QCA6390 `vddasd`, 3.0 V: antenna switch/diversity) is probably never on.** Its only vote was in the parked BTH0 PEP descriptor, and that one was rejected anyway ("graph-claim", duplicate BTH0 entries in WBRX/WBRC). Weak RF would explain the lost packets. Try voting L16A (e.g. give WPWR a PEP descriptor with LDO16_A D0 vote, or vote it from UEFI).
  2. Check whether this mouse also lags on another host (its own power saving).
  3. Compare NVM tweaks with Linux: Linux sets IBS (bit 0x80) and deep sleep in NVM. We clear deep sleep. Try leaving the NVM untouched except the baud byte.

## 6. DO NOT
- Never format/flash persist or persistbak. Never flash UEFI images: always `fastboot boot`.
- Never commit proprietary files to GitHub: touch fw, BT fw, the WLAN driver, Kona BSP files, modified Qualcomm INFs, Windows images, partition backups.
- Don't retry disabling UAC.
- Don't use the Kona `qcspi8250` (maps the SM8150 TLMM address, hangs) or the Kona `qcuart`/`qcbtfmuart` for BT (`UR18`/`BTH0` are parked on purpose; UR18's IRQ is back at 0x269 so WPWR owns 0x27F).
- Don't let any Kona PEP descriptor drive gpio36 (pipa display power).
- phase10 edits the SYSTEM hive offline. It must end with "SYSTEM hive unloaded" before you reboot the tablet. If the hive is corrupt (0xc000014c), run `RUN_FIX_HIVE.bat`, which restores the snapshot.
- If boot sits at the logo with no spinner: run `RUN_FIX_BOOTSTATUS.bat` (BCD recovery off).
- Fastboot "Load Error": `fastboot set_active a`, `fastboot reboot bootloader`, retry.
- KD physical reads of device registers need `!dd [uc] <addr>`.
- Commits end with the trailer lines the user wants (see earlier commits).

## 7. Later work (user's order)
1. Wi-Fi.
2. Backlight off when the screen goes off: in ktz8866 via `PoRegisterPowerSettingCallback(GUID_CONSOLE_DISPLAY_STATE)`; plus a brightness tray app.
3. Battery level.
4. Suspend.
5. GPU.
