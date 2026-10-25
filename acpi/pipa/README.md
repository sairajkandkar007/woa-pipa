# Pipa ACPI implementation — from scratch

This directory is the device-specific ACPI source for Xiaomi Pad 6 (pipa / Snapdragon 870).

## Current status

- `DSDT.asl` exposes a single UFS host device (`QCOM24A5`) as an **experimental discovery prototype**.
- It declares the live-tree UFS host MMIO range `0x01D84000` / `0x3000`, ICE range `0x01D90000` / `0x8000`, and candidate level/high GSI `0x129`.
- The interrupt mapping, HID/Windows driver contract, and clock/reset/PHY/power dependencies remain unverified or unimplemented.
- The UEFI build workflow copies this source into Mu-Qcom's pipa package, compiles it with `iasl`, and then builds the firmware. A successful compile is not a successful hardware validation.
- Do not flash this as a working UFS implementation.

## Build-up order

1. Keep pipa-specific source separate from SoC-level reference tables.
2. Resolve the Windows UFS driver's expected ACPI ID and resources.
3. Resolve host/PHY clocks, reset provider, PHY initialization ownership, and power sequencing.
4. Validate GSI, trigger, polarity, APIC/GIC and GTDT/FADT table values against pipa.
5. Compile/decompile AML and inspect table composition.
6. Only then perform recoverable device testing; don't treat IASL success as proof of hardware correctness.

## Evidence rule

Use pipa's own device tree and matching driver sources as primary evidence. SM8250 donor implementations are reference material only; never copy device-specific values without verifying them for pipa.
