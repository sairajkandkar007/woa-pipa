#!/usr/bin/env python3
"""Build the `pipa_k` DSDT for Xiaomi Pad 6 (pipa) from Silicium-ACPI's pipa DSDT.

Silicium-ACPI's Platforms/Xiaomi/pipa/DSDT.aml is Qualcomm's SM8250 MTP DSDT with pipa
edits, but every HID was renamed QCOM25xx -> QCOM05xx, so no WOA-Project Kona BSP driver
(windows_silicon_qcom_kona) can bind. This script:
  1. renames QCOM05 -> QCOM25 (same length, in-place, 152 occurrences)
  2. changes the TLMM GPIO controller (GIO0) _UID from 0 to 1, so an existing Windows
     install re-detects it and installs qcgpio instead of the inbox genpass driver
  3. (--ufs-rails) points the UFS0 PEP power votes at pipa's real UFS rails: the MTP DSDT
     votes LDO10_A (VCC, 2.904 V) and LDO3_C (VCCQ, 1.2 V), but pipa's UFS uses
     PM8150 L17A (VCC) and L6A (VCCQ) according to the stock device tree. Only the UFS0
     PEP entries are changed (occurrence indices verified against the Silicium-ACPI
     8c4cdc43 pipa DSDT).
  4. fixes the ACPI table checksum

  (--ufs-dep-scm) UFS0's _DEP on \\_SB.PEP0 -> \\_SB.SCM0 (boot-start qcscm), so the boot disk does not
     wait for PEP0. With qcpep installed but failing (code 31), Windows still boots.
  (--ufs-nopep) EXPERIMENT: renames the three PEP config entries for "\\_SB.UFS0" to "\\_SB.UFSZ"
     so PEP does not power-manage the UFS controller (it stays as UEFI left it).

Usage: make_pipa_k.py <Silicium-ACPI>/Platforms/Xiaomi/pipa/DSDT.aml <out>/DSDT.aml [--keep-uid] [--ufs-rails] [--ufs-nopep] [--ufs-dep-scm]
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
    # PEP0.PEPH returns the PEP's own hardware ID as a string ("ACPI\\VEN_QCOM&DEV_0519"); qcpep compares it
    # with its device ID and fails AddDevice with STATUS_NO_SUCH_DEVICE (code 31) on a mismatch.
    m = bytes(d).count(b'VEN_QCOM&DEV_05')
    d = bytearray(bytes(d).replace(b'VEN_QCOM&DEV_05', b'VEN_QCOM&DEV_25'))
    print(f'renamed {m} VEN_QCOM&DEV_05xx -> DEV_25xx string(s)')
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
    if '--ufs-rails' in sys.argv:
        def repl_nth(buf, old, new, which):
            assert len(old) == len(new)
            offs, i = [], buf.find(old)
            while i != -1:
                offs.append(i); i = buf.find(old, i + 1)
            assert len(offs) == {b'PPP_RESOURCE_ID_LDO10_A\x00': 6, b'PPP_RESOURCE_ID_LDO3_C\x00': 59}[old], 'unexpected DSDT'
            for k in which:
                buf[offs[k]:offs[k] + len(old)] = new
        repl_nth(d, b'PPP_RESOURCE_ID_LDO10_A\x00', b'PPP_RESOURCE_ID_LDO17_A\x00', [1, 2, 3, 4, 5])
        repl_nth(d, b'PPP_RESOURCE_ID_LDO3_C\x00', b'PPP_RESOURCE_ID_LDO6_A\x00', [15, 16, 17, 18])
        print('UFS0 PEP rails: LDO10_A -> LDO17_A (x5), LDO3_C -> LDO6_A (x4)')
    if '--ufs-dep-scm' in sys.argv:
        dev = bytes(d).find(b'UFS0')
        dep = bytes(d).find(b'_DEP', dev)
        k = bytes(d).find(b'\\._SB_PEP0', dep)
        assert dev == 0x192 and 0 < dep - dev < 40 and 0 < k - dep < 16, 'unexpected DSDT layout'
        d[k + 6:k + 10] = b'SCM0'
        print('UFS0 _DEP: PEP0 -> SCM0')
    if '--ufs-nopep' in sys.argv:
        old, new = b'\\_SB.UFS0\x00', b'\\_SB.UFSZ\x00'
        assert bytes(d).count(old) == 3, 'unexpected DSDT'
        d = bytearray(bytes(d).replace(old, new))
        print('UFS0 removed from PEP config (3 entries -> \\_SB.UFSZ)')
    d[9] = 0
    d[9] = (-sum(d)) & 0xFF
    assert sum(d) & 0xFF == 0 and len(d) == int.from_bytes(d[4:8], 'little')
    open(dst, 'wb').write(d)
    print(f'wrote {dst} ({len(d)} bytes, checksum ok)')

if __name__ == '__main__':
    main()
