# woa-pipa: Windows 11 ARM64 on Xiaomi Pad 6 (pipa)

Bring-up notes, patches and tooling for running **Windows 11 Pro ARM64** on the
**Xiaomi Pad 6 (pipa, Snapdragon 870 / SM8250-AC)** with
[Project Silicium Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) UEFI and the
[WOA-Project Kona BSP](https://github.com/WOA-Project/windows_silicon_qcom_kona).

> ⚠️ Experimental. This is an active hardware bring-up project. Flashing, repartitioning
> or injecting experimental drivers can make the device unbootable or cause data loss.
> Back up all partitions before making changes.

## Hardware status (2026-11-23)

The table records project bring-up reports. **Windows is not installed on the current Pad setup yet**; treat the reported Windows-side hardware results as historical project notes until they are reproduced after installation.

| Area | State |
|---|---|
| Windows 11 Pro ARM64 install + boot | ⏳ Not installed on the current Pad setup; installation remains pending |
| Mu-Silicium UEFI | ✅ Boots on pipa with the pipa RAM-manager fix |
| UFS storage | ✅ Working |
| Display | ✅ Working through UEFI GOP / current display path |
| Touchscreen | ✅ Working |
| 10-point multitouch | ✅ Working |
| Wi-Fi | ⚠️ Wi-Fi works, but there is currently no internet connectivity |
| Bluetooth | ✅ Working |
| USB host mode | ✅ Working |
| Backlight | ✅ Working |
| GPU acceleration | ❌ Not working |
| Charging | ❌ Not working |

**Wi-Fi note:** The wireless hardware/driver path is working, but internet access is not.
Network association, IP configuration, routing and DNS should be diagnosed separately;
do not interpret the lack of internet as proof that the Wi-Fi device itself is nonfunctional.

**Touch note:** The custom NT36532 SPI/HID implementation supports 10-point multitouch.

**Bluetooth note:** The custom QCA6390/UART implementation is reported working in Windows.

**USB note:** USB host mode is working. This supersedes the older status entry that described
USB host support as partial.

**Still not working:** GPU acceleration and charging.

Details: [Installation guide](docs/INSTALLATION_GUIDE.md) ·
[Status log](docs/STATUS_LOG.md) · PEP debugging:
[PEP findings](docs/PEP_FINDINGS.md) · Plan: [Roadmap](docs/ROADMAP.md) ·
UEFI build: [uefi/BUILD.md](uefi/BUILD.md)

## Key findings
1. **Upstream UEFI regression:** `8c321aab` moved `RamManagerDxe` out of pipa's APRIORI → TrustZone drivers run out of memory → black screen. Fixed by the patch above.
2. **Silicium's pipa DSDT = Qualcomm SM8250 MTP DSDT** with pipa edits and every HID renamed `QCOM25xx`→`QCOM05xx` (Kona BSP expects `QCOM25xx`). [`acpi/make_pipa_k.py`](acpi/make_pipa_k.py) renames them back, also fixes `PEP0.PEPH` (`DEV_0519`→`DEV_2519`, else qcpep code 31), TLMM `_UID`, and optionally: UFS0 `_DEP` PEP0→SCM0 (`--ufs-dep-scm`, else INACCESSIBLE_BOOT_DEVICE when PEP fails), UFS rails (`--ufs-rails`), and 8 package element counts Silicium left wrong after removing PM8009 (`--pkg-fix`). All same-length binary edits + checksum.
3. **NULL-driver installs:** raw-capable ACPI devices installed on first boot without their driver are never re-checked → inject drivers before first boot or set `ConfigFlags=0x20`.
4. **Kernel debugging over USB works** (KDNET-EEM via `kd_8003_5143.dll`, DBG2 has USB0). This is how PEP is being debugged; ETW/boot logging was useless because the hang happened before log files exist.
5. **Retail pipa firmware ≠ Qualcomm MTP** (what qcpep assumes): LMh (0x1835xxxx) and OSM (0x1832xxxx) registers hang the bus; AOP has no GLINK `rpm_requests` (Android uses QMP at 0xC300000); cmd-db lacks some rails; display RSC has 2 TCS; no bus-arbiter (BUSARB/ICB) provider. Details in [`docs/PEP_FINDINGS.md`](docs/PEP_FINDINGS.md).
6. **Never install `PPMSettings-8250Profiles.wd.ppkg`** (from the PEP INF): unrecoverable CRITICAL_PROCESS_DIED. Registry-only rollback after a crashed PEP boot is unreliable → rebuild instead.
7. USB mass storage works fine with the 4096-byte UFS sectors when the whole LUN0 (`/dev/block/sda`) is exported.

## Repository layout

| Path | Contents |
|---|---|
| `drivers/` | All Windows driver source projects and their build tooling |
| `drivers/ktz8866/` | KTZ8866 I²C backlight driver |
| `drivers/nt36532spi/` | NT36532 SPI/HID touchscreen driver |
| `drivers/pipabt/` | QCA6390 Bluetooth driver |
| `drivers/pipapwr/` | QCA6390 WLAN/BT power and UART support driver |
| `drivers/qualcommpep/QualcommPEP/` | QualcommPEP source tree, including the SM8250 pipa port |
| `drivers/qualcommpep/build/` | QualcommPEP build and test-signing scripts |
| `acpi/` | ACPI source tables and the pipa DSDT patch/generation script |
| `uefi/` | UEFI build notes, Mu-Silicium patches and firmware-specific components |
| `scripts/windows/` | Windows installation, diagnostics and device-management scripts |
| `scripts/android/` | Android/recovery mass-storage gadget scripts |
| `docs/` | Project status, roadmap, PEP findings and hardware notes |
| `debug/` | Debugging helpers and WinDbg investigation notes |
| `logs/` | Captured Windows device state and diagnostic output |

The QualcommPEP driver and its build helpers are grouped under `drivers/`; the
ACPI and UEFI directories remain separate because they contain firmware tables,
firmware patches and platform build material rather than Windows driver packages.

## Windows-side tooling (`scripts/windows/`)
Expects: `C:\platform-tools\adb.exe` / `fastboot.exe`, the scripts in a folder `claude\` inside a
workspace that also holds `install_pro.wim` (Win11 ARM64 22621) and `kona_bsp\Drivers` (Kona BSP copy).
UEFI images are not included (they contain proprietary Qualcomm binaries). Build your own (see `uefi/BUILD.md`).

| Script | What it does |
|---|---|
| `RUN_PHASE0B.bat` | READ-ONLY: dump all LUN partition tables, back up + MD5-verify every partition ≤512 MiB (TWRP) |
| `RUN_PHASE2_INSTALL.bat` | Format p34 (ESP) / p35 (Windows) after exact size checks + typed YES, DISM apply, unattend, offline registry, bcdboot, BCD testsigning |
| `RUN_TEST_*.bat` / `phase3_boot.ps1 -Img <img>` | `fastboot boot` a UEFI image (temporary) |
| `RUN_PHASE4B_UEFI_MSC.bat` | With UEFI Mass Storage active: compare LUN0 partitions with the backups, copy Windows logs/Enum hive/`C:\pipa-logs` |
| `RUN_PHASE5_BATCH*.bat` → `phase5_drivers.ps1` | Inject a Kona driver batch offline (DISM) after a full `System32\config` snapshot; installs a logon device-state dump |
| `RUN_PHASE5_UNDO2.bat` | Restore the last registry snapshot |
| `RUN_PHASE6_FLASH_UEFI.bat` / `RUN_PHASE6_RESTORE_BOOT_A.bat` | Flash the UEFI into `boot_a` (MD5, product, unlock, slot and size checks + typed FLASH) / write the original `boot_a` backup back |
| `RUN_PHASE9_KD_ON/OFF.bat` → `phase9_kdnet.ps1` | Enable/disable kernel debugging over USB in the tablet's BCD (UEFI Mass Storage) |
| `RUN_REBUILD_KNOWN_GOOD.bat` | Reinstall Windows + inject the known-good driver set before first boot |
| `RUN_DIAG_*.bat` | Read-only fastboot / disk diagnostics |
| `unattend.xml` | Skips OOBE, local account + auto-logon. **Set your own password** before use (the sample one is a placeholder) |

Disk layout used: sda p34 `esp` 1 GiB FAT32, p35 `win` 224.7 GiB NTFS (Android removed, LUN0 4096-byte sectors).
The size checks in the scripts are hard-coded for that layout. Adjust them for yours.

## Credits
Project Silicium (Mu-Silicium, Silicium-ACPI), WOA-Project (Kona BSP, Qualcomm reference ACPI and
definitions), Project Aloha, the original XDA pipa Windows guide authors, and Microsoft Project Mu.
