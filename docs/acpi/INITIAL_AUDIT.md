# Xiaomi Pad 6 (pipa) ACPI — initial source audit

Date: 2026-10-24

## Scope

This is a source-level audit of the existing Mu-Qcom pipa DSDT and EDK2-MSM's SM8250 platform layout. It is not a claim that the current AML is runtime-correct. No firmware was built, flashed, or tested on hardware as part of this audit.

## Findings

### Mu-Qcom has a pipa DSDT source

Source:
- `Platforms/Xiaomi/pipaPkg/AcpiTables/DSDT.dsl`
- `Platforms/Xiaomi/pipaPkg/Include/ACPI.inc`
- `Platforms/Xiaomi/pipaPkg/pipa.dsc`

The DSDT identifies itself as `QCOMM / SM8250`. It describes a UFS device with HID `QCOM24A5`, memory range base `0x01D84000`, length `0x0001C000`, and interrupt `0x129` (decimal 297). The EDK2-MSM Xiaomi `lmi` UFS ASL uses the same resource values. This establishes a shared template/resource claim, not proof that pipa's firmware routing is identical.

The DSDT also contains processor-container / CPU device declarations and low-power idle (`_LPI`) packages.

### The DSDT contains unresolved-looking platform data

The DSDT's initial `_SB` scope declares numerous values using patterns such as `0xFFFFFFFF`, `0xAAAAAAAA`, `0xDEADBEEF`, and `0xBEEFDEAD`, including names `SOID`, `SIDV`, `SVMJ`, `TCMA`, `TCML`, `SOSI`, `PRP0`–`PRP6`, and others.

These values may be sentinels used by the original platform implementation; their semantics must be traced before changing them. They must not be assumed to be valid hardware addresses. In particular, `TCMA` and `TCML` resemble placeholder patterns and require source-reference tracing.

### ACPI packaging in Mu-Qcom

`Platforms/Xiaomi/pipaPkg/Include/ACPI.inc` references these table binaries:
- `SM8250Pkg/AcpiTables/APIC.aml`
- `pipaPkg/AcpiTables/DSDT.aml`
- `SM8250Pkg/AcpiTables/FACP.aml`
- `SM8250Pkg/AcpiTables/GTDT.aml`

Other potential tables (DBG2, IORT, MCFG, PPTT, SPCR, TPM2, etc.) are commented out in this include. This include alone does not prove that every referenced AML is correct for pipa or that Windows needs all commented-out tables.

### EDK2-MSM has no pipa-specific ACPI source set in the audited tree

The SM8250 EDK2-MSM Xiaomi platform contains an `lmi` implementation and Lenovo SM8250 device implementations (including `j716f`, `tb-9707f`, and `tb371fc`). The audited tree did not contain a `Platform/Xiaomi/sm8250/AcpiTables/pipa/` source set.

The existing `lmi.fdf.inc` packages `Platform/Xiaomi/sm8250/AcpiTables/alioth/DSDT.aml` and Qualcomm common tables. This is a useful example of packaging structure, but it is not a pipa integration and should not be copied blindly.

### Source-to-binary consistency is not yet established

The source tree has both `DSDT.dsl` and `DSDT.aml`. This audit did not download/decompile the AML or establish that it was built from the current DSL. That is a required next check.

## Risk register

| Area | Status | Required evidence |
|---|---|---|
| DSDT syntax / compiler warnings | Not verified in this audit | Run IASL against the current DSL and preserve diagnostics |
| DSL-to-AML synchronization | Unknown | Compile source and compare table identity/content against checked-in AML |
| UFS address/IRQ | Matches EDK2-MSM lmi template; pipa-specific validity unverified | Confirm against pipa's SM8250 DT and firmware resource routing |
| CPU topology and _LPI | Present; correctness unverified | Compare CPU MPIDR/topology, idle states, and timer/PSCI behavior |
| Other devices | Not demonstrated by the inspected DSDT portions | Map pipa DT nodes and Windows driver expectations to ACPI devices |
| FADT/MADT/GTDT integration | Referenced by packaging include | Inspect each source/binary and confirm no duplicates or incompatible topology |
| EDK2-MSM integration | Missing pipa ACPI directory/source set in audited tree | Add only after validated source inventory |

## Next steps

1. Run IASL against the existing Mu-Qcom DSDT and record all errors/warnings without treating a successful compile as proof of correctness.
2. Inspect the DSDT's complete device tree and trace every sentinel/placeholder name to its consumers.
3. Obtain the actual pipa DT source from the kernel/device tree and extract the relevant hardware resources.
4. Compare pipa DT resources against the DSDT and the EDK2-MSM `lmi` / Lenovo SM8250 references.
5. Design the minimal pipa-specific EDK2-MSM ACPI set and FDF/DSC integration. Do not enable guessed peripheral resources.
6. Build and inspect AML/table output in CI before any device testing.

## References

- Mu-Qcom pipa DSDT: https://github.com/sairajkandkar007/Mu-Qcom/blob/main/Platforms/Xiaomi/pipaPkg/AcpiTables/DSDT.dsl
- Mu-Qcom ACPI include: https://github.com/sairajkandkar007/Mu-Qcom/blob/main/Platforms/Xiaomi/pipaPkg/Include/ACPI.inc
- Mu-Qcom pipa DSC: https://github.com/sairajkandkar007/Mu-Qcom/blob/main/Platforms/Xiaomi/pipaPkg/pipa.dsc
- EDK2-MSM Xiaomi lmi DSDT: https://github.com/edk2-porting/edk2-msm/blob/master/Platform/Xiaomi/sm8250/AcpiTables/lmi/Dsdt.asl
- EDK2-MSM Xiaomi lmi UFS ASL: https://github.com/edk2-porting/edk2-msm/blob/master/Platform/Xiaomi/sm8250/AcpiTables/lmi/ufs.asl
- EDK2-MSM Xiaomi lmi FDF include: https://github.com/edk2-porting/edk2-msm/blob/master/Platform/Xiaomi/sm8250/lmi.fdf.inc
