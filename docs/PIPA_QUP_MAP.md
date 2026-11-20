# pipa QUP (serial engine) map — from the pipa Android DTB

Source: `boot.img` DTB of the tablet (`model = "Qualcomm Technologies, Inc. xiaomi pipa"`, msm-id 0x164, board-id 0x34),
decompiled with dtc. Only SEs with `status = "ok"` are used by the HLOS on pipa.

| Wrapper / SE | Address | Linux name | Mode | Devices on it |
|---|---|---|---|---|
| QUP0 SE1 | 0x984000 | i2c1 | I2C | NFC `nq@28`, smart PA `aw882xx@36`, `@37` |
| QUP0 SE2 | 0x988000 | i2c2 | I2C | `nanosic@4c` (keyboard-cover MCU) |
| QUP0 SE3 | 0x98C000 | i2c3 | I2C | smart PA `aw882xx@34`, `@35` |
| **QUP0 SE4** | **0x990000** | **spi4** | **SPI** | **touch `novatek,NVT-ts-spi` (NT36532, pen), 9.6 MHz, IRQ GPIO 39, SE IRQ GIC SPI 0x25D** |
| QUP0 SE6 | 0x998000 | uart6 | UART | debug UART (`msm-geni-serial-hs`) |
| QUP1 SE0 | 0xA80000 | i2c8 | I2C | `nuvolta_1665@41` |
| QUP1 SE3 | 0xA8C000 | i2c11 | I2C | haptics `aw8697@5A`, **backlight `ktz8866@11`** |
| QUP2 SE1 | 0x884000 | i2c15 | I2C | **USB-C mux `fsa4480@42`**, `nq@64`, charge pumps `smb1390@10/@18`, `hl6111r@25` |
| QUP2 SE2 | 0x888000 | i2c16 | I2C | charge pump `bq2597x@66` |
| QUP2 SE3 | 0x88C000 | i2c17 | I2C | USB redriver `ps5169@28` |

Everything else (incl. QUP0 SE0/SE5/SE7, QUP1 SE1/2/4/5, QUP2 SE0/4/5, all other SPI modes) is **disabled** on pipa.

## Consequences for the Kona (MTP) DSDT
- Kona `I2C5` = QUP0 SE4 as **I2C** — on pipa that SE is the **touch SPI** -> wrong protocol.
- Kona `I2C8` = QUP0 SE7, `IC10` = QUP1 SE1 — disabled on pipa.
- Kona `SPI4` = QUP0 SE3 as SPI — on pipa that SE is **I2C** (amps).
- Installing qci2c/qcspi with the MTP set hung a CPU (0x101) on 2026-11-19.

## DSDT work (next)
1. Remove/disable the MTP QUP devices (I2C5, I2C8, IC10, SPI4) and any children.
2. Add pipa buses with PEP descriptors (clock = SE clock + wrapper m_ahb, ICB QUP_x paths, TLMM pins from the DTB pinctrl):
   - SPI on QUP0 SE4 (touch) -> needs a Novatek NT36532 HID driver (none exists for Windows).
   - I2C11 on QUP1 SE3 (backlight KTZ8866, haptics).
   - I2C15 on QUP2 SE1 (FSA4480 USB-C mux, charge pumps) — relevant for USB-C role/VBUS and charging.
