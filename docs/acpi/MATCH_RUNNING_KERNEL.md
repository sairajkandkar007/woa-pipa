# Match the running pipa kernel to source

Before making ACPI resource changes, capture the kernel/build identity from the **actual Xiaomi Pad 6 currently booted into Android**. This is read-only and does not flash or modify firmware.

## Collect on the tablet

Copy `tools/acpi/collect_kernel_identity.sh` to Termux and run:

```sh
su -c 'sh ./collect_kernel_identity.sh /sdcard/Download/pipa-kernel-identity.txt'
```

If the script is in Termux's private storage, use its full path. The output includes `uname`, `/proc/version`, kernel command line, Android build properties, selected kernel config symbols, and device-tree model/compatibles.

Then inspect the output and match:
1. Android build fingerprint and incremental version to the firmware/ROM build.
2. Kernel release/compiler string and build config to the exact kernel repository and branch.
3. UFS/PHY driver configuration and DT compatible strings to the source revision.

## Interpretation limits

- A ROM fingerprint alone does not uniquely identify the kernel commit.
- A kernel release string may be customized and is not sufficient proof of source identity.
- Missing `/proc/config.gz` does not mean a feature is absent.
- The collector intentionally does not print serial numbers, IMEI, Android ID, or account data.
- Do not publish the entire `/proc/cmdline` or build output publicly without reviewing it for device-specific details.

Until the kernel source is pinned, Qualcomm Android kernel findings remain comparative evidence only. Keep the UFS ACPI device disabled.
