#!/usr/bin/env python3
"""Build the `pipa_k` DSDT for Xiaomi Pad 6 (pipa) from Silicium-ACPI's pipa DSDT.

Silicium-ACPI's Platforms/Xiaomi/pipa/DSDT.aml is Qualcomm's SM8250 MTP DSDT with pipa
edits, but every HID was renamed QCOM25xx -> QCOM05xx, so no WOA-Project Kona BSP driver
(windows_silicon_qcom_kona) can bind. This script:
  1. renames QCOM05 -> QCOM25 (same length, in-place, 152 occurrences)
  2. changes the TLMM GPIO controller (GIO0) _UID from 0 to 1, so an existing Windows
     install re-detects it and installs qcgpio instead of the inbox genpass driver
  3. fixes the ACPI table checksum

Usage: make_pipa_k.py <Silicium-ACPI>/Platforms/Xiaomi/pipa/DSDT.aml <out>/DSDT.aml [--keep-uid]
"""
import sys

def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    d = bytearray(open(src, 'rb').read())
    assert d[:4] == b'DSDT', 'not a DSDT'
    n = bytes(d).count(b'QCOM05')
    d = bytearray(bytes(d).replace(b'QCOM05', b'QCOM25'))
    print(f'renamed {n} QCOM05xx -> QCOM25xx')
    if '--keep-uid' not in sys.argv:
        dev = None
        i = d.find(b'GIO0')
        while i != -1 and dev is None:
            if any(d[i - b] == 0x5B and d[i - b + 1] == 0x82 for b in range(2, 6)):  # DeviceOp
                dev = i
            i = d.find(b'GIO0', i + 1)
        assert dev is not None, 'Device (GIO0) not found'
        uid = d.find(b'_UID', dev)
        assert d[uid - 1] == 0x08 and uid - dev < 200, 'unexpected _UID encoding'
        print(f'GIO0 _UID {d[uid + 4]:#x} -> 0x1')
        d[uid + 4] = 0x01
    d[9] = 0
    d[9] = (-sum(d)) & 0xFF
    assert sum(d) & 0xFF == 0 and len(d) == int.from_bytes(d[4:8], 'little')
    open(dst, 'wb').write(d)
    print(f'wrote {dst} ({len(d)} bytes, checksum ok)')

if __name__ == '__main__':
    main()
