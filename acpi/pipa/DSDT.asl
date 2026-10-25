/*
 * Xiaomi Pad 6 (pipa) — experimental UFS ACPI device
 *
 * Source evidence: docs/acpi/PIPA_LIVE_DT_RESOURCE_AUDIT.md
 *
 * The controller is exposed for build-time experimentation with the audited
 * host MMIO and ICE windows. The interrupt GSI 0x129 follows the captured
 * Linux IRQ 297 mapping, but its final ACPI GSI/polarity convention still
 * requires platform validation. Clock/reset/PHY sequencing and the Windows
 * driver contract are not yet implemented or verified. This is NOT production
 * firmware and must not be flashed as a working UFS implementation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
DefinitionBlock ("", "DSDT", 0x02, "XIAOMI", "PIPA0001", 0x00000003)
{
    Scope (_SB)
    {
        Device (UFS0)
        {
            Name (_HID, "QCOM24A5")
            Name (_UID, Zero)
            Name (_CCA, Zero)

            // Expose the controller while the full driver/dependency path
            // is being developed. This does not imply that Windows can use it.
            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    // Live DT: ufs_mem, base 0x01D84000, length 0x3000.
                    Memory32Fixed (ReadWrite, 0x01D84000, 0x00003000)

                    // Live DT: ufs_ice, base 0x01D90000, length 0x8000.
                    Memory32Fixed (ReadWrite, 0x01D90000, 0x00008000)

                    // Captured Linux IRQ 297 (0x129); ACPI GSI mapping still
                    // needs validation on the target ACPI/GIC implementation.
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,,)
                    {
                        0x00000129
                    }
                })
                Return (RBUF)
            }
        }
    }
}
