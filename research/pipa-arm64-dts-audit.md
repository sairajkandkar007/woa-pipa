# pipa ARM64 and Qualcomm Device-Tree Audit

**Status:** Partial source audit; the kernel configuration is now identified, but the device-tree/build integration still needs investigation.  
**Source branch:** [Xiaomi_Kernel_OpenSource / pipa-t-oss](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)  
**Audited branch commit:** `77dc8e2290f3f5f15861821564f68d30ea193449`  
**Audit date:** 2026-10-24

## Confirmed findings

- The branch tree was retrieved using GitHub's recursive Git Trees API. It contains **66,939 paths** and the response reports `truncated: false`, so the returned path list is complete for this commit.
- The top-level kernel `Makefile` declares Linux version **4.19.157** (recorded in the earlier source audit).
- The branch commit message explicitly says: **“The kernel config file used is pipa_user_defconfig.”**
- Both `arch/arm64/configs/pipa_user_defconfig` and `arch/arm64/configs/vendor/pipa_user_defconfig` exist. They have different blob SHAs, so treat them as separate files until a full diff establishes whether they differ. The root-level config includes `CONFIG_ARCH_QCOM=y`, `CONFIG_ARCH_KONA=y`, and `# CONFIG_EFI is not set`. These are Linux kernel configuration settings, not proof of Windows compatibility.
- `AndroidKernel.mk` expects the Android build to provide `KERNEL_DEFCONFIG`; it uses that value to run the kernel `make` target. This file alone does not establish which config path the complete product build selects.
- `arch/arm64/boot/dts/qcom/Makefile` was retrieved successfully. Its visible DTB list includes generic Qualcomm targets such as `msm8996-mtp.dtb` and `sdm845-mtp.dtb`; no pipa-named target appears in that Makefile.
- A complete tree-path filter found no path containing `pipa` under the usual `arch/arm64/boot/dts/qcom` device-tree file paths; the only matching `pipa` paths were the two defconfigs and pipa-named power-supply driver files. This is a finding about filenames in this exact source commit, not proof that the device's DT information is absent from all vendor artifacts or other repositories.
- `drivers/gpu/msm/Kconfig` contains a Qualcomm KGSL Linux graphics-driver option. This establishes Linux-side source/configuration only; it does not establish that it is enabled for pipa or provide Windows Adreno 650 support.

## Interpretation and limitations

The strongest configuration lead is `pipa_user_defconfig`, corroborated by the branch's own commit message. The tree contains both a root-level and a `vendor/` copy. Before using either in a build, compare them fully and trace the device's Android product/build files to see which one is selected.

The usual Qualcomm DTS directory does not show an explicitly pipa-named target in its Makefile or filenames. The device may rely on other source locations or vendor-supplied/prebuilt device-tree artifacts; that remains unverified. Do not substitute a generic DTS/DTB just because it is present.

The configuration's `CONFIG_EFI` setting is disabled in the inspected defconfig. That is a relevant difference to investigate, but it does **not** by itself determine the UEFI strategy for a Windows-on-ARM project: UEFI may be a separate firmware layer, and the Linux defconfig is not a Windows boot configuration.

## Next actions

1. Compare the two `pipa_user_defconfig` files and record their exact differences.
2. Trace how Xiaomi's Android build sets `KERNEL_DEFCONFIG` and which config path it selects for pipa.
3. Inspect Qualcomm DTS/Kbuild entries and the device/vendor trees for references to pipa, DTB/DTBO packaging, and boot-image integration.
4. Inventory existing device boot/vendor artifacts before deciding whether a Linux DTS is needed for the UEFI/Windows path.
5. Only after this source mapping, make a separate driver feasibility matrix for display/GPU, touch, storage, USB, battery/charging, Wi-Fi, and audio.

## WoA relevance

Linux device trees and Linux driver source can help identify hardware topology and dependencies, but they are not Windows ACPI tables or Windows drivers. This audit does not establish that Windows boots, that UEFI is available, or that Adreno 650 acceleration works under Windows. No kernel build, UEFI build, or flashing was performed for this audit.

## Source links

- [pipa branch commit and its config note](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/commit/77dc8e2290f3f5f15861821564f68d30ea193449)
- [Root pipa_user_defconfig](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/blob/pipa-t-oss/arch/arm64/configs/pipa_user_defconfig)
- [Vendor pipa_user_defconfig](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/blob/pipa-t-oss/arch/arm64/configs/vendor/pipa_user_defconfig)
- [AndroidKernel.mk](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/blob/pipa-t-oss/AndroidKernel.mk)
- [GitHub Git Trees API documentation](https://docs.github.com/en/rest/git/trees)


## Follow-up: defconfig comparison and DTB packaging

The two configuration files were fetched from the same `pipa-t-oss` branch and compared byte-for-byte:

- `arch/arm64/configs/pipa_user_defconfig` — blob `d6a82b0f59de9cece8ba7d41a1823526c26b99b6`
- `arch/arm64/configs/vendor/pipa_user_defconfig` — blob `f682ea85a4b51270d9ade0d7a53d48cb2a7020ab`
- Both contain 21,020 characters and their fetched contents are identical. The blob SHAs differ despite identical text, so the files should not be assumed to be interchangeable paths in the build system without checking references.

The inspected `AndroidKernel.mk` confirms:

- `KERNEL_HEADER_DEFCONFIG` defaults to `KERNEL_DEFCONFIG` when not separately set (around lines 36–38).
- The kernel make invocation uses `$(KERNEL_DEFCONFIG)` as the make target (around line 193).
- When `TARGET_KERNEL_APPEND_DTB` is not `true`, it assembles `dtb.img` by concatenating `$(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/dts/vendor/qcom/*.dtb` (around lines 255–257).

This identifies the expected *output* directory for the Android DTB packaging path. It does not identify which product file sets `KERNEL_DEFCONFIG`, nor does it prove which DTB files are generated for pipa. The product/device tree and the actual output directory still need inspection.

## Updated next actions

1. Trace the product/device configuration that assigns `KERNEL_DEFCONFIG` and any `TARGET_KERNEL_APPEND_DTB` setting.
2. Inspect the source and build rules under `arch/arm64/boot/dts/vendor/qcom/`, especially the relevant Makefiles and included DTS files.
3. Identify how the pipa device's Android build selects and packages DTB/DTBO artifacts.
4. Keep the Linux configuration/DTS investigation separate from the Windows UEFI, ACPI, and Windows-driver work.


## Follow-up: exact defconfig comparison and build integration

On 2026-10-24, both config files were fetched from the `pipa-t-oss` branch:

- `arch/arm64/configs/pipa_user_defconfig` — blob `d6a82b0f59de9cece8ba7d41a1823526c26b99b6`
- `arch/arm64/configs/vendor/pipa_user_defconfig` — blob `f682ea85a4b51270d9ade0d7a53d48cb2a7020ab`

The fetched contents are byte-for-byte identical (21,020 characters each). The distinct blob IDs mean these are separate Git blobs, but their contents match in this audit.

The inspected `AndroidKernel.mk` shows that `KERNEL_HEADER_DEFCONFIG` defaults to `KERNEL_DEFCONFIG` when not set separately, and the kernel make invocation uses `$(KERNEL_DEFCONFIG)` as its target. When `TARGET_KERNEL_APPEND_DTB` is not `true`, the makefile creates `dtb.img` by concatenating generated files from `$(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/dts/vendor/qcom/*.dtb`.

**Still unconfirmed:** which pipa product/device makefile assigns `KERNEL_DEFCONFIG`, which DTB/DTBO files are produced for the device, and how the product packages them. The GitHub code-search connector searches the default branch, so its empty results do not establish absence on `pipa-t-oss`.

## Next investigation items

1. Locate the device/product makefiles that assign `KERNEL_DEFCONFIG` and `TARGET_KERNEL_APPEND_DTB`.
2. Inspect DTS/Kbuild rules under `arch/arm64/boot/dts/vendor/qcom/` and any included pipa-related source.
3. Identify the Android boot/vendor artifact packaging path for DTB and DTBO.
4. Keep this Linux source audit separate from UEFI, Windows ACPI, and Windows driver support. No kernel build, UEFI build, or flashing was performed.


## Automated full-tree inventory workflow

Added an on-demand GitHub Actions workflow at `.github/workflows/pipa-kernel-source-inventory.yml`. It requests the recursive Git tree for the `pipa-t-oss` ref, checks that GitHub did not mark the response as truncated, groups relevant paths (defconfigs, DTS/Kbuild files, build files, and DTB/DTBO names), and uploads both the raw tree JSON and a readable Markdown inventory as a 14-day artifact. It does not build or flash anything and requires no repository write permission.

**To run it:** open the workflow file in GitHub, choose **Actions → Pipa Kernel Source Inventory → Run workflow**, select the `main` branch, and start the run. After completion, open the run and download the `pipa-kernel-source-inventory` artifact. This workflow has been committed but has not been dispatched from this session because the available GitHub tools do not expose a workflow-dispatch action.

The inventory is the next evidence-gathering step. It will list candidate paths; it will not alone prove which product configuration is active. The follow-up must inspect the relevant product makefiles and actual DTB/DTBO packaging references.
