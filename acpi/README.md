# ACPI source layout

This directory is the canonical home for **pipa-specific ACPI source authored and reviewed for this project**.

## Source of truth

- `pipa/DSDT.asl` — current pipa-specific DSDT scaffold. It intentionally leaves UFS disabled; it is not a production table.
- `../docs/acpi/` — evidence, live device-tree audits, hardware-resource notes, and validation reports.
- `../third_party/mu-qcom-pipa-snapshot/` — retained upstream/reference snapshot. Do not treat its contents as the canonical edited source.

## Rules

1. Keep device-specific pipa ASL separate from SoC-family reference tables.
2. Do not copy ACPI tables from a different device or SoC merely because the filenames match.
3. Before enabling a device in DSDT, verify its memory ranges, interrupt specifiers, clocks, resets, PHYs, power dependencies, and Windows driver model against the pipa device tree and other reliable evidence.
4. Compile authored ASL with ACPICA `iasl`; then inspect compiler output and table headers/checksums. A decompiler/disassembly listing is not necessarily compilable ASL.
5. Do not integrate or flash experimental ACPI tables until they pass static validation and are tested through a recoverable development workflow.

See [upstream comparison](../docs/acpi/UPSTREAM_COMPARISON.md) for the Aloha, Andromeda, Mu-Qcom, and Mu-Silicium audit.
