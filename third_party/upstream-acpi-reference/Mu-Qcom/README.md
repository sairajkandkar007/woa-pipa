# Mu-Qcom ACPI reference snapshot

Source repository: https://github.com/sairajkandkar007/Mu-Qcom
Source ref: `main` (audited 2026-10-25)

These files are copied verbatim for traceability and comparison. The files named `APIC.dsl`, `FACP.dsl`, and `GTDT.dsl` in the upstream tree are ACPICA table-disassembly listings (byte-offset field dumps), not normal ASL source. **Do not feed them to `iasl -tc` as source and do not treat them as production-ready pipa tables.** They are retained as upstream reference evidence only.

The copied `ACPI.inc` shows the firmware table inclusion paths expected by that upstream platform package; it does not prove the referenced AML files are present in this repository or suitable for pipa. Reconcile the paths with the actual DSC/FDF/build configuration before integration.

The source project is distributed under its BSD-style license. See the upstream [LICENSE](https://github.com/sairajkandkar007/Mu-Qcom/blob/main/LICENSE). Keep the original license and copyright notices with redistributed source. This reference copy does not change the status of `acpi/pipa/DSDT.asl`, which remains a non-production scaffold.
