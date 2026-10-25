# Upstream ACPI references

This directory contains verbatim reference files copied from public upstream repositories to support source-level comparison. These files are **not** a drop-in ACPI set for Xiaomi Pad 6 (pipa), and must not be wired into pipa's build merely because their names match.

## Provenance

- [Mu-Qcom](https://github.com/sairajkandkar007/Mu-Qcom), `main`: SM8250 table disassembly listings and the pipa ACPI include manifest. The files named `APIC.dsl`, `FACP.dsl`, and `GTDT.dsl` are ACPICA disassembly dumps, not compilable ASL source.
- [Project-Aloha/mu_aloha_platforms](https://github.com/Project-Aloha/mu_aloha_platforms), `main`: SM8250 `AcpiPlatformUpdateLib` source and a separate MTP8996 device DSDT. The library is relevant to compare against the SM8250 firmware architecture; the MTP8996 DSDT is retained strictly as a different-device example.
- [WOA-Project/mu_andromeda_platforms](https://github.com/WOA-Project/mu_andromeda_platforms), `main`: Surface Duo 1 FACP ASLC example. It is not a pipa table.

## Integration rules

1. Keep these files in reference-only paths unless their role and license are reviewed.
2. Do not use the MTP8996 DSDT or Surface Duo FACP as pipa production tables.
3. Author or obtain real ASL/ASLC source for the exact tables needed by pipa's firmware build, then compile and validate the resulting AML.
4. Preserve upstream notices and license terms. Consult each source repository's license before redistributing further.
5. Do not enable the current pipa UFS scaffold until resource windows, GIC interrupts, and dependencies are confirmed from pipa-specific evidence.
