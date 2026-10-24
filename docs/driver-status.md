# Driver and Hardware Support Tracker

All entries begin as **Unknown** until evidence is added. Do not imply that a driver exists or works just because the hardware component is present.

| Component | Initial status | Evidence / notes |
|---|---|---|
| UEFI boot | Unknown | Research required |
| Internal storage | Unknown | Windows-compatible path and driver requirements need investigation |
| USB | Unknown | Controller and port behaviour need investigation |
| Touch input | Unknown | Driver and device interface unknown |
| Display | Unknown | Panel initialization and display pipeline need investigation |
| Adreno 650 graphics | Researching | Focus initially on basic desktop usability; Windows driver compatibility is unverified |
| Wi-Fi | Unknown | Chipset, firmware, and driver requirements need confirmation |
| Bluetooth | Unknown | Driver and firmware requirements need confirmation |
| Audio | Unknown | Codec, routing, and driver requirements need confirmation |
| Battery / charging | Unknown | ACPI and power-management requirements need investigation |
| Sensors | Unknown | Hardware interfaces and driver availability need confirmation |
| Sleep / resume | Unknown | Requires later end-to-end testing |

## Updating an entry

Include test date, device variant, software/build identifiers, procedure, observed result, logs, and known limitations. Use **Verified** only after a result has been reproduced on the actual target device.
