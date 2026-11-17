# Assessment: porting NikkaGames/QualcommPEP to SM8250 (pipa)

_2026-11-17. Repo: https://github.com/NikkaGames/QualcommPEP (MIT), branch `sm8845` (active), `sm8250` (empty, reserved)._

## What it is
Open-source Windows ARM64 PEP (~136k lines C, KMDF, WPP/IFR tracing). On SM8845 it reports desktop, CPU DVFS,
CPU idle, screen off/on, SPI touch, BT UART, UFS, ADSP/CDSP start, and clock/GPIO/regulator/interconnect control.

| Part | Lines | Port effort |
|---|---|---|
| `Framework/PEP/src` (PoFx/PEP callbacks, ACPI parsing, idle/LPI, DCVS, D/F/P states) | ~74.5k | mostly reusable; 10 files mention 8845 |
| `Framework/PEP/8845` (resource graph, HW backends, footswitch/GDSC, QMP, IRQ, UFS bridge) | ~29.8k | re-target; much is SoC-agnostic plumbing in a SoC folder |
| `SystemLibs/*` (CmdDb, NPA, ARC, PPP, ICB, RPMh, clock, TLMM, Tsens, SCMI, GLINK) | ~20k | target subdirs need SM8250 data |

## Compatibility with what we have
- **ACPI format:** same Qualcomm PEP conventions as qcpep (`DEVICE/COMPONENT/FSTATE/PSTATE/CLOCK/BUSARB/NPARESOURCE/FOOTSWITCH/PMICVREGVOTE/TLMMGPIO`, methods `STND`, `BSMD`, `BPMD`, `G0MD`, `PREL`). Our pipa DSDT already has STND, BSMD, BPMD, G0MD, PGDS, PPPM, PPRR. ✅
- **AOP:** has a QMP mailbox transport (our biggest qcpep gap). On SM8845 the doorbell is IPCC via a companion driver; on SM8250 it is an APCS IPC register write (Linux `qcom_aoss`: mailbox 0xC300000, doorbell 0x17C0000C bit 0). Needs a small transport change + a `_DSM` (GUID 61e00e5b-…) in the PEP node. 🟡
- **cmd-db / RPMh:** generic libraries. SM8250: apps RSC DRV2 at 0x18220000, TCS offset 0xD00, tcs-config from stock DT; cmd-db already dumped (66 VRM, 37 BCM, ARC). Needs SM8250 TCS/bank layout and RSC version. 🟡
- **Clocks:** SM8845 data is from Qualcomm's clock BSP. SM8250 needs gcc/dispcc/camcc/videocc/gpucc tables (Linux `gcc-sm8250.c` etc., or the tables inside qcpep8250.sys). Only clocks referenced by the DSDT are required. 🔴 largest data task
- **Interconnect (BUSARB):** ICB routes/BCMs for SM8250 (Linux `interconnect/qcom/sm8250.c` + our cmd-db BCM list). 🔴
- **CPU perf:** SM8845 uses SCMI; SM8250 uses EPSS cpufreq-hw (0x18591000/2000/3000). New backend, but can start **disabled** (`CpuPerformanceEnable=0`). 🟢 deferrable
- **CPU idle:** PSCI/ACPI LPI. Can start disabled (`CpuIdleEnable=0`). 🟢 deferrable
- **Windows APIs:** `ExAllocatePool2`, PoFx/PEP v3 notifications: available on 22621. TargetVersion Windows10, KMDF 1.x. ✅
- **No LMh/OSM accesses, no GLINK `rpm_requests` dependency** to work around. ✅

## Verdict
**Feasible and the better long-term route**, but a multi-session port, not a drop-in:
source + WPP instead of binary patching; firmware differences (QMP, cmd-db, RSC, EPSS) become code/table changes;
result is reusable by every SM8250 device. Risk: the SoC tables (clocks, ICB) are substantial; licensing of data
imported from Linux (GPL) must be handled (facts/tables, attribution) before upstreaming to the MIT project.

## Plan
1. **Toolchain** on the PC: VS 2022 (ARM64 C++), Windows SDK + WDK 26100. Build `sm8845` unmodified to prove the setup; test-sign with our own cert.
2. **sm8250 skeleton:** copy target dirs → `8250`; CPU perf + idle off; QMP over APCS doorbell; RPMh/cmd-db for SM8250; footswitch/GDSC + clocks only for UFS, QUP (I2C/SPI/UART), USB first.
3. Boot with the kernel debugger (existing KDNET setup) + WPP; extend clock/ICB tables device by device (touch = QUP SE4 SPI next).
4. Later: EPSS CPU DVFS, LPI idle, DSPs, GPU/display.

qcpep8250 work (docs/PEP_FINDINGS.md) is parked as fallback.
