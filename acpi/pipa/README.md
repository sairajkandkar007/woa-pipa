# Pipa ACPI implementation — from scratch

This directory is the clean, device-specific starting point for ACPI on Xiaomi
Pad 6 (pipa / Snapdragon 870). It deliberately does not reuse the existing
Mu-Qcom DSDT as an implementation source.

## Current status

- `DSDT.asl` is an empty compile scaffold, **not a bootable DSDT**.
- No MMIO, IRQ, GPIO, clock, reset, or power values are guessed.
- CI compiles and decompiles the scaffold with ACPICA (`iasl`) and uploads the
  result for inspection.
- Do not package or flash this scaffold as a working UEFI/Windows image.

## Build-up order

1. Inventory pipa-specific device-tree sources, kernel bindings, and board data.
2. Create a resource ledger: device, address/length, interrupt, power/reset,
   source file and line, confidence, and validation notes.
3. Add only verified core tables/resources: interrupt controller and CPU
   topology, storage, and boot-critical buses.
4. Add display/input/USB, then power, battery, WLAN/Bluetooth, and audio.
5. Compile with warnings treated as review items; decompile and inspect AML.
6. Integrate into the UEFI package only after table composition and platform
   resource values are reviewed. A successful IASL compile alone is not proof
   of hardware correctness.

## Evidence rule

Use pipa's own device tree and driver sources as primary evidence. SM8250 donor
implementations are reference material only; never copy their device-specific
resource addresses or interrupt numbers without verifying them for pipa.
