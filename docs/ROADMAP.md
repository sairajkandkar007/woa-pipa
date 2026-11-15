# Roadmap: Windows 11 ARM64 on Xiaomi Pad 6 (pipa)
_Last updated 2026-11-16._

| Phase | State |
|---|---|
| 0: Backups / ground truth | ✅ 113 partitions + GPTs MD5-verified (kept offline, not in this repo) |
| 1: UEFI from source | ✅ Upstream Mu-Silicium main (fb4efb3) + `uefi/patches/0001-pipa-fix-boot-ram-manager.patch` |
| 2: Windows install + first boot | ✅ Win11 Pro 22621, unattended (no touch/USB-host needed) |
| 3a: ACPI for drivers | ✅ `pipa_k` DSDT (pipa DSDT with Kona HIDs, see `acpi/`) |
| 3b: Kona BSP driver bring-up | 🟡 15 drivers installed, 20 devices running (see STATUS_LOG) |
| 4: Polish / upstream | ⏳ |

## Next steps
1. **PMIC GPIO** (`qcpmicgpio8250`, `ACPI\QCOM2530`) fails to load (`0xC000026C`). It blocks side buttons, the PMIC Type-C controller and the ADCs.
2. HALs (timer/watchdog) → **PEP** (+TSENS) → I2C/SPI/UART → SMMU (+IORT).
3. PIL + ADSP/CDSP firmware → PMIC GLINK / UCSI / battery & charging.
4. Touch: Novatek NT36532 on QUP SE4 SPI (port the nabu `nt36xxx` driver).
5. Wi-Fi/BT (QCA6390), GPU (Adreno 650), audio.
6. Flash the UEFI to `boot_a` once stable. Upstream the RAM fix (Mu-Silicium) and the pipa_k DSDT (Silicium-ACPI).

## Risks
| Risk | Mitigation |
|---|---|
| Windows "Initialize disk"/format prompts for the tablet disk or `persist` | Never accept them. Scripts check exact disk/partition sizes. |
| Bad UEFI/driver | Always `fastboot boot` (temporary). Small driver batches with a full registry snapshot (`RUN_PHASE5_UNDO2`). Full reset = `RUN_PHASE2_INSTALL` (~20 min). |
| PMIC/charger misconfiguration | Bring up charging last, review PMIC nodes. |
| Windows 24H2+ | Stay on 22621/23H2. |
