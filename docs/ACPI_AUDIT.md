# Xiaomi Pad 6 (pipa) ACPI audit

Audit date: 2026-10-24

## Scope

This is a source-level audit of the public Mu-Qcom and EDK2-MSM repositories. It is not a claim that the resulting firmware has been tested on hardware.

## Findings

### Mu-Qcom

- `Platforms/Xiaomi/pipaPkg/AcpiTables/DSDT.dsl` exists (1,985 lines; 80,982 bytes at time of inspection).
- `Platforms/Xiaomi/pipaPkg/AcpiTables/DSDT.aml` also exists. Its binary contents have not yet been disassembled and compared with the ASL source, so source/AML synchronization is unverified.
- `Platforms/Xiaomi/pipaPkg/Include/ACPI.inc` packages:
  - `SM8250Pkg/AcpiTables/APIC.aml`
  - `pipaPkg/AcpiTables/DSDT.aml`
  - `SM8250Pkg/AcpiTables/FACP.aml`
  - `SM8250Pkg/AcpiTables/GTDT.aml`
- The DSDT declares a UFS device `UFS0` with HID `QCOM24A5`, MMIO base `0x01D84000`, length `0x0001C000`, and interrupt `0x129` (decimal 297). These match the values in EDK2-MSM's Xiaomi `alioth` UFS ASL, but must still be checked against pipa's device tree and interrupt wiring.
- The DSDT has 15 `Device()` declarations, 22 `Method()` declarations and no ASL `Include()` statements.
- The DSDT contains multiple suspicious/unresolved sentinel values, including `0xDEADBEEF`, `0xBEEFDEAD`, `0xAAAAAAAA`, `0xBBBBBBBB`, `0xCCCCCCCC`, `0xDDDDDDDD`, `0xEEEEEEEE`, `0x77777777`, `0xABCABCAB`, and repeated `0xFFFFFFFF`. Some might be intentional protocol defaults, but each use needs to be traced before use is approved.
- The current ACPI include comments out several common tables (for example IORT, SPCR, PPTT and DBG2). Do not enable or recreate these without establishing whether Windows and the current platform need them.

### EDK2-MSM

- The SM8250 platform has Xiaomi `alioth` ACPI sources and Lenovo SM8250 device-specific sources, but no `Platform/Xiaomi/sm8250/AcpiTables/pipa/` source directory was found in the repository tree.
- The Xiaomi `lmi.fdf.inc` example includes `Platform/Xiaomi/sm8250/AcpiTables/alioth/DSDT.aml` (despite the platform being named `lmi`) and shared Qualcomm SM8250 APIC/FACP/GTDT AML files. This demonstrates the FDF packaging pattern, not a verified pipa configuration.
- The `alioth/ufs.asl` source uses the same UFS resource values as Mu-Qcom's DSDT. This is a useful cross-reference, not independent proof that the values are correct for pipa.
- A device-specific DSDT is not enough on its own: the platform FDF/DSC, ASL compilation rules, shared tables, and ACPI installation path must all be audited together.

### Device-tree evidence limitation

The Mu-Qcom tree has an `ImageResources/DTBs/pipa.dtb` binary, but its companion `ImageResources/DTBs/pipa.dts` returned empty text through the repository contents interface. The kernel tree search did not establish a pipa-specific source DTS. Therefore, this audit has **not** yet independently verified pipa's UFS address/IRQ or peripheral resource map against a readable, board-specific DTS.

## Required work before adding ACPI to EDK2-MSM

1. Obtain/decompile the exact pipa DTB used by the current firmware and identify UFS, GPIO, I2C/SPI, interrupt, USB, display, touch, WLAN, audio, battery and sensor resources.
2. Disassemble Mu-Qcom's checked-in DSDT AML with IASL and compare the resulting ASL to `DSDT.dsl`; compile the source independently with the correct IASL version and review warnings/errors.
3. Trace each sentinel value and each resource descriptor; replace only values proven wrong or missing from authoritative platform evidence.
4. Verify processor topology and low-power idle states against SM8250 and the current UEFI's power/PSCI implementation.
5. Audit the shared APIC/FACP/GTDT AML files and table headers/versions, then decide which common tables are already correctly supplied by EDK2-MSM.
6. Add pipa-specific ASL sources and explicit build/FDF integration only after resource validation; run IASL and build checks on GitHub Actions.
7. Inspect the generated firmware's ACPI tables before attempting a Windows boot. A successful IASL compile alone does not validate hardware resources or Windows compatibility.

## Safety status

No ACPI source, table binary, firmware image, or device has been modified by this audit. The EDK2-MSM build workflow should not be dispatched until the missing pipa ACPI source and build integration are implemented and validated.
