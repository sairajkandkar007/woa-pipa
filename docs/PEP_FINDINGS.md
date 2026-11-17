# qcpep8250 on pipa: findings from kernel debugging (2026-11-17)

Kernel debugging over USB (KDNET-EEM, `kd_8003_5143.dll`, DBG2 entry for USB0 at 0xA600000) works on pipa:
`scripts/windows/RUN_PHASE9_KD_ON.bat` (BCD via UEFI Mass Storage, port 50000, key 1.2.3.4, busparams 1) and WinDbg
**File → Attach to kernel → Net, "Break on connection"**. WinDbg (new) rejects `-b`, and `-c "$$<..."` on the command line;
paste `$$< ...\pipa_peppatch.txt` at the first `brk` instead. Max 32 breakpoints.

The scripts in `debug/windbg/` patch qcpep8250.sys **in memory only**. Stage 1 waits for qcpep to map LMh
(`MmMapIoSpaceEx(0x18358800)`), stage 2 (`pipa_peppatch2.txt` + `pipa_clk.txt`) applies the patches below.
Offsets are RVAs in qcpep8250.sys from the Kona BSP (`qcpep.wd8250.inf`, PEP "10.5" build 202410300701).

The DSDT used is `make_pipa_k.py --ufs-rails --ufs-dep-scm --pkg-fix` (Mu-pipa-pepfix-pkgfix.img). PEPH fix is default.

| # | Symptom | Cause on pipa | Workaround (in-memory) |
|---|---|---|---|
| 1 | Whole-system freeze at the spinner (CPU stuck, no IPI response) | qcpep writes 0x3A to **LMh** cluster regs 0x18350800 / 0x18358800; the bus write never completes on pipa | skip writers `+0x5b88`, `+0x5b20` (return) |
| 2 | 0x14B SOC_SUBSYSTEM_FAILURE (RSC init) | display RSC 0xAF20000 reports 2 TCS, qcpep's MTP table requires 4 | data `+0x19ff84` = 2 |
| 3 | 0x14E (PPP init, PPRR parse, `C014000F`) | Silicium removed PM8009 entries from PPPP/PRRP/CPXC packages but kept element counts | **DSDT fix** `--pkg-fix` (8 NumElements bytes) |
| 4 | 0x14E (PPP init, `C0000001`) | pipa cmd-db lacks `smpa1 smpa2 smpc2 smpc5 ldoa1 ldoa8 rfclka2 rfclka3` (and all PM8009 "F" rails) | `+0x117918` = `37f80374` (skip missing rail instead of fatal) |
| 5 | Freeze (bus hang) in Clock_DriverInit | CPU-cluster clock LUT read from OSM 0x1832xxxx (`0x18323114`) – locked on retail fw (pipa uses EPSS 0x18591000) | skip `+0xfbc00` (return 0). No CPU DVFS from Windows |
| 6 | PEP start waits forever | GLINK `rpm_requests` channel never opens (qcglink is demand-start → deadlock; pipa AOP probably only speaks QMP) | `+0x144478` returns 5 (open failed) |
| 7 | 0x14B from AOP send | AOP messages (`{class: image, res: load_state, name: modem/adsp/slpi/cdsp on, spss off}`) need the channel | `+0x144b28` drops message, returns 0 (logged). TODO: send via QMP mailbox 0xC300000 |
| 8 | 0x14E/0x37 | LDCV (CPU DCVS) finds no CPU clock domains (because of #5) | `+0x20c180` = `14000050` |
| 9 | 0x14B assert `NULL != hdr` for `\_SB.SDFR` | registry default resource `DSGP` (from INF) defines SDFR | `+0x40244` = `14000009` (ignore DSGP). Offline registry delete of DSGP did not take effect |
| 10 | 0x14B assert `NULL != hdr` (BUSARB, type 6) | no bus-arbiter provider registered (lookup `0x208c80` on table `[0x1c8ee8]` empty) | bp `+0x1eb828` → `+0x1eb858` (skip resource, logged as RES-NULL) |
| 11 | 0x14E/0x3A (UARD clocks) | requested gcc_qupv3_wrap1_s4 frequencies (7.3728/14.7456/29.4912/32 MHz) not in the resource's frequency table | `+0x3d654` → `b +0x3dfbc` drops request; **still crashes at `+0x3de04`** (same check, other path) |

**Status at end of day:** PEP initialisation completes (`INIT done`), device-resource loading crashes at #11
(device `\_SB.UARD`, the debug UART). Next options: remove UARD's PEP config from the DSDT, or understand
the bus-arbiter (ICB) provider registration so BUSARB works, which likely also fixes #11.

Firmware differences learned (retail pipa vs Qualcomm MTP assumptions in qcpep):
- LMh and OSM register blocks (0x1835xxxx, 0x1832xxxx) are not accessible from HLOS (bus hang).
- AOP: no GLINK `rpm_requests`; Android uses QMP over AOSS MSG RAM 0xC300000 (`qcom,qmp-aop`).
- cmd-db VRM table differs (66 entries, listed in this doc above); cmd-db itself is valid at 0x80860000.
- Display RSC has 2 TCS.

All patches still need to be turned into a patched, test-signed `qcpep8250.sys` once PEP reaches the desktop.
