# Running-kernel source match — Xiaomi Pad 6 (pipa)

Date: 2026-10-24

## Evidence collected from the tablet

User-provided root collector output reports:

- `uname -r`: `4.19.157-perf-g240e1d0a5f52`
- Build compiler: Clang 10.0.7 for Android NDK; GNU ld 2.27
- Build timestamp: Tue Dec 16 08:39:58 UTC 2025
- Android fingerprint: `Xiaomi/pipa_in/pipa:14/UKQ1.240624.001/OS2.0.11.0.UMZINXM:user/release-keys`
- Device-tree model: `Qualcomm Technologies, Inc. xiaomi pipa`
- Device-tree compatible strings: `qcom,kona-mtp`, `qcom,kona`, `qcom,mtp`
- Kernel command line identifies the boot UFS device as `1d84000.ufshc`.
- Runtime config confirms `CONFIG_ARM64=y`, `CONFIG_SCSI_UFSHCD=y`, `CONFIG_SCSI_UFS_QCOM=y`, and `CONFIG_COMMON_CLK_QCOM=y`.
- `/proc/config.gz` is unavailable, so a full running config was not collected.

Privacy: the raw command line also contained a device serial number and other boot identifiers. Those are intentionally omitted from this report.

## Public source branch inspected

Repository: [sairajkandkar007/Xiaomi_Kernel_OpenSource](https://github.com/sairajkandkar007/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)

- The branch's top-level `Makefile` declares version `4.19.157`, matching the running kernel's base version.
- The branch contains `arch/arm64/configs/pipa_user_defconfig`.
- That defconfig sets `CONFIG_LOCALVERSION="-perf"` and `CONFIG_LOCALVERSION_AUTO=y`, consistent with the `-perf` portion of the running release string.
- The branch's latest commit metadata identifies the tree as Xiaomi Pad 6 kernel changes based on Qualcomm tag `LA.UM.9.12.r1-13300-SMxx50.QSSI12.0-1`, with `pipa_user_defconfig` named as the config used.
- The checked-in pipa defconfig has `# CONFIG_EFI is not set`. This is consistent with an Android-oriented kernel configuration, not proof of a Windows-capable kernel.
- Searching the repository's commit metadata for the literal suffix `g240e1d0a5f52` did not find a matching commit.

## Assessment

**Partial match; exact source revision not proven.**

The base version (4.19.157), `-perf` local version, device-specific defconfig, and Kona/pipa identity make this branch a plausible lineage match. However, the `g240e1d0a5f52` suffix was not matched to a commit, and the Android build was produced in December 2025 while the branch's inspected tip is older. Do not claim that the current source tree exactly reproduces the running binary.

## ACPI/UFS implications

The live device tree remains the primary resource evidence. Its UFS host at `0x01D84000` has a 0x3000-byte controller range, an ICE-named range at `0x01D90000` of 0x8000 bytes, and DT interrupt cells `<0 0x109 0x4>`. These do not by themselves establish the Windows ACPI HID, GSI encoding, or driver contract.

Keep the project's provisional UFS ACPI device hidden (`_STA = Zero`) and without `_CRS` until the Windows UFS driver ID/resource contract and clock/reset/PHY/power sequencing are established. An IASL compile only proves AML syntax, not hardware functionality.

## Next source-identification step

The most useful additional evidence would be the exact kernel source commit or build manifest used by Xiaomi for `OS2.0.11.0.UMZINXM`, plus the matching kernel build configuration. The public branch currently supports a lineage-level match, not an exact commit match.
