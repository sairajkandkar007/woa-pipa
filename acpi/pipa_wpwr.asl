DefinitionBlock ("", "SSDT", 2, "PIPA", "WPWR", 1)
{
    Scope (\_SB)
    {
        Device (WPWR)
        {
            Name (_HID, "PIPA6390")
            Name (_UID, One)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}
