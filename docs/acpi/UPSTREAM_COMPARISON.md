# ACPI upstream comparison and build integration status

Audit date: 2026-10-25. This is a source/layout audit, not proof that firmware boots or that Windows drivers work.

## Device-specific source

- Authored pipa DSDT scaffold: `acpi/pipa/DSDT.asl`.
- The scaffold deliberately hides UFS (`_STA = Zero`) and omits `_CRS`. It is syntax-testable, but it is not a finished device DSDT.
- The legacy snapshot under `third_party/mu-qcom-pipa-snapshot/` is reference material, not the canonical source tree for builds.
- The snapshot's old pipa DSDT contains sentinel values and a UFS resource model that must not be used as a production replacement.

## SoC-level tables and upstream source

The Mu-Qcom fork, [`sairajkandkar007/Mu-Qcom`](https://github.com/sairajkandkar007/Mu-Qcom), contains these binary ACPI tables in its SM8250 package:

- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/APIC.aml`
- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/FACP.aml`
- `Silicon/Qualcomm/SM8250Pkg/AcpiTables/GTDT.aml`

It also contains the source file `Platforms/Xiaomi/pipaPkg/AcpiTables/DSDT.dsl` and the package include `Platforms/Xiaomi/pipaPkg/Include/ACPI.inc`.

**Important:** the upstream files named `APIC.dsl`, `FACP.dsl`, and `GTDT.dsl` are ACPICA decoded table listings (offsets and field values), not ordinary ASL programs. Do not feed those listings to `iasl -tc` as though they were source. The checked-in `.aml` binaries are the usable reference artifacts; the workflow fetches them from the upstream repository and attempts to decompile them for structural validation.

The upstream pipa `DSDT.dsl` has many sentinel-like values (for example `0xDEADBEEF`, `0xABCABCAB`, and `0xFFFFFFFF`) and exposes UFS with a donor-derived resource window. It is not accepted as the current pipa DSDT. The local authored scaffold is compiled instead, with UFS intentionally disabled.

## Aloha, Andromeda and Mu-Silicium comparison

- [Project Aloha](https://github.com/Project-Aloha/mu_aloha_platforms) contains ACPI implementations for several other Qualcomm platforms. Its MTP8996 DSDT is not a pipa table and must not be copied into the pipa build. Its SM8250 integration code can inform build structure, subject to license and implementation review.
- [WOA-Project/Andromeda](https://github.com/WOA-Project/mu_andromeda_platforms) includes Surface Duo ACPI examples and other SoC/platform directories, but no matching pipa device table set was identified. Do not substitute Surface Duo or SM8150/SM8350 device-specific tables.
- [Project-Silicium/Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) has pipa platform resources and a different organization from the older Mu-Qcom package. Use it as a structural reference, not evidence that differently named tables are interchangeable.

## Current validation workflow

`.github/workflows/pipa-acpi-build.yml` now:

1. Installs ACPICA tools.
2. Fetches the APIC/FACP/GTDT binary tables from the Mu-Qcom SM8250 package.
3. Compiles the authored pipa DSDT scaffold.
4. Attempts to decompile all four AML tables and checks for the expected table identity.
5. Publishes the tables, source/decompilation output, upstream commit ID and a validation report as an artifact.

This validates table availability and compiler/decompiler processing only. It does **not** constitute a complete EDK II/Project Mu UEFI build, validate every table field against pipa hardware, or demonstrate bootability.

## Remaining blockers before calling UEFI build-ready

1. Run the ACPI validation workflow and fix any actual ACPICA or artifact-validation failures.
2. The DSC selection inconsistency was fixed on the Mu-Qcom fork's `main`: `GetPlatformDscAndConfig()` now returns the existing `pipaPkg/pipa.dsc` instead of the absent `pipaNoSb.dsc`.
3. Run the on-demand `pipa-uefi-build.yml` workflow and fix any actual submodule, toolchain, FDF, or build failures. The workflow overlays the authored DSDT scaffold and builds from the Mu-Qcom fork; it is not yet a verified successful build.
4. Verify APIC CPU MPIDRs, GIC distributor/redistributor addresses, performance interrupts, GTDT timer interrupts and FADT reset settings against the actual pipa/SM8250 platform.
5. Complete the UFS controller/ICE/PHY/clock/reset and Windows driver resource model before enabling UFS in DSDT.

Do not flash an experimental image based only on a successful ASL compile or table decompilation.

## References

- [Project-Silicium ACPI guide](https://github.com/Project-Silicium/Guides/blob/main/Silicium/Porting/Snapdragon/ACPI/README.md) — explicitly marked outdated.
- [Project-Silicium device-porting guide](https://github.com/Project-Silicium/Guides/blob/main/Silicium/Porting/Snapdragon/Device/README.md) — explicitly marked outdated.
- [Mu-Qcom fork](https://github.com/sairajkandkar007/Mu-Qcom)
- [Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium)
- [Project Aloha](https://github.com/Project-Aloha/mu_aloha_platforms)
- [Andromeda](https://github.com/WOA-Project/mu_andromeda_platforms)
