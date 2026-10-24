# UFS ACPI dependency ledger — Xiaomi Pad 6 (pipa)

Status: research in progress; no UFS ACPI device enabled.

## Source hierarchy

1. **Live device-tree export from this tablet** — primary evidence for the running board's resources.
2. **pipa-specific DTS and the exact kernel revision used by the tablet** — needed to resolve phandles and provider properties.
3. **Generic Linux binding/driver source** — explains DT semantics, but does not establish the Windows ACPI driver contract.
4. **Other SM8250 device tables** — reference only; not accepted as resource evidence.

The upstream pipa DTS identifies the board as `xiaomi,pipa` / `qcom,sm8250`. See the [upstream pipa DTS](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/qcom/sm8250-xiaomi-pipa.dts). The user's running device tree remains the authority for the exact currently active resource values.

## Resource ledger

| Item | Live-tree evidence | Confidence | Still required |
|---|---|---:|---|
| UFS host identity | `/soc/ufshc@1d84000`, `qcom,ufshc`, status `ok` | High | Confirm Windows driver compatible/HID contract |
| Host MMIO | `ufs_mem`: base `0x01D84000`, length `0x3000` | High | Confirm ACPI resource type and address width |
| ICE resource | `ufs_ice`: base `0x01D90000`, length `0x8000` | High | Confirm whether host driver needs this region as a separate resource |
| Interrupt | DT cells `<0 0x109 4>`; Linux IRQ 297 observed | High for DT; medium for ACPI mapping | Validate GIC/ACPI GSI mapping, polarity and trigger |
| PHY identity | `/soc/ufsphy_mem@1d87000`, `qcom,ufs-phy-qmp-v4`, status `ok` | High | Determine if PHY is initialized by firmware or a Windows-visible device |
| PHY MMIO | `phy_mem`: base `0x01D87000`, length `0xE00` | High | Confirm ownership and access path |
| PHY's ICE-named region | `0x01D90000`, length `0x8000` | High as DT property | Resolve overlap/ownership with host's ICE resource |
| PHY clocks | `ref_clk_src`, `ref_aux_clk` | High for names | Resolve clock provider IDs/rates and who enables them |
| Host clocks | `core_clk bus_aggr_clk iface_clk core_clk_unipro core_clk_ice ref_clk tx_lane0_sync_clk rx_lane0_sync_clk rx_lane1_sync_clk` | High for names | Resolve providers, rates, sequencing, and power states |
| Host reset | `core_reset` | High for name | Resolve reset provider and required sequencing |
| PHY link | host `phy-names = ufsphy` and a DT phandle | High | Resolve target and expected initialization sequence |
| Card UFS host | `/soc/ufshc_card@1da4000`, status `disabled` | High | Keep disabled unless board/runtime evidence changes |
| Separate card ICE | `/soc/ufscardice@1db0000`, `qcom,ice`, range `0x01DB0000` / `0x8000` | High for DT properties | Determine relationship to internal UFS and Windows needs |

## ACPI implementation gate

Do not change UFS `_STA` to present until all of the following are resolved:

- A real Windows UFS controller driver is identified and its ACPI device ID/resource contract is documented.
- The controller MMIO and ICE resource requirements are known.
- The interrupt is encoded according to the target firmware's GIC ACPI conventions, not copied mechanically from DT or Linux IRQ numbering.
- PHY initialization ownership and required clock/reset/power sequencing are implemented by a defined component.
- The resulting AML compiles and decompiles cleanly, and table composition is reviewed before any device testing.

## Source-inspection note

The public device-specific tree is useful for board identity and overlays. A kernel driver file lookup can fail when the branch/path differs from the Android kernel layout; a failed lookup is not evidence that a driver or feature is absent. Before claiming exact driver behavior, pin the actual kernel repository, branch and file paths used to build the tablet's running kernel.

## Current safe state

The project DSDT keeps `UFS0` hidden (`_STA = Zero`) and contains no `_CRS` for it. Preserve that state until the gates above are met. The IASL workflow is a syntax check only and must not be described as hardware validation.



## Kernel-source trace (SD870 Android kernel)

Inspected source from the public `SD870/kernel_xiaomi_sm8250` repository, branch `17.rksu-teo`:

- [`drivers/scsi/ufs/ufshcd-pltfrm.c`](https://github.com/SD870/kernel_xiaomi_sm8250/blob/17.rksu-teo/drivers/scsi/ufs/ufshcd-pltfrm.c) is the platform glue driver. It obtains the first `IORESOURCE_MEM` entry and maps it with `devm_ioremap_resource()`. This supports treating the controller's first MMIO region as a required resource, but it does not establish the Windows ACPI ID.
- The same source obtains the `core_reset` reset control by name. Its clock parsing reads device-tree properties (including `clock-names` and `freq-table-hz`) through `dev->of_node`; this Linux driver's DT setup cannot simply be translated into ACPI resources without an explicit replacement for clock/reset configuration.
- [`drivers/phy/qualcomm/phy-qcom-ufs-qmp-v4.c`](https://github.com/SD870/kernel_xiaomi_sm8250/blob/17.rksu-teo/drivers/phy/qualcomm/phy-qcom-ufs-qmp-v4.c) matches `qcom,ufs-phy-qmp-v4` and delegates probing to `ufs_qcom_phy_generic_probe()`. The PHY has explicit init/exit and power-on/power-off operations, so merely exposing the PHY's MMIO window is not proof that it will be initialized for Windows.

### Consequence for the ACPI design

The Linux source confirms this is not a controller-only MMIO declaration problem. A viable Windows boot-storage path needs a compatible Windows UFS driver and a proven way to establish the controller's clock, reset, power, and PHY state. These Linux source findings are architectural evidence only; they do not supply the Windows driver contract.

The source repository's current default branch is Android-oriented and differs from the tablet's original `pipa-t-oss` kernel lineage. Therefore, treat these findings as useful Qualcomm implementation clues, not final proof of the exact firmware currently running on the tablet. The next source pin should record the tablet's kernel build fingerprint/version and identify the matching source revision.
