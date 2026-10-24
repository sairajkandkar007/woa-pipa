# Boot Architecture Research

This document is an outline for research. It is not a flashing guide.

## Topics to investigate
- Existing boot chain and firmware stages on pipa.
- Bootloader state and supported boot interfaces.
- EDK II / UEFI architecture and platform initialization requirements.
- ACPI tables and hardware description requirements for Windows.
- Storage, USB, display, and input initialization.
- Recovery paths and device-specific failure modes.

## Evidence standard

For each finding, record:
- Claim
- Source or test procedure
- Exact device and software revision
- Result and limitations
- Confidence: unknown, tentative, or verified

Do not flash firmware or partition images based only on assumptions from another device.
