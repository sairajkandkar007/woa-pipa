# Pipa live device-tree ACPI resource audit

Date: 2026-10-24  
Target: Xiaomi Pad 6 (pipa), Qualcomm SM8250 / Snapdragon 870  
Input: user-provided live device-tree export, `/proc/interrupts`, `/proc/iomem`, kernel config, DTB and dmesg.

## Result

The DSDT now exposes the UFS host as an **experimental ACPI device** using the two UFS host resources recorded in the live tree. This is a source/build integration step only, not proof that Windows can initialize or boot from the controller. The GSI convention and the hardware dependency path remain open validation items.

### UFS host: `/soc/ufshc@1d84000`

Observed in the live tree:

- `compatible`: `qcom,ufshc`
- `status`: `ok`
- `reg-names`: `ufs_mem ufs_ice`
- `reg` cells: `0x01D84000 0x3000`, `0x01D90000 0x8000`
- `interrupts` cells: `0x0 0x109 0x4`
- `phy-names`: `ufsphy`
- `clock-names`: `core_clk bus_aggr_clk iface_clk core_clk_unipro core_clk_ice ref_clk tx_lane0_sync_clk rx_lane0_sync_clk rx_lane1_sync_clk`
- `reset-names`: `core_reset`

The first MMIO region is 12 KiB. The second named resource is the UFS ICE window at 0x01D90000, length 0x8000.

The interrupt specifier follows the GIC device-tree format: type 0 means SPI, interrupt ID 0x109 is 265 decimal, and flags 0x4 mean level-triggered. The captured Linux interrupt listing reports IRQ 297, consistent with the conventional SPI-to-Linux-IRQ offset of 32. The ASL uses `0x129` as the candidate ACPI GSI. Its correctness, trigger, and polarity still need validation against the target platform's GIC/ACPI setup.

### UFS PHY: `/soc/ufsphy_mem@1d87000`

Observed:

- `compatible`: `qcom,ufs-phy-qmp-v4`
- `status`: `ok`
- `reg-names`: `phy_mem ufs_ice`
- `reg` cells: `0x01D87000 0xE00`, `0x01D90000 0x8000`
- `clock-names`: `ref_clk_src ref_aux_clk`

The host references this PHY via a DT phandle. PHY initialization, clock dependencies, and shared ICE-window ownership need driver/binding review before creating a Windows-visible PHY device.

### Separate card UFS controller

`/soc/ufshc_card@1da4000` is marked `disabled` in the live tree. It remains absent from the DSDT.

### Other ICE node

`/soc/ufscardice@1db0000` reports `compatible = qcom,ice`, register range `0x01DB0000` length `0x8000`, and clocks `ufs_core_clk bus_clk iface_clk ice_core_clk`. Do not conflate this separate node with the host's named `ufs_ice` region without confirming how the vendor driver uses both.

## What the experimental DSDT currently exposes

- UFS host HID `QCOM24A5`, UID 0.
- MMIO region `0x01D84000`, length `0x3000`.
- ICE region `0x01D90000`, length `0x8000`.
- Candidate level/high interrupt GSI `0x129` (297 decimal).

It does **not** yet model the PHY, clock providers, reset controller, or power sequencing. It also does not prove that `QCOM24A5` is the correct ACPI ID for a compatible Windows UFS driver. The device is visible in ACPI, but that alone does not make the storage controller operational.

## Remaining validation

1. Identify the Windows UFS driver and verify its ACPI ID and resource contract.
2. Resolve all named host/PHY clock provider IDs, rates, sequencing, and power states.
3. Resolve the `core_reset` provider and required sequencing.
4. Confirm whether PHY initialization is performed by firmware or must be exposed/controlled by a Windows driver.
5. Validate the GSI, trigger, and polarity against the target firmware's APIC/GIC tables.
6. Compile/decompile the AML and inspect the final table set. IASL validation is syntax validation only; hardware testing is a separate step.

## References

- [Xiaomi kernel source repository, pipa-t-oss branch](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)
- [Linux GICv3 device-tree binding](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/interrupt-controller/arm%2Cgic-v3.yaml)
- [Linux Qualcomm UFS binding](https://code.googlesource.com/linux/torvalds/linux/%2B/b8910630c967ffee582289451ddb5f9f19c26872/Documentation/devicetree/bindings/ufs/qcom%2Cufs.yaml)
