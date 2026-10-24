# Mu-Qcom dependency plan

The selected pipa platform files are stored in `third_party/mu-qcom-pipa-snapshot/`. This is not a complete EDK II / Project Mu workspace.

## Direct dependencies declared by upstream

These URLs are copied from the upstream `.gitmodules` file. The source repository's gitlink revisions must be used when assembling a reproducible workspace; do not blindly use each dependency's latest default branch.

| Path | Repository |
|---|---|
| `Binaries` | https://github.com/Robotix22/Device-Binaries |
| `Common/Mu` | https://github.com/microsoft/mu_plus |
| `Common/Mu_OEM_Sample` | https://github.com/microsoft/mu_oem_sample |
| `Common/Mu_Tiano_Plus` | https://github.com/microsoft/mu_tiano_plus |
| `Features/DFCI` | https://github.com/microsoft/mu_feature_dfci |
| `Mu_Basecore` | https://github.com/microsoft/mu_basecore |
| `Silicon/Arm/Mu_Tiano` | https://github.com/microsoft/mu_silicon_arm_tiano |

The platform build script also references `Silicon/Qualcomm`, which is not listed as a direct submodule in the copied `.gitmodules`; inspect the upstream repository tree and build setup to establish where that package comes from.

## Known blockers

- The first recursive submodule attempt failed because a nested `Zeex/subhook` repository could not be found.
- A failed recursive clone does not prove the pipa platform files are invalid. It means dependency resolution needs to be handled deliberately.
- Some upstream nested submodules may be optional for this target, but that must be established from the build graph rather than assumed.
- Binary resources such as `ImageResources/DTBs/pipa.dtb` and `ImageResources/ramdisk` have not yet been copied into this snapshot.
- The source snapshot's build scripts refer to generated output under `Build/pipaPkg`; those artifacts do not exist yet.

## Next action

Use a reproducible source-assembly workflow to:
1. Resolve every direct submodule to the exact gitlink SHA recorded by the upstream Mu-Qcom commit.
2. Fetch each direct dependency at that SHA without recursively initializing unrelated nested submodules.
3. Record nested submodule references and fetch failures.
4. Check for required DTB and ramdisk resources.
5. Produce a source bundle and audit report before attempting compilation.

Do not build or flash until the required workspace inputs and their provenance are verified.
