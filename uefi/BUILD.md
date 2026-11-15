# Building the pipa UEFI

```bash
git clone --recursive https://github.com/Project-Silicium/Mu-Silicium.git
cd Mu-Silicium            # tested at fb4efb37b4ac289ce83ad94c7acb73defb87955c (2026-11-12)
git apply /path/to/woa-pipa/uefi/patches/0001-pipa-fix-boot-ram-manager.patch

# Kona-ID ACPI (pipa_k)
mkdir -p Silicium-ACPI/Platforms/Xiaomi/pipa_k
python3 /path/to/woa-pipa/acpi/make_pipa_k.py \
    Silicium-ACPI/Platforms/Xiaomi/pipa/DSDT.aml Silicium-ACPI/Platforms/Xiaomi/pipa_k/DSDT.aml
cp /path/to/woa-pipa/acpi/pipa_k/AcpiTables.inf Silicium-ACPI/Platforms/Xiaomi/pipa_k/
sed -i 's|ACPITABLE pipa/AcpiTables.inf|ACPITABLE pipa_k/AcpiTables.inf|' Platforms/Xiaomi/pipaPkg/pipa.fdf

python3 build_uefi.py -d pipa            # or: -r DEBUG for an on-screen boot log
fastboot boot Mu-pipa.img                # temporary boot, nothing is flashed
```

Silicium-ACPI tested at `8c4cdc43abf23cffdeb588621126253bb2c559da`.

## Why the patch is needed
Upstream commit `8c321aab` ("Update RamManagerDxe", 2026-05-21) moved the RAM manager out of
pipa's APRIORI list. Qualcomm's `TzDxe`/`ShmBridgeDxe` then run with only the 51 MB DXE heap,
fail with `Out of Resources` / translation faults, and the device shows a black screen and reboots.
The patch loads `RamManagerDxe` right after `SmemDxe` (RAM partitions come from SMEM), marks
the `QTEE`/`TZApps` TrustZone regions as reserved (as on alioth), and restores the `CPU Vectors`
region expected by `EnvDxe`.

Bisect: tags 2.3, v3.1, v3.2, v3.4 boot; `a72d7eb4`/`96615491` grey screen; `8c321aab`, v3.6,
v4.0, main black → main + patch boots Windows.

## Restricted build environments
If gitlab.com or the Azure NuGet feed is blocked: init submodules shallowly and skip
`cmocka`/`subhook`, skip `--setup`, and disable the `mu_nasm` ext_dep (not used for AARCH64).
