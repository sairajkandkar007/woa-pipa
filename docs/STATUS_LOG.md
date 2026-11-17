# Status log

## 2026-11-17 23:30: PEP plan switched to QualcommPEP port; toolchain works

- Assessment: `docs/QUALCOMMPEP_ASSESSMENT.md` (feasible, better long-term; qcpep patching parked).
- PC toolchain installed: VS 2022 Community 17.14 (C++ ARM64), SDK + WDK 10.0.26100 (+ VS DriverKit component).
- `qpep/RUN_QPEP_BUILD.bat` clones QualcommPEP (sm8845), creates a test cert, builds ARM64 Release: **qcpep.sys built OK**
  (infverif error 1323 on service-key values worked around in the local INF copy).
- Next: create the sm8250 target (CPU perf/idle off, QMP via APCS doorbell, SM8250 RPMh/cmd-db, clocks/ICB for UFS+QUP).

## 2026-11-17 21:30: PEP debugged with the kernel debugger; init completes, device loading still crashes

- Kernel debugging over USB (KDNET-EEM) works on pipa (`scripts/RUN_PHASE9_KD_ON.bat`, WinDbg "Break on connection").
- The PEP boot hang was qcpep writing the LMh registers (0x18350800); 10 further pipa/firmware differences found and
  worked around **in memory** via WinDbg scripts (`debug/windbg/`). Full table: `docs/PEP_FINDINGS.md`.
- New DSDT option `make_pipa_k.py --pkg-fix` (PM8009 removal left wrong package counts) → `Mu-pipa-pepfix-pkgfix.img`.
- qcpep now reaches `INIT done`; it crashes while loading device power resources (0x14E/0x3A on `\_SB.UARD`;
  BUSARB resources fail because no bus-arbiter provider is registered).
- Tablet state unchanged otherwise: flashed UEFI = ufsdepscm; KD is ON in BCD (`RUN_PHASE9_KD_OFF.bat` to disable);
  DSGP deleted from PEP Device Parameters (batch 4f).

## 2026-11-17: PEP root causes found, PEP now starts but boot hangs

- **Why PEP blue-screened the boot (`INACCESSIBLE_BOOT_DEVICE`):** UFS0 has `_DEP` on PEP0. Without a PEP driver PEP0 is raw-started, so UFS starts; with qcpep installed PEP0 failed (code 31) → UFS0 waited forever → no boot disk. Fix: `make_pipa_k.py --ufs-dep-scm` (UFS0 depends on SCM0 instead). With it Windows boots even when PEP fails.
- **Why qcpep failed AddDevice (code 31, `0xC000000E`):** `\_SB.PEP0.PEPH` returns the PEP's own hardware ID string `"ACPI\\VEN_QCOM&DEV_0519"` (missed by the QCOM05→25 rename). qcpep compares it to its device ID. Fix: rename `VEN_QCOM&DEV_05xx` strings too (now always done by `make_pipa_k.py`).
- **First-boot effect:** a PEP installed during a boot loads late (as a normal driver) and cannot register with PoFx; it only works from the next boot (boot-start).
- **Diagnostic WinPE (RAM disk on the ESP, one-time BCD boot):** `RUN_PHASE8_BUILD_DIAG.bat` / `RUN_PHASE8_INSTALL_DIAG.bat`. WIM must have boot index set (`/Bootable`), BCD must use an explicit `partition=`/`[S:]` device on pipa.
- **Current blocker:** with the PEPH fix, PEP0 attaches and **boot hangs at the spinner** (no BSOD). Not fixed by qcpep "safe mode" registry (`pep_safe.reg`: processor management/LPM/LPI/AVS/BCL/BW-mon off, WPP on) nor by removing UFS0 from PEP's config. No trace survives (hang happens before the log file is created).
- Flashed UEFI = `Mu-pipa-konaIDs-ufsdepscm.img` (no PEPH fix → PEP fails quietly, Windows boots). Test UEFIs: `pepfix`, `pepfix-ufsnopep` via `fastboot boot` only.
- Next idea: capture the hang (USB keyboard + CrashOnCtrlScroll → dump → WinDbg on the PC), or bisect PEP's default resources (SRDL: display clocks / MMCX rail / bus votes).

## 2026-11-16 (evening): PEP attempt failed, clean rebuild

- **PEP (`qcpep.wd8250`)** → `INACCESSIBLE_BOOT_DEVICE` (param2 `0xC0000034`): the UFS disk disappears once PEP manages power.
  Likely cause: the MTP DSDT's UFS0 PEP rail votes (LDO10_A 2.904 V, LDO3_C 1.2 V) don't match pipa, whose stock DT has UFS VCC = PM8150 **L17A**, VCCQ = **L6A**, VCCQ2 = S4A, PHY = L5A/L9A. `acpi/make_pipa_k.py --ufs-rails` retargets them (built, **not yet tested with PEP**).
- PEP's INF also drops `PPMSettings-8250Profiles.wd.ppkg` into `Windows\Provisioning\Packages`. The registry undo does not remove it; Windows applied it on a later boot → **`CRITICAL_PROCESS_DIED` on every boot**, even after restoring an older registry snapshot. Recovered by **full reinstall**.
- **New install procedure:** `RUN_PHASE2_INSTALL.bat`, then (before first boot) `RUN_PHASE5_BATCH_BASE.bat` (batches 1a–2 in one go). With drivers present at first boot, SPMI/PMIC are installed properly (no NULL-driver problem): SPMI ✅ PMIC ✅ PMIC GPIO ✅ buttons ✅ ADC ✅ TCC ✅. Only code 28 left.
  Side effect: TLMM GPIO (`QCOM250D`) and Type-C (`QCOM257D`) picked Microsoft `genpass.inf` instead of the Kona INFs on first boot.
- **Rules for the next PEP try:** install without the .ppkg, use the `--ufs-rails` UEFI via `fastboot boot`, and extend undo to remove non-registry leftovers (INF, ppkg).

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

## 2026-11-18 00:30 — QualcommPEP SM8250 port, first compile candidate
- AOP QMP: APCS IPC doorbell 0x17C0000C bit 0 replaces IPCC; built-in MSGRAM fallback (0x0C300000/0x400) when PEP0 has no QMP `_DSM`.
- Clocks: `SystemLibs/clock/8250/src/clock_data.c` regenerated from Linux gcc-sm8250.c (`gen_gcc.py`): 66 GCC clocks/GDSCs, 24 RCG domains all READ_ONLY (no rate programming). Display CRM/CESTA disabled (none on SM8250); GPUCC/TCSR windows removed; dispcc 0x0AF00000; GDSC table: UFS 0x77004, PCIe0 0x6B004, MDSS 0xAF03000.
- CPU perf/idle default OFF on 8250 (no SCMI/CPUCP); BWMON bus DCVS default OFF (targets still 8845).
- New minimal INF `inf/qcpep8250.inx` (HWID ACPI\QCOM2519, no 8845 resource blobs).
- DSDT: `make_pipa_k.py --qpep-crs` replaces 4 unused PEP0 interrupts with Memory32Fixed RSC DRV2 0x18220000/0x10000 + AOP MSGRAM (QualcommPEP needs DRV2 as a PnP memory resource).
- Build: `qpep/RUN_QPEP8250_BUILD.bat` (downloads repo main, builds QcPep8250.sln, test-signs). Not yet compiled.
- Before installing on the tablet: remove the old Kona qcpep (same HWID QCOM2519).

## 2026-11-18 00:20 — QualcommPEP SM8250 first boot
- Boots to desktop. PEP0 `ACPI\QCOM2519` DeviceNodeStarted with service qcpep_8480; PoFx registered.
- Trace: cmd-db 0x80860000 parsed (16 BCMs), RSC DRV2 identity+geometry OK, GPLL0 locked, TLMM pin 52 OK, QMP mailbox mapped.
- Admission denied: probes=2A/3F (missing CMDDB rail, RPMH read, CLOCK sentinel).
  - rails: 8845 cmd-db naming (L10B_E0) -> added SM8250 legacy naming (ldob10/smpa1/bobb1/lnbclka1).
  - clock sentinel: probe list was QUP wrap3 (8845) -> QUP wrap1.
