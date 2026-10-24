# Testing and Reporting Guide

This project is experimental. This guide focuses on documenting research and reducing avoidable risk; it does not certify any build as safe.

## Before a test
- Read the full procedure and understand its purpose.
- Confirm the device codename and exact hardware variant.
- Back up important files.
- Ensure you understand the available recovery method and its limitations.
- Do not proceed if the image source, compatibility, or recovery plan is unclear.

## Record the environment
- Date and time
- Device and variant
- Firmware / bootloader state, when relevant
- Build or commit identifier
- Exact test steps
- Expected result
- Actual result
- Logs, screenshots, and limitations

## Share logs safely
Remove authentication tokens, passwords, private keys, serial numbers, and other personal information before publishing. Do not upload proprietary firmware or driver files unless you have permission to redistribute them.

## Reporting template

```text
Component:
Device / variant:
Test date:
Build or revision:
Steps:
Expected result:
Observed result:
Logs / screenshots:
Known limitations:
```
