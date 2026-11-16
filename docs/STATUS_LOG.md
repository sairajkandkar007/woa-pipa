# Status log

## 2026-11-16 (later): SPMI/PMIC root cause found and fixed, PMIC GPIO + power button working

**Root cause:** on Windows' first boot (before any Qualcomm driver was in the store) ACPI devices that are allowed to run without a driver ("raw"), namely SPMI (`QCOM250C`), PMIC core (`QCOM252E`) and PEP0 (`QCOM2519`), were installed with a **NULL driver** (`ConfigFlags=0`, no `Service`). Windows never re-checks them when drivers are added later (unlike devices with no driver at all, `ConfigFlags=0x40`, which retry). So qcspmi/qcpmic never attached, the SPMI and PMIC device interfaces (`{A44DA282…}`, `{5186FCEA…}`, `{9A8FF884…}`) were never published, and PMIC GPIO failed its start (`0xC0000001`, then `0xC000026C`/`0xC000038E` on retries).
How it was found: boot-time ETW AutoLogger (`scripts/windows/pmicgpio_trace.reg`) with Kernel-PnP + qcpmicgpio WPP, WPP messages mapped back to call sites by disassembly (no TMF needed), then offline registry check (`DeviceClasses`, `Enum`).
**Fix:** `RUN_PHASE5_BATCH3c_REINSTALL.bat` sets `ConfigFlags=0x20` (CONFIGFLAG_REINSTALL) on those Enum keys (written with SeRestorePrivilege + REG_OPTION_BACKUP_RESTORE, because Enum keys are SYSTEM-only).

**Result (2 boots later):** SPMI ✅, PMIC core ✅, **PMIC GPIO ✅, side/power button (hidinterrupt) ✅ — power button now turns the display off/on and locks**, PMIC Type-C controller ✅, 3× ADC ✅.
New: USB Type-C port manager (`QCOM257D`, oem15) now gets to start and fails with code 10 / `0xC000009E`. PEP0 still has no driver (none installed yet). Only code-28 (no driver) devices remain otherwise. Snapshot: `logs/2026-11-16_after-reinstall/`.

**Lesson:** after injecting drivers for raw-capable ACPI devices, check `Enum` for keys with no `Service` and `ConfigFlags=0` and flag them for reinstall (PEP0 will need this again when PEP is added).

## 2026-11-16: driver bring-up (current state)

**UEFI:** upstream Mu-Silicium main @ fb4efb3 + `uefi/patches/0001-pipa-fix-boot-ram-manager.patch` + `pipa_k` ACPI (pipa DSDT with Kona IDs, TLMM `_UID` 0→1, built by `acpi/make_pipa_k.py`) + Kona CSRT. Booted with `fastboot boot`; Mass Storage = hold **Volume Down** at the Silicium screen.

**Kona drivers installed offline (DISM), Windows still boots:** SCM, ACPI helper (ABD), SPMI, TLMM GPIO, PMIC Core/Apps/Extension/GPIO, BAM, GPI, GLINK, ADC, RPEN, PMIC Type-C controller, USB TCPM.

**Running on Kona drivers:** SCM, SPMI, TLMM GPIO (`qcgpio`, after the `_UID` change), PMIC Core/Apps/Extension, ACPI bridge, 8× BAM, 3× GPI, GLINK SMEM port, RPEN.

**Open problem:** PMIC GPIO (`ACPI\QCOM2530\1`, service `qcpmgpio` = `qcpmicgpio8250.sys`) fails to load on every boot (Kernel-PnP 219: `0xC000026C` STATUS_DRIVER_UNABLE_TO_LOAD, then `0xC000038E`). Its imports are fine (ntoskrnl, WDFLDR). It is a GpioClx client that opens `\Device\Spmi\SUPERUSER` and reads platform config from the registry. Knock-on: side buttons (`ACPI0011`), PMIC TCC (`QCOM2582`) and 3× ADC (`QCOM2512`) stay in "waiting on dependency" (code 51).
Ideas: missing PMIC registry configuration, KMDF version, SPMI SUPERUSER access, or a PEP dependency.

Device state snapshot: `logs/2026-11-16_device-state/`.

## 2026-11-15: Kona IDs, staged driver injection
- Silicium-ACPI's pipa DSDT **is Qualcomm's SM8250 MTP DSDT** (WOA-Project `acpi_oem_qcom_common/8250`) with pipa edits **and every HID renamed `QCOM25xx`→`QCOM05xx`**, so no Kona BSP driver could bind. The alioth DSDT is from an older BSP (IDs off by 1–2).
- `pipa_k`: rename back (152×, same length, checksum fixed) + Kona SSDT/APIC/CSRT/DBG2/FACP/GTDT/PPTT (no IORT/MCFG yet).
- Raw Qualcomm MTP ACPI → `DRIVER_PNP_WATCHDOG` even without drivers. Don't use it.
- Batches 1a (SCM, ABD), 1b (SPMI, GPIO), 1c (PMIC ×4), 1d (BAM, GPI, GLINK, ADC, RPEN), 2 (PMIC TCC, USB TCPM) all boot.
- **Lesson:** restoring registry hives without their `.LOG1/.LOG2` files broke the registry (Windows had never flushed the hives) → repair loop → reinstall. Snapshots now cover the whole `System32\config` directory, and BCD has `bootstatuspolicy IgnoreAllFailures`.

## 2026-11-15: upstream UEFI regression fixed
- Commit `8c321aab` (2026-05-21) moved `RamManagerDxe` out of pipa's APRIORI → `TzDxe` Out of Resources → black screen. Found by release/commit bisect + DEBUG framebuffer log. Fixed by the patch in `uefi/patches/` (details in `uefi/BUILD.md`).
- Bisect: 2.3 / v3.1 / v3.2 / v3.4 ✅, `a72d7eb4` / `96615491` grey, `8c321aab` / `b0f560bf` / v3.6 / main black → main + patch ✅.

## 2026-11-15: Windows installed
- All 113 partitions + GPTs backed up and MD5-verified (kept offline).
- Install over USB Mass Storage of the whole UFS LUN0: diskpart p34/p35 → DISM → unattend → offline registry → bcdboot. 4096-byte sectors are fine.
- Windows 11 Pro ARM64 22621 boots to the desktop; unattend skips OOBE with auto-logon.
- The XDA TWRP in boot_a boot-loops (pre-existing kernel issue); not needed any more.
