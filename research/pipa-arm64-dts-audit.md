# pipa ARM64 and Qualcomm Device-Tree Audit

**Status:** Partial source inspection; the exact pipa configuration and DTS files are still not identified.  
**Source branch:** [Xiaomi_Kernel_OpenSource / pipa-t-oss](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)  
**Audit date:** 2026-10-24

## Confirmed findings

- `arch/arm64/Makefile` exists and contains ARM64 kernel build rules.
- The top-level kernel `Makefile` declares Linux version 4.19.157 (recorded in the earlier source audit).
- `drivers/gpu/msm/Kconfig` contains a Qualcomm KGSL Linux graphics-driver option. This establishes that Linux-side KGSL source/configuration exists, not that it is enabled for pipa or provides Windows GPU support.
- The file `arch/arm64/boot/dts/qcom/Makefile` was retrieved successfully. Its visible DTB list contains generic Qualcomm platform targets such as `msm8996-mtp.dtb` and `sdm845-mtp.dtb`; it does **not** list a pipa-named target.
- Direct checks for `arch/arm64/configs/vendor/pipa_defconfig` and `arch/arm64/configs/pipa_defconfig` did not find those exact paths. These checks do not prove that no pipa config exists elsewhere.

## What this means

The available Qualcomm DTS Makefile does not provide evidence of a pipa-specific device tree being built from this directory. The tree may use device-tree sources elsewhere, vendor build scripts, or prebuilt DT artifacts; those possibilities are unverified. Do not assume a generic DTS target matches Xiaomi Pad 6.

GitHub code searches for `pipa` and `pipa_defconfig` in the fork returned no matching results in the current search index. Search indexing may be incomplete, so this is not definitive proof of absence.

## Next actions

1. Inspect the actual listing of `arch/arm64/configs`, including subdirectories, and identify the configuration used by Xiaomi's build scripts.
2. Inspect the full `arch/arm64/boot/dts/qcom` listing and trace any DTB/DTBO files or includes relevant to the device.
3. Search the repository's build scripts for `KERNEL_DEFCONFIG`, `TARGET_KERNEL_CONFIG`, `DTS`, `DTB`, and `pipa`.
4. If the published source lacks device-specific DTS/configuration, document that as a source-release limitation and compare against the device's actual boot image/vendor artifacts before planning changes.
5. Keep source evidence separate from assumptions and from anything tested on hardware.

## WoA relevance

Linux device trees and Linux driver source can help identify hardware topology and dependencies, but they are not Windows ACPI tables or Windows drivers. This audit does not establish that Windows boots, UEFI is available, or Adreno 650 acceleration works under Windows. No build or flashing was performed for this audit.
