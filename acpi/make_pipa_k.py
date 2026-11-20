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

  (--pkg-fix) Silicium removed PM8009 entries from 8 packages but kept their element counts
     (PEP0 PPPP 61->57, PRRP 33->27, 6 camera CPXC packages). qcpep fails parsing PPRR with
     STATUS_ACPI_INVALID_DATA (0xC014000F). Fixes the NumElements bytes.

Usage: make_pipa_k.py <Silicium-ACPI>/Platforms/Xiaomi/pipa/DSDT.aml <out>/DSDT.aml [--keep-uid] [--ufs-rails] [--ufs-nopep] [--ufs-dep-scm] [--pkg-fix] [--qpep-crs] [--pipa-qup] [--pipa-i2c15] [--pipa-bklt]
"""
import sys, re

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
    if '--pkg-fix' in sys.argv:
        fix = {0x3469: (61, 57), 0x3b4a: (33, 27), 0x3c431: (17, 14), 0x3c5ee: (16, 13),
               0x3c7a7: (17, 14), 0x3c964: (16, 13), 0x3cb1d: (16, 14), 0x3ccda: (15, 13)}
        for off, (old, new) in fix.items():
            assert d[off] == old and 0x12 in (d[off - 2], d[off - 3]), f'unexpected DSDT at {off:#x}'
            d[off] = new
        print('fixed 8 package element counts (PPPP, PRRP, 6x CPXC)')
    if '--pipa-qup' in sys.argv:
        # pipa uses QUP1 SE3 (i2c11 @0xA8C000: KTZ8866 backlight, haptics) - not the Kona MTP buses.
        # Repurpose IC10 (QUP1 SE1 @0xA84000) as i2c11 in place, and park I2C5/I2C8/SPI4
        # (SEs that are touch-SPI/unused on pipa) by renaming their _HID so no driver binds.
        import struct
        b = bytes(d)
        def one(pat, what):
            assert b.count(pat) == 1, f'{what}: {b.count(pat)} matches'
            return b.index(pat)
        # _CRS of IC10: Memory32Fixed base + Interrupt
        crs_old = b'\x86\x09\x00\x01' + struct.pack('<II', 0xA84000, 0x4000)
        i = one(crs_old, 'IC10 mem'); d[i+4:i+8] = struct.pack('<I', 0xA8C000)
        irq_old = b'\x89\x06\x00\x01\x01' + struct.pack('<I', 0x182)
        j = b.find(irq_old, i, i + 64); assert j > 0, 'IC10 irq'; d[j+5:j+9] = struct.pack('<I', 0x184)
        # PEP descriptor of IC10: clocks + pins inside the \_SB.IC10 package
        k = one(b'\\_SB.IC10\x00', 'IC10 desc'); end = k + 1500
        n = 0
        for _ in range(2):
            c = bytes(d).find(b'gcc_qupv3_wrap1_s1_clk', k, end); assert c > 0, 'IC10 clk'
            d[c:c+22] = b'gcc_qupv3_wrap1_s3_clk'; n += 1
        pinmap = {0x27: 0x3C, 0x28: 0x3D}
        t = k
        for _ in range(4):   # D0 + D3 TLMMGPIO entries for the two pins
            t = bytes(d).find(b'TLMMGPIO\x00', t + 1, end); assert t > 0, 'IC10 TLMMGPIO'
            m = re.search(rb'\x0a([\x27\x28])', bytes(d[t:t + 16]))
            assert m, 'IC10 pin byte'
            d[t + m.start() + 1] = pinmap[m.group(1)[0]]
        # park the MTP buses
        parked = ((b'I2C8', b'QCOM2511'), (b'SPI4', b'QCOM250F')) if '--pipa-i2c15' in sys.argv else ((b'I2C5', b'QCOM2511'), (b'I2C8', b'QCOM2511'), (b'SPI4', b'QCOM250F'))
        for dev, hid in parked:  #, b'QCOM2511'), (b'SPI4', b'QCOM250F')):
            pos = bytes(d).find(dev + b'\x08_HID\x0d' + hid)
            assert pos > 0, f'{dev} _HID'
            q = pos + len(dev) + 6
            d[q:q+8] = b'QCOMFFFF'
        print('pipa QUP: IC10 -> i2c11 (0xA8C000, irq 0x184, wrap1_s3, gpio60/61); %s parked' % '/'.join(x[0].decode() for x in parked))
    if '--pipa-i2c15' in sys.argv:
        # Repurpose I2C5 (QUP0 SE4 @0x890000) as pipa i2c15 (QUP2 SE1 @0x884000: fsa4480, smb1390, hl6111r)
        import struct
        b = bytes(d)
        i = b.find(b'\x86\x09\x00\x01' + struct.pack('<I', 0x890000)); assert i > 0 and b.count(b'\x86\x09\x00\x01' + struct.pack('<I', 0x890000)) == 1, 'I2C5 mem'
        d[i+4:i+8] = struct.pack('<I', 0x884000)
        j = b.find(b'\x89\x06\x00\x01\x01' + struct.pack('<I', 0x27D), i, i + 64); assert j > 0, 'I2C5 irq'
        d[j+5:j+9] = struct.pack('<I', 0x267)
        pat = b'\rDEVICE\x00\r\\_SB.I2C5\x00'; assert b.count(pat) == 1, 'I2C5 desc'; k = b.index(pat) + 9
        end = b.find(b'\rDEVICE\x00', k + 10); end = end if end > 0 else k + 1500
        for old, new in ((b'gcc_qupv3_wrap0_s4_clk', b'gcc_qupv3_wrap2_s1_clk'), (b'gcc_qupv3_wrap_0_m_ahb_clk', b'gcc_qupv3_wrap_2_m_ahb_clk'),
                         (b'ICBID_MASTER_QUP_0', b'ICBID_MASTER_QUP_2'), (b'ICBID_SLAVE_QUP_0', b'ICBID_SLAVE_QUP_2')):
            t = k; n = 0
            while True:
                c = bytes(d).find(old, t, end)
                if c < 0: break
                d[c:c+len(old)] = new; t = c + 1; n += 1
            assert n > 0, old
            print(f'  {old.decode()} -> {new.decode()} x{n}')
        t = k; n = 0
        while True:
            t = bytes(d).find(b'TLMMGPIO\x00', t + 1, end)
            if t < 0: break
            m = re.search(rb'\x0a([\x33\x34])', bytes(d[t:t + 16])); assert m, 'I2C5 pin'
            d[t + m.start() + 1] = {0x33: 0x2C, 0x34: 0x2D}[m.group(1)[0]]; n += 1
        assert n == 4, f'I2C5 TLMMGPIO {n}'
        print('pipa i2c15: I2C5 -> 0x884000, irq 0x267, wrap2_s1, QUP_2, gpio44/45')
    if '--qpep-crs' in sys.argv:
        # QualcommPEP (SM8250 build) needs the RSC DRV2 window and AOP MSGRAM as PEP0 memory
        # resources. Replace 4 PEP0 interrupts unused by QualcommPEP (0xA1,0x1A5,0x40,0x41;
        # 36 bytes) with 3 Memory32Fixed descriptors (36 bytes) - same length, in place.
        import struct
        ints = b''.join(b'\x89\x06\x00' + bytes([f]) + b'\x01' + struct.pack('<I', v)
                        for f, v in [(0x01, 0xA1), (0x03, 0x1A5), (0x01, 0x40), (0x01, 0x41)])
        mem = b''.join(b'\x86\x09\x00\x01' + struct.pack('<II', a, n)
                       for a, n in [(0x18220000, 0x10000), (0x0C300000, 0x400), (0x0C300400, 0xC00)])
        assert len(ints) == len(mem) == 36 and bytes(d).count(ints) == 1, 'PEP0 _CRS pattern not found'
        d = bytearray(bytes(d).replace(ints, mem))
        print('PEP0 _CRS: 4 interrupts -> RSC DRV2 0x18220000 + AOP MSGRAM 0x0C300000 memory')
    if '--pipa-bklt' in sys.argv:
        # Append Scope(\\_SB){Device(BKLT) _HID PIPA8866, I2C 0x11 @400k on \\_SB.IC10} (KTZ8866 backlight).
        # AML compiled from acpi/pipa_bklt.asl; top-level terms may be appended to a DSDT.
        bk = bytes.fromhex('1045055c5f53425f5b824c04424b4c54085f4849440d504950413838363600085f5549440114095f53544100a40a0f085f43525311210a1e8e1900020001020000010600801a060011005c5f53422e49433130007900')
        assert b'PIPA8866' not in bytes(d)
        d = bytearray(bytes(d) + bk); d[4:8] = len(d).to_bytes(4, 'little')
        # pipa fsa4480 sits at 0x42 (DTB), Kona MTP CFSA says 0x43
        fsa = b'\xa0\x86\x01\x00\x43\x00\\_SB.I2C5'
        assert bytes(d).count(fsa) == 1, 'CFSA i2c'
        i = bytes(d).index(fsa); d[i + 4] = 0x42
        print('pipa: added \\_SB.BKLT (KTZ8866 @IC10:0x11); CFSA 0x43 -> 0x42')
    d[9] = 0
    d[9] = (-sum(d)) & 0xFF
    assert sum(d) & 0xFF == 0 and len(d) == int.from_bytes(d[4:8], 'little')
    open(dst, 'wb').write(d)
    print(f'wrote {dst} ({len(d)} bytes, checksum ok)')

if __name__ == '__main__':
    main()
