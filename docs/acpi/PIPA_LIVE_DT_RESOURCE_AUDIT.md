# Pipa live device-tree ACPI resource audit

Date: 2026-10-24  
Target: Xiaomi Pad 6 (pipa), Qualcomm SM8250 / Snapdragon 870  
Input: user-provided live device-tree export, `/proc/interrupts`, `/proc/iomem`, kernel config, DTB and dmesg.

## Result

The live tree verifies the UFS controller's first MMIO region and exposes additional resources that the initial DSDT omitted. The DSDT remains disabled until the complete ACPI representation and Windows driver expectations are established.

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

The first MMIO region is 12 KiB, not the draft's 0x1C000 bytes. The second named resource is the UFS ICE window at 0x01D90000, length 0x8000.

The interrupt specifier follows the GIC device-tree format: type 0 means SPI, interrupt ID 0x109 is 265 decimal, and flags 0x4 mean level-triggered. The captured Linux interrupt listing reports IRQ 297, consistent with the conventional SPI-to-Linux-IRQ offset of 32. The old ASL value `0x129` is 297 decimal, so it is consistent with that translation and must **not** be described as disproven. However, the correct ACPI interrupt resource still needs validation against the target platform's GIC/ACPI setup and polarity semantics.

### UFS PHY: `/soc/ufsphy_mem@1d87000`

Observed:

- `compatible`: `qcom,ufs-phy-qmp-v4`
- `status`: `ok`
- `reg-names`: `phy_mem ufs_ice`
- `reg` cells: `0x01D87000 0xE00`, `0x01D90000 0x8000`
- `clock-names`: `ref_clk_src ref_aux_clk`

The UFS host references this PHY via a DT phandle. PHY initialization, clock dependencies, and shared ICE-window ownership need driver/binding review before creating an ACPI device for it.

### Separate card UFS controller

`/soc/ufshc_card@1da4000` is marked `disabled` in the live tree. Do not expose it as enabled based on this snapshot alone.

### Other ICE node

`/soc/ufscardice@1db0000` reports `compatible = qcom,ice`, register range `0x01DB0000` length `0x8000`, and clocks `ufs_core_clk bus_clk iface_clk ice_core_clk`. Do not conflate this separate node with the host's named `ufs_ice` region without confirming how the vendor driver uses both.

## Required changes to the scaffold

The original DSDT used:

- UFS MMIO base `0x01D84000`, length `0x1C000` — contradicted by the live tree; the first resource is length `0x3000`.
- Interrupt value `0x129` — consistent with Linux IRQ 297 if the SPI-to-IRQ offset is 32; not independently proven as the final ACPI GSI resource.
- No explicit second ICE resource, PHY, clocks, or reset dependency.

The DSDT is currently intentionally disabled. Do not integrate or flash it as production firmware. The register resources must be modeled according to the ACPI resource model expected by the Windows UFS driver, and dependencies must be addressed rather than silently omitted.

## Next source audit

1. Trace pipa's exact UFS DT node through its binding and the exact kernel driver revision.
2. Resolve clock-controller phandles and reset-controller phandle for all named UFS clocks and `core_reset`.
3. Verify the Windows UFS driver resource contract, including controller/ICE resources and PHY initialization ownership.
4. Confirm GIC-to-ACPI interrupt mapping and trigger/polarity.
5. Only then implement and enable the UFS device. Keep other devices absent until independently audited.

## Validation boundary

This report records values present in the user's live device tree. It is a resource inventory, not a claim that Windows has a compatible driver or that an ACPI table will boot. IASL success validates syntax/AML generation only; it does not validate hardware behavior.

## References

- [Xiaomi kernel source repository, pipa-t-oss branch](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss)
- [Linux GICv3 device-tree binding](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/interrupt-controller/arm%2Cgic-v3.yaml)
- [Linux Qualcomm UFS binding](https://code.googlesource.com/linux/torvalds/linux/%2B/b8910630c967ffee582289451ddb5f9f19c26872/Documentation/devicetree/bindings/ufs/qcom%2Cufs.yaml)
