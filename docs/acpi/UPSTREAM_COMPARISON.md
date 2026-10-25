# ACPI upstream comparison and repository layout plan

Audit basis: public default-branch Git trees and selected text files fetched for this review. This is a source-layout and compatibility audit, not proof that any table works on hardware.

## Findings

### This repository: `sairajkandkar007/woa-pipa`

- The current pipa-specific source scaffold is `acpi/pipa/DSDT.asl`. Its header documents that UFS is intentionally disabled pending resource verification.
- A legacy/reference Mu-Qcom snapshot is retained under `third_party/mu-qcom-pipa-snapshot/`.
- Its `Platforms/Xiaomi/pipaPkg/Include/ACPI.inc` includes:
  - `SM8250Pkg/AcpiTables/APIC.aml`
  - `pipaPkg/AcpiTables/DSDT.aml`
  - `SM8250Pkg/AcpiTables/FACP.aml`
  - `SM8250Pkg/AcpiTables/GTDT.aml`
- In the current repository tree, only the pipa `DSDT.dsl` reference file and `ACPI.inc` are present at those relevant paths. The referenced SM8250 table source/binaries and compiled pipa `DSDT.aml` are absent from the checked-in snapshot. Therefore, the existing include file points at files that are not present in this repository snapshot.

### Project Aloha: `Project-Aloha/mu_aloha_platforms`

- Contains ACPI table files for other platforms/devices, including `Platforms/IstariPkg/Device/qcom-mtp8996/ACPI/DSDT.dsl` and AML tables under IstariPkg.
- MTP8996 is not the pipa/Snapdragon 870 platform. Those device-specific tables must **not** be copied into the pipa build as if they matched.
- Aloha can be used as a structural/build-integration reference only after checking its exact SoC package, source licensing, and implementation assumptions.

### Andromeda: `WOA-Project/mu_andromeda_platforms`

- The inspected tree includes ACPI material for Surface Duo devices and SoC-specific directories for SM8150 and SM8350.
- No pipa/SM8250-specific set matching the referenced pipa include paths was found in the inspected tree.
- Similar filenames or a shared Qualcomm/Project Mu lineage do not establish hardware compatibility. Do not import the Surface Duo or SM8150/SM8350 tables into pipa.

### Mu-Silicium: `Project-Silicium/Mu-Silicium`

- Its tree includes the pipa platform package and device-tree resources, but the older Mu-Qcom table paths do not appear as direct matching paths in the inspected tree.
- Use its pipa package/device-tree organization as a layout reference; use the pipa DTS and live device-tree audits as evidence for pipa hardware resources.
- Do not assume the lack of a matching path means the implementation is missing entirely; paths and generation methods may differ.

### Mu-Qcom fork: `sairajkandkar007/Mu-Qcom`

- The fork's files named `APIC.dsl`, `FACP.dsl`, and `GTDT.dsl` returned ACPI disassembly-style listings (offsets, decoded table headers and fields), not conventional ASL source suitable to feed directly to `iasl`.
- These files should not be blindly copied into a source directory and treated as compilable DSL.
- The fork's current table values are useful for comparison, but must be checked against the pipa hardware and table-generation path before use.

## Restructuring decision

Keep the legacy snapshot in `third_party/` so existing scripts and provenance are not broken. Keep authored pipa ASL in `acpi/pipa/`, and use `docs/acpi/` for evidence and audit records. The new `acpi/README.md` defines this separation.

No unrelated Aloha or Andromeda device tables have been imported. No AML binaries have been fabricated. The missing table references are recorded as build blockers until valid, source-backed SM8250 tables and a compiled pipa DSDT are supplied or generated.

## Next engineering steps

1. Decide the actual upstream package layout used by the build and correct the missing-file references only after inspecting the DSC/FDF/build scripts.
2. Obtain or author genuine compilable ASL for APIC, FACP, GTDT, and the pipa DSDT; preserve attribution and license requirements.
3. Compile authored sources with `iasl` in CI and fail the workflow on compilation errors.
4. Validate the generated table signatures, lengths, checksums, CPU/GIC topology, timer interrupts, and pipa resource map.
5. Do not enable UFS in DSDT until the full controller/PHY/clock/reset/power model and Windows driver expectations are verified.

## Reference repositories

- [woa-pipa](https://github.com/sairajkandkar007/woa-pipa)
- [Mu-Qcom fork](https://github.com/sairajkandkar007/Mu-Qcom)
- [Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium)
- [Project Aloha](https://github.com/Project-Aloha/mu_aloha_platforms)
- [Andromeda](https://github.com/WOA-Project/mu_andromeda_platforms)
