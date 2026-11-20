DefinitionBlock ("", "SSDT", 2, "PIPA", "WPWR", 1)
{
    Scope (\_SB)
    {
        Device (WPWR)
        {
            Name (_HID, "PIPA6390")
            Name (_UID, One)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            // uart6 (QUP0 SE6) interrupt, GIC SPI 607: the BT HCI UART driven by pipapwr
            Name (_CRS, ResourceTemplate ()
            {
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 0x0000027F }
            })
        }
    }
}
