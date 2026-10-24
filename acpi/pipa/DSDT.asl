/*
 * Xiaomi Pad 6 (pipa) — initial ACPI DSDT bring-up
 *
 * This file is authored for the pipa project rather than copied from the
 * existing Mu-Qcom DSDT. It intentionally starts with the UFS host only.
 *
 * IMPORTANT:
 * The UFS register window and GSI below match the SM8250 lmi reference
 * implementation. They are NOT yet independently verified against pipa's
 * live DT/firmware. Do not integrate into a production firmware until the
 * resource audit and AML/runtime validation pass.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
DefinitionBlock ("", "DSDT", 0x02, "XIAOMI", "PIPA0001", 0x00000001)
{
    Scope (_SB)
    {
        Device (UFS0)
        {
            Name (_HID, "QCOM24A5")
            Name (_UID, Zero)
            Name (_CCA, Zero)

            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0x01D84000,
                        0x0001C000
                    )

                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, , , )
                    {
                        0x00000129
                    }
                })

                Return (RBUF)
            }

            Device (DEV0)
            {
                Name (_ADR, 0x08)
                Method (_RMV, 0, NotSerialized)
                {
                    Return (Zero)
                }
            }
        }
    }
}
