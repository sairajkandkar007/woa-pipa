# Windows on ARM for Xiaomi Pad 6 (pipa)

An experimental community project exploring Windows on ARM (WoA) support on the **Xiaomi Pad 6**, codename **pipa**, based on the Qualcomm Snapdragon 870 platform.

> **Status: Research and planning.** This repository does not claim that Windows currently boots or that any specific hardware component works. Update the status only when results have been verified on the actual device.

## Project goals

- Research the device boot chain and UEFI requirements.
- Investigate the Qualcomm platform and relevant ARM64 Windows driver architecture.
- Research Adreno 650 graphics support, with an initial focus on desktop usability rather than gaming.
- Track hardware compatibility, test results, known limitations, and reproducible findings.
- Build clear documentation so developers and beginners can contribute.

## Device overview

| Item | Information |
|---|---|
| Device | Xiaomi Pad 6 |
| Codename | pipa |
| SoC | Qualcomm Snapdragon 870 |
| GPU | Qualcomm Adreno 650 |
| Target OS | Windows on ARM (ARM64) |
| Project stage | Research and planning |

Specifications are a starting point for research; confirm implementation details against reliable device sources before making changes.

## Repository map

- [ROADMAP.md](ROADMAP.md) — proposed milestones.
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to contribute.
- [docs/device-overview.md](docs/device-overview.md) — device notes and source references.
- [docs/boot-architecture.md](docs/boot-architecture.md) — boot and UEFI research.
- [docs/driver-status.md](docs/driver-status.md) — component support tracking.
- [docs/testing-guide.md](docs/testing-guide.md) — safe, reproducible reporting.
- [research/references.md](research/references.md) — external technical references.
- [compatibility/status.md](compatibility/status.md) — verified compatibility status.

## Community

Telegram group: https://t.me/+6SvVXdaRAYU5ZDZl

Developers with experience in UEFI, EDK II, ACPI, ARM64, Qualcomm platforms, Windows drivers, and hardware debugging are welcome. Beginners who want to learn and document research are welcome too.

## Safety and scope

- Treat all future builds and instructions as experimental unless explicitly marked otherwise.
- Back up important data before device experiments.
- Verify device codename, image compatibility, and recovery options before changing partitions or firmware.
- Never publish private keys, credentials, personal data, or files you do not have permission to redistribute.
- Keep logs free of serial numbers and other personal identifiers where possible.

This is an unofficial community project and is not affiliated with Xiaomi or Microsoft. Windows on ARM support is not guaranteed.
