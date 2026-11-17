# Roadmap: Windows 11 ARM64 on Xiaomi Pad 6 (pipa)
_Last updated 2026-11-17._

| Phase | State |
|---|---|
| 0: Backups / ground truth | ✅ 113 partitions + GPTs MD5-verified (kept offline, not in this repo) |
| 1: UEFI from source | ✅ Upstream Mu-Silicium main (fb4efb3) + RAM-manager patch; flashed to `boot_a` (ufsdepscm DSDT) |
| 2: Windows install + first boot | ✅ Win11 Pro 22621, unattended; `RUN_REBUILD_KNOWN_GOOD.bat` reinstalls + known-good drivers |
| 3a: ACPI for drivers | ✅ `pipa_k` DSDT via `acpi/make_pipa_k.py` (Kona IDs, PEPH, UFS `_DEP`, package counts) |
| 3b: Kona BSP base drivers | ✅ SCM, ACPI, SPMI, PMIC core/apps/ext/GPIO, BAM, GPI, GLINK, VCM, RPEN, PMIC TCC, TCPM |
| 3c: **PEP** | 🔧 in progress, kernel debugger; see `docs/PEP_FINDINGS.md` |
| 4+: buses, peripherals | ⏳ blocked on PEP |

## Next steps
1. **PEP:** remove/adjust `\_SB.UARD` PEP config (or make BUSARB work: find/emulate the bus-arbiter/ICB provider), continue until boot completes with qcpep running.
2. Turn the in-memory patches into a patched, test-signed `qcpep8250.sys` + DSDT; flash; boot without the debugger.
3. AOP messages via QMP mailbox (0xC300000) instead of GLINK `rpm_requests` (needed before DSPs). CPU DVFS via EPSS (0x18591000) instead of OSM.
4. TLMM GPIO + Type-C off `genpass`; admin rights on the tablet install.
5. I2C/SPI/UART → SMMU (+IORT) → PIL + ADSP/CDSP → PMIC GLINK/UCSI/battery & charging.
6. Touch: Novatek NT36532 on QUP SE4 SPI (0x990000, IRQ TLMM GPIO 39, per UniXNU-pipa; verify against stock DT).
7. Wi-Fi/BT (QCA6390), GPU (Adreno 650), audio. Upstream the RAM fix and DSDT fixes.

## Risks
| Risk | Mitigation |
|---|---|
| Windows "Initialize disk"/format prompts for the tablet disk or `persist` | Never accept them. Scripts check exact disk/partition sizes. |
| Bad UEFI/driver | Test UEFIs with `fastboot boot`. Small driver batches with a registry snapshot. Crashed PEP boots: rebuild (`RUN_REBUILD_KNOWN_GOOD.bat`), not registry undo. |
| PEP `.ppkg` | Never install it (CRITICAL_PROCESS_DIED loop). |
| qcpep touching locked registers (LMh/OSM) | Bus hang → hard reset only (hold power ~8 s). |
| Windows 24H2+ | Stay on 22621/23H2. |
