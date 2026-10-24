# Kernel Source Audit — Xiaomi Pad 6 (pipa)

**Status:** Initial static inspection only  
**Source fork:** https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss  
**Upstream reference:** https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss  
**Audit date:** 2026-10-24

## Verified findings

1. The `pipa-t-oss` branch is listed by Xiaomi's kernel-source index for **Xiaomi Pad 6**, Android T (Android 13), based on `LA.UM.9.12.r1-13300-SMxx50.QSSI12.0-1`. This identifies the intended source branch; it does not establish Windows compatibility.
2. The branch's top-level `Makefile` declares Linux kernel version **4.19.157**.
3. `arch/arm64/Makefile` is present, confirming that this source tree includes the ARM64 architecture build rules.
4. `drivers/gpu/msm/Kconfig` contains the Qualcomm KGSL driver option. Its help text describes Linux-side Adreno-family graphics support, including GPU memory management, scheduling, and power management. This confirms relevant Linux driver code exists in the tree, but does **not** prove which options are enabled in the pipa build.
5. The checked paths `arch/arm64/configs/vendor/pipa_defconfig` and `arch/arm64/configs/pipa_defconfig` were not found at those exact paths. The correct device config path still needs to be located; do not infer that no pipa config exists anywhere in the repository.

## What this does and does not tell us

- The source is an Android/Linux kernel tree, not a Windows kernel or a UEFI implementation.
- Linux touchscreen, display, USB, Wi-Fi, storage, and KGSL drivers cannot simply be loaded by Windows as native Windows drivers.
- Kernel source and device-tree information can help identify hardware interfaces and dependencies, but Windows support still requires suitable firmware/boot integration and compatible Windows drivers.
- The source branch's Android version and kernel version are facts about this source tree, not proof that the installed tablet currently runs an identical build.

## Next inspection tasks

1. Locate the actual pipa defconfig and related build scripts.
2. Find pipa-specific device-tree files and identify their include relationships.
3. Trace display, input, USB, storage, Wi-Fi, audio, power, and firmware dependencies.
4. Record each conclusion with a source path and distinguish source-level evidence from hardware-tested results.

## References

- [Xiaomi kernel source branch for pipa](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)
- [Forked pipa branch](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)
- [ARM64 architecture Makefile](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/blob/pipa-t-oss/arch/arm64/Makefile)
- [Qualcomm KGSL Kconfig](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/blob/pipa-t-oss/drivers/gpu/msm/Kconfig)
