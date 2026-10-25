# ACPI upstream comparison and build integration status

Audit date: 2026-10-25. This is a source/layout audit, not proof that firmware boots or that Windows drivers work.

## Device-specific source

- Authored pipa DSDT: `acpi/pipa/DSDT.asl`.
- The DSDT now exposes UFS as an experimental device using the audited host MMIO range (`0x01D84000`, length `0x3000`), ICE range (`0x01D90000`, length `0x8000`), and candidate GSI `0x129`.
- The HID/Windows driver contract, final ACPI/GIC interrupt mapping, and clock/reset/PHY/power sequencing remain unresolved. Do not call UFS operational or flash this as a working storage implementation.
- `third_party/mu-qcom-pipa-snapshot/` is reference material, not the canonical source tree for builds.
- `third_party/upstream-acpi-reference/Mu-Qcom/` contains verbatim upstream ACPI disassembly listings and an ACPI include reference. The `.dsl` files for APIC/FACP/GTDT are decoded table listings, not normal ASL source; do not feed them to `iasl -tc`.

## SoC-level tables and upstream source

The Mu-Qcom fork, [`sairajkandkar007/Mu-Qcom`](https://github.com/sairajkandkar007/Mu-Qcom), contains binary ACPI tables in its SM8250 package:

- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/APIC.aml`
- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/FACP.aml`
- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/GTDT.aml`

The pipa package's `Include/ACPI.inc` includes APIC, DSDT, FACP, and GTDT. The build workflow checks that the DSDT include path exists, overlays the authored pipa ASL, compiles it to AML, and then runs the upstream build. The ACPI workflow separately compiles the authored DSDT and decompiles the four-table set for inspection.

The upstream pipa DSDT contains sentinel-like values (for example `0xDEADBEEF`, `0xABCABCAB`, and `0xFFFFFFFF`) and a device model that is not accepted as the current pipa DSDT.

## Aloha, Andromeda and Mu-Silicium comparison

- [Project Aloha](https://github.com/Project-Aloha/mu_aloha_platforms) contains ACPI implementations for several other Qualcomm platforms. Its MTP8996 DSDT is not a pipa table and must not be copied into the pipa build. Its SM8250 integration code can inform build structure, subject to license and implementation review.
- [WOA-Project/Andromeda](https://github.com/WOA-Project/mu_andromeda_platforms) includes Surface Duo ACPI examples and other SoC/platform directories, but no matching pipa device table set was identified. Do not substitute Surface Duo or SM8150/SM8350 device-specific tables.
- [Project-Silicium/Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) has pipa platform resources and a different organization from the older Mu-Qcom package. Use it as a structural reference, not evidence that differently named tables are interchangeable.

## Validation boundary

- The ACPI workflow checks AML compilation/decompilation and presence of the SM8250 reference binaries.
- The UEFI workflow integrates the local DSDT into the Mu-Qcom pipa package before building.
- Neither workflow has been reported as passing in this audit. A successful ASL compile does not prove the full UEFI build succeeds or that the hardware works.
- APIC CPU MPIDRs, GIC distributor/redistributor addresses, performance interrupts, GTDT timer interrupts and FADT reset settings still need pipa-specific validation.

## Remaining blockers before calling the UEFI build-ready

1. Run the ACPI workflow and fix any actual ACPICA or artifact-validation failures.
2. Run the on-demand UEFI workflow and fix any submodule, toolchain, FDF, or build failures.
3. Verify APIC/GTDT/FADT table values against the actual pipa/SM8250 platform.
4. Complete the UFS Windows driver/resource model and clock/reset/PHY sequencing.
5. Do not flash an experimental image based only on a successful ASL compile or table decompilation.

## References

- [Project-Silicium ACPI guide](https://github.com/Project-Silicium/Guides/blob/main/Silicium/Porting/Snapdragon/ACPI/README.md) — explicitly marked outdated.
- [Project-Silicium device-porting guide](https://github.com/Project-Silicium/Guides/blob/main/Silicium/Porting/Snapdragon/Device/README.md) — explicitly marked outdated.
- [Mu-Qcom fork](https://github.com/sairajkandkar007/Mu-Qcom)
- [Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium)
- [Project Aloha](https://github.com/Project-Aloha/mu_aloha_platforms)
- [Andromeda](https://github.com/WOA-Project/mu_andromeda_platforms)
