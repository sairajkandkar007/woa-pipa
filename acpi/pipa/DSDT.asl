/*
 * Xiaomi Pad 6 (pipa) — ACPI resource-audit scaffold
 *
 * This is authored from scratch, but it is NOT a usable UFS ACPI device yet.
 * The live pipa device tree reports:
 *   ufs_mem: 0x01D84000, length 0x3000
 *   ufs_ice: 0x01D90000, length 0x8000
 *   GIC specifier: <0 0x109 4>
 * See docs/acpi/PIPA_LIVE_DT_RESOURCE_AUDIT.md.
 *
 * The previous 0x1C000 window and 0x129 interrupt were donor-derived and
 * contradicted by the live DT. This scaffold deliberately keeps UFS hidden
 * until the Windows ACPI resource model, GIC mapping, PHY, clocks and reset
 * dependencies are verified. Do not integrate or flash as production firmware.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
DefinitionBlock ("", "DSDT", 0x02, "XIAOMI", "PIPA0001", 0x00000002)
{
    Scope (_SB)
    {
        Device (UFS0)
        {
            Name (_HID, "QCOM24A5")
            Name (_UID, Zero)
            Name (_CCA, Zero)

            // Keep disabled until all ACPI resources are verified.
            Method (_STA, 0, NotSerialized)
            {
                Return (Zero)
            }

            // No _CRS is exposed while this device is disabled.
        }
    }
}
