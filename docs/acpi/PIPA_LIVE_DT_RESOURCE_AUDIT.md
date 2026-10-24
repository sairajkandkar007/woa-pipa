# Pipa live device-tree ACPI resource audit

Date: 2026-10-24  
Target: Xiaomi Pad 6 (pipa), Qualcomm SM8250 / Snapdragon 870  
Input: user-provided live device-tree export, `/proc/interrupts`, `/proc/iomem`, kernel config, DTB and dmesg.

## Result

The live device tree contradicts the original draft's UFS resources. The draft must remain a non-production scaffold until the ASL is corrected and its interrupt/controller dependencies are modeled.

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

The first register resource is therefore 12 KiB, not the draft's 0x1C000 bytes. The second resource is the UFS ICE window at 0x01D90000, length 0x8000. The DT interrupt specifier is a three-cell GIC specifier; it must not be copied as an ACPI GSI without interpreting the interrupt-controller binding. The captured Linux interrupt listing reports IRQ 297 for the UFS host, consistent with an SPI ID of 265 plus the Linux GIC offset of 32, but the ACPI interrupt encoding still requires platform-level validation.

### UFS PHY: `/soc/ufsphy_mem@1d87000`

Observed:

- `compatible`: `qcom,ufs-phy-qmp-v4`
- `status`: `ok`
- `reg-names`: `phy_mem ufs_ice`
- `reg` cells: `0x01D87000 0xE00`, `0x01D90000 0x8000`
- `clock-names`: `ref_clk_src ref_aux_clk`

The UFS host references this PHY via the DT phandle. PHY initialization, clock dependencies and ownership of the shared ICE window need driver/binding review before an ACPI device is described.

### Separate card UFS controller

`/soc/ufshc_card@1da4000` is marked `disabled` in the live tree. It must not be exposed as an enabled Windows device based on this snapshot alone.

### Other ICE node

`/soc/ufscardice@1db0000` reports `compatible = qcom,ice`, register range `0x01DB0000` length `0x8000`, and clocks `ufs_core_clk bus_clk iface_clk ice_core_clk`. This is a separate node and should not be conflated with the UFS host's `ufs_ice` resource.

## Required changes to the scaffold

Current `acpi/pipa/DSDT.asl` still declares:

- UFS MMIO base `0x01D84000`, length `0x1C000`
- Interrupt value `0x129`

Both values are unsupported by the live-tree evidence as currently encoded. Do not use that DSDT for firmware integration or flashing. The register resource needs to be represented using the ACPI resource model selected for the Windows UFS driver; the DT's second named resource, PHY, clocks, reset, and GIC mapping also need an intentional design rather than being silently dropped.

## Next source audit

1. Trace the pipa UFS DT node through its binding and the exact kernel driver revision used by the device.
2. Resolve clock-controller phandles and reset-controller phandle for all named UFS clocks and `core_reset`.
3. Verify how the Windows UFS driver expects controller and ICE resources, and whether the PHY is modeled as a separate ACPI device or initialized by platform firmware.
4. Establish the correct GIC-to-ACPI interrupt mapping and polarity/trigger semantics.
5. Only then replace the provisional UFS ASL. Keep other devices absent until independently audited.

## Validation boundary

This report records values present in the user's live device tree. It is a resource inventory, not a claim that Windows has a compatible driver or that an ACPI table will boot. IASL success validates syntax/AML generation only; it does not validate hardware behavior.
