# Vendored Mu-Qcom pipa UEFI source snapshot

This directory contains selected text source files copied from [sairajkandkar007/Mu-Qcom](https://github.com/sairajkandkar007/Mu-Qcom), branch `main`, for the Xiaomi Pad 6 (pipa) WoA research project.

## Scope and limits

- This is a **partial source snapshot**, not a standalone UEFI build tree.
- Mu-Qcom core packages and Git submodule contents are not vendored here. Their URLs and paths are recorded in `.gitmodules`.
- Binary resources such as the pipa DTB, ramdisk, and prebuilt firmware are not copied in this snapshot.
- No firmware build or device flashing has been performed.
- Keep this snapshot synchronized with upstream deliberately; it is not an independently maintained fork.

## Source provenance

- Upstream: https://github.com/sairajkandkar007/Mu-Qcom
- Source ref: `main`
- Snapshot date: 2026-10-24
- Upstream `LICENSE` is included. Individual files retain their own copyright and license headers.

## Included

- `configs/`: pipa build configuration and Android boot-image packaging script
- `Platforms/Xiaomi/pipaPkg/`: pipa platform metadata, ACPI source, and memory/configuration map libraries
- `BootShim/`: BootShim Makefile and assembly source
- `ImageResources/`: Android boot-image packaging script
- Root build/environment scripts and `.gitmodules`

## Before building

The required Mu-Qcom core packages, pinned submodule sources, binary resources, and toolchain still need to be made available and validated. Do not treat this snapshot alone as build-ready or flashable.
