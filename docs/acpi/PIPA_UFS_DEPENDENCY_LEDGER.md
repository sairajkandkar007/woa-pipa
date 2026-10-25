# UFS ACPI dependency ledger — Xiaomi Pad 6 (pipa)

Status: UFS controller is exposed in the experimental DSDT; hardware functionality is unverified.

## Source hierarchy

1. **Live device-tree export from this tablet** — primary evidence for the running board's resources.
2. **pipa-specific DTS and the exact kernel revision used by the tablet** — needed to resolve phandles and provider properties.
3. **Generic Linux binding/driver source** — explains DT semantics, but does not establish the Windows ACPI driver contract.
4. **Other SM8250 device tables** — reference only; not accepted as resource evidence.

The upstream pipa DTS identifies the board as `xiaomi,pipa` / `qcom,sm8250`. See the [upstream pipa DTS](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/qcom/sm8250-xiaomi-pipa.dts). The user's running device tree remains the authority for exact currently active resource values.

## Resource ledger

| Item | Live-tree evidence | Confidence | Remaining work |
|---|---|---:|---|
| UFS host identity | `/soc/ufshc@1d84000`, `qcom,ufshc`, status `ok` | High | Verify Windows ACPI ID/driver contract |
| Host MMIO | `ufs_mem`: `0x01D84000`, length `0x3000` | High | Confirm expected ACPI resource representation |
| ICE resource | `ufs_ice`: `0x01D90000`, length `0x8000` | High | Confirm Windows driver expects a separate resource |
| Interrupt | DT cells `<0 0x109 4>`; Linux IRQ 297 observed | High for DT; medium for ACPI | Verify GSI mapping, trigger, and polarity |
| PHY identity | `/soc/ufsphy_mem@1d87000`, `qcom,ufs-phy-qmp-v4`, status `ok` | High | Establish PHY initialization ownership |
| PHY MMIO | `phy_mem`: `0x01D87000`, length `0xE00` | High | Confirm ownership/access path |
| PHY ICE-named region | `0x01D90000`, length `0x8000` | High as DT property | Resolve shared-window ownership |
| PHY clocks | `ref_clk_src`, `ref_aux_clk` | High for names | Resolve providers, rates, sequencing |
| Host clocks | `core_clk bus_aggr_clk iface_clk core_clk_unipro core_clk_ice ref_clk tx_lane0_sync_clk rx_lane0_sync_clk rx_lane1_sync_clk` | High for names | Resolve providers, rates, sequencing, power states |
| Host reset | `core_reset` | High for name | Resolve provider and sequencing |
| PHY link | host `phy-names = ufsphy` and DT phandle | High | Resolve target and initialization sequence |
| Card UFS host | `/soc/ufshc_card@1da4000`, status `disabled` | High | Keep absent unless evidence changes |
| Separate card ICE | `/soc/ufscardice@1db0000`, range `0x01DB0000` / `0x8000` | High for DT properties | Determine relationship to internal UFS and Windows needs |

## Current ACPI representation

The experimental DSDT exposes `UFS0` (`QCOM24A5`) with the two audited host MMIO windows and candidate level/high GSI `0x129`. The ID and GSI are not yet confirmed against a Windows driver and the target firmware's ACPI/GIC contract. Clock, reset, PHY, and power dependencies are not modeled. Treat this as an enabled discovery prototype, not a usable boot-storage implementation.

## Required follow-up

- Identify a real Windows UFS controller driver and verify its ACPI ID/resource contract.
- Confirm whether the controller requires the ICE window as a separate ACPI memory resource.
- Validate the interrupt GSI, trigger, and polarity against the APIC/GIC tables.
- Resolve clock provider IDs/rates, reset provider, sequencing, and power states.
- Establish whether PHY initialization is performed by firmware or needs a Windows-visible device/driver.
- Compile/decompile AML and inspect final table composition. Passing IASL validates syntax and AML parsing only, not hardware behavior.

## Source-inspection note

The public device-specific tree is useful for board identity and overlays. A kernel driver file lookup can fail when the branch/path differs from the Android kernel layout; a failed lookup is not evidence that a driver or feature is absent. Pin the tablet's kernel build fingerprint/version and identify the matching source revision before claiming exact driver behavior.

## Kernel-source trace (SD870 Android kernel)

Inspected source from the public `SD870/kernel_xiaomi_sm8250` repository, branch `17.rksu-teo`:

- [`drivers/scsi/ufs/ufshcd-pltfrm.c`](https://github.com/SD870/kernel_xiaomi_sm8250/blob/17.rksu-teo/drivers/scsi/ufs/ufshcd-pltfrm.c) obtains the first `IORESOURCE_MEM` entry and maps it with `devm_ioremap_resource()`. It also obtains `core_reset` by name and reads clock configuration from device-tree properties.
- [`drivers/phy/qualcomm/phy-qcom-ufs-qmp-v4.c`](https://github.com/SD870/kernel_xiaomi_sm8250/blob/17.rksu-teo/drivers/phy/qualcomm/phy-qcom-ufs-qmp-v4.c) matches `qcom,ufs-phy-qmp-v4` and delegates probing to `ufs_qcom_phy_generic_probe()`. PHY init/exit and power-on/power-off operations mean that exposing its MMIO alone does not prove it will be initialized for Windows.

These sources are architectural clues, not proof of the exact kernel revision running on the tablet or a Windows driver contract.
