# ACPI source layout

This directory is the canonical home for **pipa-specific ACPI source authored and reviewed for this project**.

## Source of truth

- `pipa/DSDT.asl` — pipa-specific experimental DSDT. UFS is exposed with the audited host and ICE MMIO ranges, but the Windows driver contract and clock/reset/PHY path are not verified. It is not production firmware.
- `../docs/acpi/` — evidence, live device-tree audits, hardware-resource notes, and validation reports.
- `../third_party/mu-qcom-pipa-snapshot/` — retained upstream/reference snapshot. Do not treat its contents as the canonical edited source.
- `../third_party/upstream-acpi-reference/Mu-Qcom/` — verbatim upstream listings and ACPI include reference, kept for comparison only.

## Rules

1. Keep device-specific pipa ASL separate from SoC-family reference tables.
2. Do not copy ACPI tables from a different device or SoC merely because filenames match.
3. UFS is currently exposed as an experimental discovery prototype. Do not interpret that as proof of a working Windows storage path.
4. Compile authored ASL with ACPICA `iasl`; inspect compiler output and table headers/checksums. A disassembly listing is not necessarily compilable ASL.
5. Do not flash experimental ACPI tables until the Windows driver ID/resource contract, GSI mapping, PHY, clocks, reset, power sequencing, and table composition have been validated and the device has a recoverable test path.

See [upstream comparison](../docs/acpi/UPSTREAM_COMPARISON.md) for the Aloha, Andromeda, Mu-Qcom, and Mu-Silicium audit.
