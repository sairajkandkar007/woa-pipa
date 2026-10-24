# ACPI source investigation continuation — pipa kernel and UFS

Date: 2026-10-24

## Findings from the public Xiaomi kernel branch

Repository: [Xiaomi_Kernel_OpenSource, branch pipa-t-oss](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)

1. The branch's top-level `Makefile` is Linux 4.19.157, matching the numeric base of the tablet's running release `4.19.157-perf-g240e1d0a5f52`.
2. `arch/arm64/configs/pipa_user_defconfig` exists and selects `CONFIG_LOCALVERSION="-perf"` plus `CONFIG_LOCALVERSION_AUTO=y`, which is consistent with the reported release naming.
3. The tip commit is `77dc8e2290f3f5f15861821564f68d30ea193449`, authored 2023-05-16. Its message says it contains Xiaomi Pad 6 kernel changes based on Qualcomm tag `LA.UM.9.12.r1-13300-SMxx50.QSSI12.0-1` and names `pipa_user_defconfig`.
4. The public branch's `arch/arm64/boot/dts/qcom/` directory exposes `kona-mtp.dtsi` among the matching Kona/pipa-named entries, but no obvious pipa-specific board DTS file was found in that directory listing. This is not proof that all board DT sources are absent; Android device-tree overlays and vendor-supplied DTBs may live outside this directory.
5. A commit search for the exact suffix `g240e1d0a5f52` returned no match. The reported December 2025 build date is much later than the inspected branch tip. Therefore this is a **lineage/configuration match only**, not an exact binary/source match.

## UFS implementation evidence

The branch's `drivers/scsi/ufs/ufshcd-pltfrm.c` contains:
- a named lookup for reset control `core_reset`;
- clock parsing that reads `clock-names` and `freq-table-hz` from the Linux device-tree node;
- platform resource mapping for the controller through the platform-device resource path.

This confirms that a Linux UFS implementation depends on more than the controller's base address. It does not define the Windows ACPI HID or guarantee that a Windows driver can initialize this controller.

## Current safe design decision

Keep `UFS0` hidden in `acpi/pipa/DSDT.asl` with `_STA = Zero` and no `_CRS`. The live DT provides strong evidence for the controller's physical ranges and interrupt specifier, but not the Windows driver contract or a complete ACPI replacement for Linux clock/reset/PHY/power setup.

Do not turn the device on just to test whether Windows sees it: the UFS controller is the tablet's boot storage path, and an incorrect firmware description can make the Windows boot path unusable.

## Next concrete task

Build a machine-readable resource inventory from the already-collected live DT archive, and make CI test that parser against the known archive layout. Separately, obtain a trusted Windows UFS driver ACPI-ID/resource specification before designing the final UFS ACPI device. The parser is an evidence-reporting tool only; it must not auto-generate or enable AML resources.
