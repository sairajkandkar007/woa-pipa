#!/system/bin/sh
# Collect kernel/build identity for Xiaomi Pad 6 (pipa) source matching.
# Read-only: does not modify the device.
set -u

out="${1:-pipa-kernel-identity.txt}"
{
    echo "=== collection ==="
    date 2>/dev/null || true
    echo
    echo "=== uname ==="
    uname -a 2>&1 || true
    echo
    echo "=== proc/version ==="
    cat /proc/version 2>&1 || true
    echo
    echo "=== kernel command line ==="
    cat /proc/cmdline 2>&1 || true
    echo
    echo "=== build properties ==="
    for key in \
        ro.build.fingerprint \
        ro.build.version.incremental \
        ro.build.version.release \
        ro.build.version.sdk \
        ro.product.device \
        ro.product.vendor.device \
        ro.bootimage.build.fingerprint \
        ro.boot.slot_suffix \
        ro.boot.verifiedbootstate
    do
        printf '%s=' "$key"
        getprop "$key" 2>/dev/null || true
    done
    echo
    echo "=== relevant kernel config ==="
    if [ -r /proc/config.gz ]; then
        zcat /proc/config.gz 2>/dev/null | grep -E '^(CONFIG_(ARM64|SCSI_UFSHCD|SCSI_UFS_QCOM|PHY_QCOM_QMP|RESET_QCOM|COMMON_CLK_QCOM|ACPI|EFI|EFI_STUB|BLK_DEV_INITRD))=' || true
    else
        echo "/proc/config.gz unavailable"
    fi
    echo
    echo "=== device-tree model/compatibles ==="
    for f in /sys/firmware/devicetree/base/model /sys/firmware/devicetree/base/compatible; do
        if [ -r "$f" ]; then
            echo "--- $f"
            tr '\000' '\n' < "$f" 2>/dev/null || true
        fi
    done
} > "$out" 2>&1
status=$?
echo "Wrote $out (exit=$status)"
exit "$status"
