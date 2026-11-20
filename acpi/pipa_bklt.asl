DefinitionBlock ("", "SSDT", 2, "PIPA", "BKLT", 1)
{
    Scope (\_SB)
    {
        Device (BKLT)
        {
            Name (_HID, "PIPA8866")
            Name (_UID, One)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_CRS, ResourceTemplate ()
            {
                I2cSerialBusV2 (0x0011, ControllerInitiated, 0x00061A80,
                    AddressingMode7Bit, "\\_SB.IC10", 0x00, ResourceConsumer, , Exclusive, )
            })
        }
    }
}
