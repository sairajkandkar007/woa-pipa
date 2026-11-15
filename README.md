# woa-pipa: Windows 11 ARM64 on Xiaomi Pad 6 (pipa)

Bring-up notes, patches and tooling for running **Windows 11 Pro ARM64** on the
**Xiaomi Pad 6 (pipa, Snapdragon 870 / SM8250-AC)** with
[Project Silicium Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) UEFI and the
[WOA-Project Kona BSP](https://github.com/WOA-Project/windows_silicon_qcom_kona).

> ⚠️ Experimental. Windows on pipa currently has **no touch, no Wi-Fi, no charging, no GPU**.
> Everything here can brick your device if misused. Back up all partitions first.

## Status (2026-11-16)
- ✅ Windows 11 Pro 22621 installs and boots to the desktop (unattended; no input devices needed)
- ✅ Current upstream Mu-Silicium boots again on pipa with [`uefi/patches/0001-pipa-fix-boot-ram-manager.patch`](uefi/patches/0001-pipa-fix-boot-ram-manager.patch) (upstream regression since 2026-05-21)
- ✅ ACPI with Kona BSP hardware IDs ([`acpi/make_pipa_k.py`](acpi/make_pipa_k.py)): 15 Kona drivers installed, 20 devices running (SCM, SPMI, TLMM GPIO, PMIC core/apps/ext, BAM, GPI, GLINK, RPEN…)
- 🟡 PMIC GPIO driver fails to load → side buttons / Type-C controller / ADC blocked
- ⏳ PEP, I2C/SPI, SMMU, DSP subsystems, battery, USB host, touch, Wi-Fi, GPU

Details: [`docs/STATUS_LOG.md`](docs/STATUS_LOG.md) · Plan: [`docs/ROADMAP.md`](docs/ROADMAP.md) · UEFI build: [`uefi/BUILD.md`](uefi/BUILD.md)

## Key findings
1. **Upstream UEFI regression:** `8c321aab` moved `RamManagerDxe` out of pipa's APRIORI → TrustZone drivers run out of memory → black screen. Fixed by the patch above.
2. **pipa DSDT = Qualcomm SM8250 MTP DSDT** with pipa edits and every HID renamed `QCOM25xx`→`QCOM05xx`, so no Kona driver could bind. `make_pipa_k.py` renames them back (same-length binary patch + checksum) and bumps the TLMM GPIO `_UID` so Windows reinstalls it with `qcgpio`.
3. **USB mass storage works fine with the 4096-byte UFS sectors** when the whole LUN0 (`/dev/block/sda`) is exported (a previous "512-byte sector" diagnosis was wrong).
4. Raw Qualcomm MTP ACPI causes `DRIVER_PNP_WATCHDOG` without the full driver stack; use `pipa_k`.

## Repository layout
| Path | Contents |
|---|---|
| `uefi/patches/` | Mu-Silicium pipaPkg fix (RAM manager / memory map) |
| `uefi/BUILD.md` | How to build the UEFI, root-cause and bisect notes |
| `uefi/AutoMassStorageDxe/` | Diagnostic DXE driver (2.3-era Mass Storage at ReadyToBoot) |
| `acpi/` | `make_pipa_k.py` + `pipa_k/AcpiTables.inf` (Kona-ID ACPI set) |
| `scripts/windows/` | PowerShell/batch tooling run on the Windows PC (see below) |
| `scripts/android/` | TWRP mass-storage gadget scripts (whole `sda`) |
| `logs/` | Device state from Windows (`pnputil` problem/all devices) |
| `docs/` | Status log and roadmap |

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
| `RUN_DIAG_*.bat` | Read-only fastboot / disk diagnostics |
| `unattend.xml` | Skips OOBE; local admin `pipa` (password `pipa`, change it), auto-logon |

Disk layout used: sda p34 `esp` 1 GiB FAT32, p35 `win` 224.7 GiB NTFS (Android removed, LUN0 4096-byte sectors).
The size checks in the scripts are hard-coded for that layout. Adjust them for yours.

## Credits
Project Silicium (Mu-Silicium, Silicium-ACPI), WOA-Project (Kona BSP, Qualcomm reference ACPI and
definitions), Project Aloha, the original XDA pipa Windows guide authors, and Microsoft Project Mu.
