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

## 2026-11-18 00:35 — run 2: all probes pass at D0
- `D0 revalidation complete admission=open` (RPMh read of a cmd-db rail works through TCS on DRV2).
- Blocked at `runtime providers unavailable DPM=rejected`: PMIC GPIO provider (8845 pmic 5 gpio 5 IOCTL probe) unavailable.
- Fix: on 8250, PMIC GPIO delegation no longer gates DPM; trace now prints each delegation state.

## 2026-11-18 17:40 — run 3: past the delegation gate, ACPI resource graph empty
- Driver now parses the Kona PEP0 descriptors; every device is "unsupported" -> `graph build failed status=C0000225` (no boot graph).
- DSDT coverage (Kona names vs catalogs): clocks 155 (69 known), PMICVREGVOTE 20 (0 known), NPA 4 (3), ICB masters 23 (11) / slaves 15 (9) / 34 routes, FOOTSWITCH 17 (Kona names without gcc_ prefix).
- Fix: GCC table regenerated from 8845 names + Kona DSDT gcc names (93 clocks, 66/71 Kona gcc covered); catalog gets the SM8250 GCC names; PPP catalog/rail list replaced with the 20 Kona names (+ CXO_BUFFERS_BBCLKn -> lnbclkan).
- Next: ICB (SM8250 masters/slaves/routes from Linux interconnect/qcom/sm8250.c + cmd-db BCMs), FOOTSWITCH names/offsets, NPA /ipa/pc.

## 2026-11-18 18:30 — SM8250 ICB topology + GDSC names
- New generator `SystemLibs/ICB/8250/tools/gen_icb_8250.py`: Linux interconnect/qcom/sm8250.c nodes+BCMs + Kona DSDT BUSARB routes -> `bcm_nodes_gen.c` (27 BCMs, ACV as vector BIT3), `icb_catalog_8250.inc`, `icb_routes_8250.inc` (30/34 Kona routes, 90 hops; BFS paths checked against Linux, e.g. QUP_1->EBI1 = qhm_qup1->qns_a1noc_snoc | qnm_aggre1_noc->qns_gemnoc_sf | qnm_snoc_sf->qns_llcc | llcc_mc->ebi).
  Skipped: CAMNOC_*_UNCOMP (3) and IPA_CORE (no Linux node).
- ICB probe routes -> APPSS->QUP_1, APPSS->UFS_MEM_CFG, UFS_MEM->EBI1, QUP_1->EBI1.
- FOOTSWITCH: Kona names `ufs_phy_gdsc`, `pcie_0_gdsc` adopted as canonical domain names on 8250 (UFS footswitch graph, gdsc tables). Camera/video/GPU/USB GDSCs still unsupported.
- gcc -fsyntax-only clean on the regenerated data files.

## 2026-11-18 18:45 — run 4: resource graph BUILT
- Kona descriptors now accepted: e.g. 8/8, 3/3 devices supported in several methods; UFS F/P catalog compiled; `ufs: required client bound`.
- Still unsupported: JPGE/MPCS/VFE0 (camera clocks), APMD/PEMD/SDMD/CPMX, G0..GL MD (status=1 malformed — to inspect).
- Stops at `npa: client activation rejected created=0` (a resource key not owned before any client creation).
- Fix: rails absent from pipa cmd-db (ldoa1 etc.) are no-op instead of unknown; failing NPA keys are now logged individually.

## 2026-11-18 19:40 — run 6: per-device claim reasons
- I2C5/SPI4/I2C8/IC10/QWLN/BTH0: why=1 — QualcommPEP only admits devices with component F-states from a reviewed whitelist (8845: IC17/18/21 empty-F0, UFS0). Needs real F-state support later.
- UFS0: contract OK, why=1008 kind=CLOCK — D0 P-state set requests gcc_ufs_phy_axi/unipro/ice at 300 MHz; all GCC domains were READ_ONLY.
- Fix: gen_gcc.py now emits Linux freq tables + parent mux for non-DFS UFS/USB/PCIe-aux domains (writable); QUP (DFS) stays read-only.

## 2026-11-19 15:10 — run 8: FIRST PUBLISHED GRAPH (UFS0 admitted) -> boot I/O hang
- `UFS0 ... contract=1 graph=1 clients=1 eligible=1`, `resources: graph published devices=7 clients=74`.
- Then boot hangs ~2.5 min in: worker thread in fastfat!FatNonCachedIo waiting on a disk write (autochk / EFI FAT volume) -> UFS I/O no longer completing after PEP took over UFS0 power.
- Suspects: Kona UFS D3/F-state votes switching off LDO6_A/LDO17_A (Kona MTP rails, may not match pipa UFS supplies), or GDSC/clock action. Collecting !irp / !fxdevice next.

## 2026-11-19 16:00 — UFS F1/F0 trace + hardware state at the hang
- Trace: repeated UFS F1 (ICB 0, 9 clocks off) / F0 (axi/unipro/ice 300 MHz, clocks on, ICB 2.4 GB/s) cycles, every action ok=1. One F1 request took 20 s to be applied.
- At the hang: UFS GDSC on (0xF822F000), clocks on (CBCR 0x221/0x1), rx_symbol_0 CLK_OFF (0x80000001), axi RCG 300 MHz (CFG 0x00100103).
  UFSHCI: IS=0, **IE=0**, **HCS=0x50F -> UPMCRS=5 (fatal power-mode error)**, HCE=3. Host controller lost its link/interrupt state after a PEP F1/F0 cycle -> I/O never completes.
- Mitigation: on 8250 UFS F-states are acknowledged without gating (registry UfsFStateGating=1 to re-enable). D-state control kept.

## 2026-11-19 17:00 — F-state bypass works; UFS D3->D0 cycle is the second killer
- With F-state bypass: UFS F1/F0 acknowledged with no hardware actions (confirmed in trace).
- But early in boot Windows cycled UFS0 D3 -> D0 (~200 ms): D3 applied clocks off, LDO5/9 low-power, **LDO6_A and LDO17_A to 0 V**, ICB 0; D0 restored everything (all ok=1).
  Afterwards UFSHCI HCS=0x507 (UPMCRS=5 fatal), IE=0 -> same I/O hang.
- Fix: on 8250 UFS D-states are acknowledged without hardware actions too (registry UfsDStateControl=1 opts in).
  Net effect: UFS stays as firmware left it, PEP graph still published (UFS admitted), boot should complete.
- Proper fix later: UFS D3/F1 must only happen after storufs puts the link into hibern8 / the Kona storufs <-> PEP handshake (likely needs storufs-side notifications QualcommPEP's 8845 UFS model doesn't expect).

## 2026-11-19 17:05 — MILESTONE: Windows boots to desktop with QualcommPEP (SM8250 build) published
- PEP graph published (7 devices, 74 NPA clients), UFS0 admitted; UFS D/F-states acknowledged without hardware actions.
- Boot completes; no in-memory patching (old Kona qcpep path retired).
- Next: real F-state support so I2C/SPI/QUP, WLAN, BT get admitted; UFS hibern8 handshake; USB.

## 2026-11-19 17:40 — admit Kona QUP serial engines
- I2C5/SPI4/I2C8/IC10 have COMPONENT0 with an empty F0 only (same shape as 8845 IC17/18/21) -> added to the reviewed empty-F0 list on 8250.
- Their D0 rate votes (QUP SE 19.2 MHz) target DFS/firmware-owned RCGs -> executed as plain enables on 8250 (rate left as firmware set it).
- USB: CM_PROB_USED_BY_DEBUGGER on URS0 when KDNET-over-USB is on; with KD off USB host (mouse/keyboard) works.

## 2026-11-19 18:15 — QUP I2C admitted -> CLOCK_WATCHDOG_TIMEOUT (0x101)
- With Kona qci2c8250/qcspi8250 installed: PEP accepted I2C5, I2C8, IC10 and applied D0 (ICB QUP_x paths, wrap_N_m_ahb + SE clock; all ok=1). ~7 s later bugcheck 0x101 (CPU 1 stopped responding).
- Likely: QUP wrapper core/core_2x/s_ahb clocks not voted by the Kona descriptor (APMD/PEMD rejected) and left off by firmware for wrap0 -> SE register access hangs a CPU.
- Fix: on first use of a QUP wrapper, PEP votes its core, core_2x and s_ahb clocks on (kept on). Added those clocks to the GCC table.
- 18:15 re-run: still 0x101. GCC 0x52008 = 0x06BE4040 (wrap0 s_ahb/core/core_2x votes NOT set), wrap0 s_ahb CBCR 0x117008 = 0x80000002 (CLK_OFF). The PEP-client keep-on did not take effect.
  Fix: at D0 peripheral mapping PEP sets the QUP wrapper common-clock vote bits directly (0x52008 bits 6-9,18-21; 0x52010 bits 0-3).
- 18:25: same 0x101 with KD OFF -> real hang, not the debug link. QUP wrapper clocks confirmed on (0x52008 bits 6-9/17-21 set, core CBCRs running; s_ahb CLK_OFF is HW gating).
  Root cause (likely): mainline sm8250-xiaomi-pipa.dts enables only **i2c11** (KTZ8866 backlight) among QUP SEs. The Kona MTP buses I2C5 (wrap0 s4), I2C8 (wrap0 s7), IC10 (wrap1 s1), SPI4 (wrap0 s3) are not used/owned by HLOS on pipa -> touching their SE registers hangs a CPU.
  Action: Kona qci2c/qcspi removed again; next: pipa-specific DSDT for QUP (i2c11 + the touch SPI from the downstream DT), not the Kona MTP set.

## 2026-11-20 12:15 — DSDT --pipa-qup (first pipa-specific bus)
- Also found: Kona DSDT pairs SE addresses with mismatched clocks (I2C5 @0x890000 = Linux wrap2 SE4, but votes gcc_qupv3_wrap0_s4 = the touch SE @0x990000; I2C8 @0x89C000 is not a SE on SM8250).
- `make_pipa_k.py --pipa-qup`: IC10 repurposed in place as **i2c11** (QUP1 SE3 @0xA8C000, GIC 0x184, wrap1_s3 clock, TLMM gpio60/61 = qup11); I2C5/I2C8/SPI4 parked (_HID -> QCOMFFFF). Verified by iasl decompile.
- UEFI image Mu-pipa-qpep8250.img rebuilt with it (md5 cb8f046f); previous image kept as Mu-pipa-qpep8250-noqup.img. Kona qci2c re-enabled (I2C only).
- 12:00 RESULT: boots; `\_SB.IC10` (pipa i2c11) accepted, D0 (ICB QUP_1 paths, wrap1 core/core_2x/s_ahb, m_ahb, wrap1_s3 all ok=1), qci2c **DeviceNodeStarted**, then runtime **D3** (clocks off, ICB 0, all ok=1).
  First full PEP-managed device lifecycle (D0 -> start -> D3) on pipa with no hacks.

## 2026-11-20 — pipa i2c15 added to DSDT
- `make_pipa_k.py --pipa-i2c15`: Kona I2C5 repurposed as pipa i2c15 (QUP2 SE1 @0x884000, GSIV 0x267, gcc_qupv3_wrap2_s1_clk + wrap_2_m_ahb, ICB QUP_2, gpio44/45). Devices: fsa4480, smb1390 x2, hl6111r.
- UEFI: Mu-pipa-qpep8250.img md5 acf92bdf…; previous kept as Mu-pipa-qpep8250-i2c11only.img.
- TESTED OK (12:06): PEP accepted \_SB.I2C5 (i2c15), D0 votes + runtime D3 all ok=1; qci2c DeviceNodeStarted on ACPI\QCOM2511\5 and \a. Both pipa I2C buses up, no 0x101.

## 2026-11-20 — KTZ8866 backlight, stage 1
- DSDT `--pipa-bklt`: appended `\_SB.BKLT` (_HID PIPA8866, I2C 0x11 @400 kHz on \_SB.IC10 = i2c11). Also CFSA (fsa4480) I2C address 0x43 -> 0x42 to match the pipa DTB.
- `drivers/ktz8866`: minimal KMDF SPB client. At D0 reads regs 0x01/02/03/04/05/08/09/0F, logs via DbgPrint (`ktz8866:`) and to the device HW key (KtzReadOkMask, KtzDeviceId, KtzBlEn, KtzBrightnessNow). Doesn't re-init the chip; writes brightness only if HW key DWORD `Brightness` exists. Built and installed by the qpep8250 build/phase10 scripts.
- UEFI md5 f061dbf1…; previous = Mu-pipa-qpep8250-i2c15.img.
- TESTED OK (12:26): first real I2C transfers on pipa. KTZ8866 @i2c11:0x11 all 8 reads ok: ID=0x21, BL_CFG1=0xB3, BL_CFG2=0xCD, BRT=1009/2047, BL_EN=0x5F (enabled, 5 sinks), LCD_BIAS=0x9F, FLAG=0 (no faults).
- TESTED OK (12:34): first I2C WRITE on pipa - Brightness=300 written to KTZ8866 (st=0), screen visibly dimmed. Full read+write I2C path proven.

## 2026-11-20 — touch SPI bus
- ktz8866 INF Brightness back to 1009 (bootloader level).
- DSDT `--pipa-spi4`: Kona SPI4 (QUP2 SE17 @0x88C000) -> pipa touch SPI QUP0 SE4 @0x990000, GSIV 0x27D, gcc_qupv3_wrap0_s4_clk, TLMM gpio8-11 func 1; _HID restored QCOM250F. qcspi enabled in qpep_extra_drivers.txt (PC).
- Caveat: Kona ADSP AUDD child still points at \_SB.SPI4 (no ADSP driver running, so inert).
- SPI4 not enumerated/started (held by _DEP on QGP0 GPI-DMA + MMU0, no drivers). _DEP rewritten to PEP0 only.
- SPI4 + qcspi => hardware reset (no bugcheck). Without qcspi: boots, PEP publishes \_SB.SPI4 fine. qcspi8250.inf sets Instance\4 OpMode=GPI (2) while every qci2c instance is FIFO; Kona QGP0 is at 0x904000 (Windows GPI EE) but pipa's TZ/Android uses EE offset 0x1000 -> likely XPU violation on GPI access. Test: local modified INF (OpMode=FIFO), not committed (BSP file).
- 13:34 QUP diag: wrap0_s4 branch was off (bit14 of 0x52008 unvoted), runs once voted; DFS perf table programmed (identical to wrap1_s3); SE4 GENI fw_rev=0x0101 (SPI firmware loaded). So clock/firmware OK when voted.
- qcpep: live DbgPrint mirror of ICB/CLK/TLMM actions, D-state, accepted and perf-state requests, to see the last action before the qcspi hang.
- 13:45: live trace shows PEP never accepted/powered \_SB.SPI4 before the hang -> qcspi hit SE4 with its clock off. Fix attempt: PEP votes wrap0_s4 always-on at init (bring-up), and prepare rejections now mirrored to KD.
- 14:12: boot fixed (BCD recoveryenabled had come back to Yes; set No, bootmgr timeout 0). SPI4 still resets: no SPI4 prepare at all before hang, even with wrap0_s4 voted at init. Test: vote ALL QUP SE clocks at PEP init (qcspi may probe every SE listed in its Qup registry: 8+6+6).
- 14:30: ROOT CAUSE of SPI reset: Kona qcspi8250.sys maps hard-coded PA 0x03900000 len 0x300000 in EvtDeviceAdd (before PEP prepare) - SM8150-style TLMM address, unpowered LPASS space on SM8250 -> CPU stall + watchdog. qcspi is unusable on pipa.
- New drivers/nt36532spi: own GENI SPI (FIFO, polled, mode 0, 8-bit, ~1 MHz, XO DFS level 0) binding ACPI\QCOM250F; stage 1 = SE init + NT36532 reads (page 0xFF select, addr&0x7F, 1 dummy) at 0x3F004/0x3F000/0x21C00, logged "nt365:". Built/installed by the qpep8250 build + phase10 loop.
- 15:08: nt36532spi: loopback MATCH (controller OK), PEP accepted SPI4 + pins, but NT36532 returns all 0xFF (also after 0x69 bootloader reset). Next: pin levels + gpio33 (DT pmx_ts_reset) pulse. Note: qcspi's 0x03900000 = SM8150 TLMM north tile; SM8250 north is 0x0F900000.
- 15:25 reference: github.com/pipa-mainline/linux (working touch on Linux). pipa touch = spi4, cs-gpios tlmm 11, 19.2 MHz, irq gpio39, swrst-n8 0x3F0FE, firmware novatek/nt36532_{csot,tianma}.bin (flash-less, downloaded by host after the panel driver's prepare: "make sure display reset(RESX) sequence and mipi dsi cmds sent before this"). Driver reads with 1 dummy byte (memcpy rbuf+2). NT36532 uses new TDDI map: trim 0x1FB104, swrst_sif 0x1FB43E, event buf 0x125800 (cascade). Next test: reads at the new-map addresses.
- 15:38 TOUCH CHIP ANSWERS over our SPI: @0x1FB104 (after dummy) = 20 00 00 32 65 03 -> trim id 0x036532 = NT36532. Event buffer 0x125800 returns live (non-FF) data. Old-map addresses read 0xFF (do not exist). Full own-driver SPI path proven. Next: flash-less firmware download (nvt_download_firmware_hw_crc, cascade, nt36532_{csot,tianma}.bin from user's Android/Linux), then event read + HID.
- nt36532spi stage 2: streaming polled GENI transfers (any length, TX-only/full-duplex, 9.6 MHz), port of pipa-mainline nt36xxx_fw_update.c hw-CRC + cascade download (bootloader reset 0x69@0x3F0FE, ILM/DLM CRC banks, TX auto copy 0x56@0x1FC925, SRAM writes, fw crc enable 0xAE, boot ready 0x1FB50D, reset state >=0xA0), FW info, 20 ms event polling with finger/pen logging. Firmware read from \SystemRoot\System32\drivers\nt36532_{csot,tianma}.bin (phase10 copies claude\touch_fw; files from github.com/pipa-mainline/xiaomi-pipa-firmware, not in repo).
- 16:07 MILESTONE: NT36532 firmware downloaded by our own driver and touch reports arrive (1 and 2 fingers confirmed by user in WinDbg). Also fixed qpep_cycle retry: `fastboot reboot bootloader` (plain reboot booted the flashed UEFI/Kona DSDT).
- nt36532spi stage 3: HID multi-touch (parallel mode, 10 contacts, native portrait coords 0..17999 x 0..28799, 14.8x23.7 cm) as lower filter under mshidkmdf; INF Class=HIDClass. Polling 10 ms while touching / 20 ms idle.
