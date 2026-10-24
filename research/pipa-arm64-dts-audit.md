# pipa ARM64 and Qualcomm Device-Tree Audit

**Status:** Partial inspection; no directory listing has been retrieved through the available GitHub file-reading action.  
**Source branch:** [Xiaomi_Kernel_OpenSource / pipa-t-oss](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)  
**Audit date:** 2026-10-24

## Scope

Directories supplied for the next inspection:
- [arch/arm64](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss/arch/arm64)
- [arch/arm64/boot/dts/qcom](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss/arch/arm64/boot/dts/qcom)

## Confirmed so far

- The branch's `arch/arm64/Makefile` exists and contains ARM64 kernel build rules.
- The top-level kernel `Makefile` declares Linux version 4.19.157.
- `drivers/gpu/msm/Kconfig` contains the Qualcomm KGSL Linux graphics-driver option.
- The exact paths `arch/arm64/configs/vendor/pipa_defconfig` and `arch/arm64/configs/pipa_defconfig` were not found in the initial direct-file checks. This is not proof that a pipa configuration is absent; we still need a directory listing or build scripts to find the real path.

## Important limitation

The GitHub file-reading interface used for this audit retrieves files, not directory listings. Searches for the exact device configuration and device-tree paths did not return matching excerpts. Therefore, the actual filenames under the two supplied directories remain **unverified**. Do not treat a guess about a filename as a source finding.

## Next actions

1. Obtain the directory listing for `arch/arm64/configs` and `arch/arm64/boot/dts/qcom` using GitHub's web UI or a GitHub API tree listing.
2. Locate the pipa-specific defconfig, DTS/DTSI files, and the device-tree Makefile entries that include them.
3. Trace the DTS include chain and record only hardware facts directly supported by those files.
4. Inspect relevant driver/configuration entries for display, touchscreen, USB, storage, Wi-Fi, audio, power, and firmware.
5. Separate source evidence from assumptions and from anything actually tested on hardware.

## WoA relevance

Device trees and Linux driver source can reveal hardware topology and software dependencies, but they are not Windows ACPI tables or Windows drivers. This audit alone cannot establish that Windows boots, that UEFI is available, or that Adreno 650 acceleration works under Windows.
