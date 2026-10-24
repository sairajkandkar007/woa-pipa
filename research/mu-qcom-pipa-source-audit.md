# Mu-Qcom fork audit — Xiaomi Pad 6 (pipa)

Date: 2026-10-24

Working fork: https://github.com/sairajkandkar007/Mu-Qcom
Upstream reference: https://github.com/vueDev-k/Mu-Qcom
Project documentation: https://github.com/sairajkandkar007/woa-pipa

## Scope

Read-only source inspection. No firmware was built, no image was generated, and no device was flashed.

## Verified findings

### 1. Build entry point

File: `build_uefi.sh`

The script:
- accepts a device codename through `-d` / `--device`;
- loads `configs/<codename>.conf`;
- builds `BootShim` using firmware base/size variables from that configuration;
- invokes `Platforms/<TARGET_DEVICE_VENDOR>/<codename>Pkg/PlatformBuild.py`;
- optionally sources `configs/<codename>.sh` to create a device image;
- supports a separate ACPI recompilation option that expects a device-specific `ACPI.inc`.

Therefore, adding a pipa entry to the status page alone would not make the device buildable. The required configuration, platform package, ACPI inputs and image-generation logic must be present and internally consistent.

### 2. Pipa status in Mu-Qcom

File: `Status.md`, section “Xiaomi Pad 6”, codename `pipa`.

The project-reported UEFI status marks display, clocks, UFS, buttons, USB, ACPI and slot switching as working; touchscreen is marked not working.

The Windows OS table marks boot, UFS, USB, CPU and display as working. Buttons, touchscreen, WLAN, battery/charging, audio, Bluetooth and GPU are marked not working in the table.

These are maintainer-reported status entries, not independent validation on this audit run.

### 3. Missing or unverified source paths

Attempts to retrieve these expected paths from the fork did not return files:
- `configs/pipa.conf`
- `configs/pipa.sh`
- `Platforms/Qualcomm/pipaPkg/PlatformBuild.py`

This does **not** prove the equivalent files are absent under every possible path or branch. It means we must inventory the repository tree and identify the exact vendor/package names before attempting a build.

## Next actions

1. Obtain the complete recursive tree for the fork's default branch, including submodule entries.
2. Locate all configuration files and find which one sets `TARGET_DEVICE_VENDOR`, `TARGET_FD_BASE`, `TARGET_FD_SIZE` and `TARGET_FD_BLOCKS`.
3. Locate all platform packages and check whether any package targets `pipa` or the Snapdragon 870 / SM8250 family.
4. Trace the pipa ACPI tables and image-generation script, if present.
5. Compare the fork with the upstream repository and record the exact commit IDs before proposing code changes.

## Safety / validation boundary

Do not build an image for pipa by substituting another device's configuration. Do not flash an image merely because the status table lists pipa as supported. A buildable configuration and a safe, device-specific boot/recovery path must be confirmed first.
