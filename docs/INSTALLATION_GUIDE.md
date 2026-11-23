# Windows 11 ARM64 installation guide — Xiaomi Pad 6 (pipa)

> **Status: preparation guide, not a one-tap installer.** This document describes the repository's current workflow and clearly marks the parts that are not yet usable from the tablet alone. Do not start partitioning until the preflight and backup requirements below are met.

## 1. Scope and current limitations

Target: Xiaomi Pad 6 (**pipa**, Snapdragon 870 / SM8250-AC), Windows 11 ARM64, Project Silicium Mu-Silicium UEFI and the WOA-Project Kona BSP.

The scripts currently in `scripts/windows/` are **Windows-host scripts**. They expect a separate Windows PC with ADB/Fastboot, DISM, DiskPart, registry tools and the Windows ADK/servicing tools available. They are not Termux scripts.

The repository does **not yet provide a complete Pad-only installation path**. GitHub Actions can build supported source projects, but it does not by itself deploy a WIM to the tablet, create the ESP, flash UEFI or install Windows. A tablet-only workflow still needs to be implemented and tested.

The existing scripts also contain fixed assumptions for one particular storage layout. They must not be run against a different partition map.

## 2. Read these files first

- [Project overview](../README.md)
- [UEFI build instructions](../uefi/BUILD.md)
- [Hardware and bring-up status](STATUS_LOG.md)
- [Qualcomm PEP findings](PEP_FINDINGS.md)
- [Development roadmap](ROADMAP.md)
- [Windows host scripts](../scripts/windows/)

## 3. Requirements

### Tablet

- Xiaomi Pad 6 (pipa), bootloader unlocked.
- Working Android/recovery environment with root or a recovery shell.
- A known-good way to enter fastboot and recovery.
- Enough free storage for backups and Windows deployment files.
- A complete, verified backup of critical partitions and a tested restore route.

### Windows host (for the current scripts)

- Windows 10/11 PC with administrator rights.
- ADB and Fastboot at `C:\platform-tools\`.
- DISM, DiskPart, BCDEdit and Registry tools.
- Windows 11 ARM64 image exported as `install_pro.wim`.
- The WOA Kona BSP extracted so its `Drivers` directory is available.
- A compatible pipa Mu-Silicium UEFI image built using [uefi/BUILD.md](../uefi/BUILD.md).

**No-PC users:** stop here for now. The current install scripts cannot be run directly in Termux. Do not try to execute the PowerShell scripts through a Linux compatibility layer or substitute random Android partition commands. Wait until a separately documented and tested tablet-only installer is available.

## 4. Understand the storage layout before changing anything

The current `phase2_install.ps1` is hard-coded for this layout:

| Partition | Number | Expected size | Use |
|---|---:|---:|---|
| ESP | 34 | 1 GiB | FAT32 UEFI system partition |
| Windows | 35 | 224.7 GiB | NTFS Windows partition |

It also expects the whole UFS LUN0 to appear to the host as a USB disk with:

- Size: `253461790720` bytes
- Logical sector size: `4096` bytes
- GPT partition table

These are **script-specific expected values**, not universal pipa partition instructions. Your actual partition map must be read and compared before proceeding. Never create, move, resize, format or delete partitions based only on partition numbers copied from this guide.

## 5. Phase 0 — inventory and backups (read-only on the tablet)

The host-side wrapper is `RUN_PHASE0B.bat`, which calls `phase0b_verify.ps1`.

It reads partition tables and backs up selected partitions no larger than 512 MiB, then compares file sizes and MD5 hashes. It intentionally excludes large or sensitive partitions such as `userdata`, `super`, `cache`, `metadata`, and the proposed ESP/Windows partitions.

Important limitations:

- This is **not a complete whole-device backup**.
- Keep backups somewhere other than the tablet's internal storage.
- Verify that every backup reports `OK`; investigate every mismatch.
- Preserve the partition list and backup logs.
- Separately arrange a reliable full firmware/critical-partition restore path.

Do not continue if backups are incomplete or the partition table differs from the expected layout.

## 6. Phase 1 — build and test UEFI without flashing

Follow [uefi/BUILD.md](../uefi/BUILD.md) to build Mu-Silicium with the pipa-specific patch and generated `pipa_k` ACPI table.

The repository's documented first test uses:

```sh
fastboot boot Mu-pipa.img
```

This is a **temporary boot** and does not flash the image. Confirm that the UEFI display appears and that the device can proceed to the expected boot path before considering any permanent flash.

Do not flash an unverified UEFI image. The permanent-flash helper is a separate operation and should only be considered after the temporary boot and recovery path are proven.

## 7. Phase 2 — deploy Windows (current Windows-host path only)

The current host-side entry point is:

- `RUN_PHASE2_INSTALL.bat`
- `phase2_install.ps1`

The script expects `install_pro.wim` in the parent workspace and performs these operations:

1. Checks that the WIM exists and selects the Windows 11 Pro image index.
2. Looks for the tablet's whole UFS LUN0 exported over USB mass storage.
3. Checks the disk size, 4096-byte logical sectors, GPT layout, and exact p34/p35 sizes.
4. Requires the operator to type `YES`.
5. Formats **only p35 as NTFS and p34 as FAT32**, applies the WIM, writes the unattended setup file, and creates UEFI boot files with `bcdboot`.
6. Sets BCD options and removes temporary drive letters.

The script aborts when the expected disk or partition sizes do not match. Do not bypass those checks.

### Critical mass-storage warning

The Android helper `scripts/android/msc_sda.sh` exports the **entire** `/dev/block/sda` UFS LUN as a writable USB mass-storage device while also retaining ADB. This exposes the tablet's partition table and data to the host. Close Android apps that may write to storage and do not allow Windows to initialize, repair, or format any partition other than the two explicitly intended by the reviewed install script.

If Windows shows an unexpected disk-initialization or format prompt, cancel it. Never initialize the tablet disk.

## 8. Phase 3 — first UEFI boot

The current temporary boot helper is `RUN_PHASE3_BOOT.bat` / `phase3_boot.ps1`. It reboots to fastboot when ADB is available and runs `fastboot boot` on the selected UEFI image.

Watch the tablet directly. First boot can take several minutes. If it fails, return to fastboot/recovery and restore the known-good boot path; do not proceed to permanent flashing or driver injection until the cause is understood.

## 9. Phase 4 — collect diagnostics

Use the appropriate `RUN_PHASE4*` helper only after Windows has booted or UEFI mass storage is active, as required by that helper. Preserve the resulting logs for debugging.

Do not assume that a successful Windows boot means all hardware is supported. Check Device Manager and the repository status log.

## 10. Phase 5 — inject drivers offline

The current host-side driver injection tool is `phase5_drivers.ps1`, called by the `RUN_PHASE5_BATCH*.bat` wrappers.

It expects:

- The tablet's Windows partition to be exposed through UEFI mass storage.
- The expected p35 size and a valid Windows installation.
- The Kona BSP at `kona_bsp\Drivers\`.
- A selected driver batch text file.

Before injecting drivers, the script snapshots the Windows registry configuration folder. It then uses DISM to add the listed drivers offline and can set selected devices to re-enumerate on first boot.

**Do not inject every driver at once.** Use reviewed batches, retain the registry snapshot and logs, and verify each batch. Some experimental packages can make Windows unbootable.

### PEP-specific warning

- Qualcomm PEP is still under active pipa-specific investigation.
- The diagnostic patch in this repository reads rail state; it is not a complete power-management fix.
- Do not install `PPMSettings-8250Profiles.wd.ppkg`. The project notes document a critical boot failure associated with it.
- Do not treat the current unsigned GitHub Actions build as an installable production driver. Driver signing, certificate trust and test-signing requirements must be resolved and documented before deployment.

## 11. Hardware status and driver expectations

Check [STATUS_LOG.md](STATUS_LOG.md) for the latest tested state. The repository has separate work for touchscreen, Bluetooth, WLAN/power, backlight and Qualcomm PEP; these are not all supplied by the PEP driver.

A working touch or Bluetooth path does not prove Wi-Fi internet access, GPU acceleration, charging or power management works. Test each device independently and record its hardware ID, driver version, Device Manager status and relevant logs.

## 12. Recovery and rollback

Before any destructive operation:

- Keep verified backups off-device.
- Know how to enter fastboot and recovery with physical buttons.
- Keep a known-good stock boot image and a tested restore procedure.
- Do not flash UEFI permanently until temporary boot is reliable.
- Do not accept Windows disk-initialization prompts.
- If an experimental driver causes a boot loop, use the documented clean-reinstall/known-good workflow rather than improvising registry edits on a damaged installation.

The repository's rollback scripts are host-side tools and are not substitutes for a complete firmware backup.

## 13. What is still needed for a Pad-only installation

The following work must be completed before this project can honestly claim a no-PC installation method:

1. A reproducible GitHub Actions build and release pipeline for the required UEFI and ARM64 driver artifacts.
2. A verified way to obtain and validate the Windows ARM64 image on the tablet.
3. A Termux/recovery-compatible deployment tool that safely applies the WIM and creates the ESP/BCD without relying on Windows DISM/DiskPart on a PC.
4. A reviewed, device-specific partition plan generated from the live partition table rather than hard-coded partition numbers.
5. A safe tablet-side UEFI boot/flash flow with checksum checks and a tested rollback route.
6. A complete pre-first-boot driver staging process, including signing and test-signing requirements.
7. End-to-end testing on pipa and clear logs for each stage.

Until these are done and tested, this document is a map of the existing workflow and its gaps—not a claim that Windows can already be installed from the Pad alone.
