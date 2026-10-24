# Project Roadmap

This roadmap tracks research and implementation milestones. A checkbox is marked complete only when the corresponding work has been performed and recorded; source presence alone does not prove device functionality.

## Phase 1 — Research and source inventory
- [x] Identify the Xiaomi Pad 6 (pipa) target and collect the known kernel/device/vendor source links.
- [x] Audit the available kernel source and record key limitations in the project research notes.
- [x] Locate the existing Mu-Qcom pipa UEFI platform and review its reported status.
- [ ] Complete a consolidated source register with dates, revisions, and confidence levels.
- [ ] Independently verify all boot-chain, partition, firmware, and recovery facts.

## Phase 2 — UEFI source preparation
- [x] Identify the existing pipa-specific Mu-Qcom platform files.
- [x] Copy selected pipa platform configuration, ACPI, memory/configuration-map, and build-support text files into `third_party/mu-qcom-pipa-snapshot/`.
- [x] Add documentation describing snapshot provenance and limitations.
- [ ] Copy and verify the complete required Mu-Qcom core packages.
- [ ] Resolve pinned submodule dependencies and confirm source availability.
- [ ] Obtain and verify required binary resources, including the pipa DTB and ramdisk.
- [ ] Audit toolchain and build dependencies against the actual build scripts.

## Phase 3 — Dependency integration
- [ ] Integrate all required pinned dependencies into a reproducible build layout.
- [ ] Verify required files and licenses without silently substituting another device's files.
- [ ] Add a GitHub Actions workflow that builds from the local project source.
- [ ] Confirm the workflow fails clearly when required inputs are absent.

## Phase 4 — UEFI build and output validation
- [ ] Produce a UEFI build using GitHub-hosted runners.
- [ ] Archive build logs, source revisions, and tool versions.
- [ ] Verify generated artifacts and Android boot-image structure.
- [ ] Review device-specific assumptions before any boot attempt.

## Phase 5 — Controlled device testing
- [ ] Document a device-specific test and recovery plan.
- [ ] Verify recovery access and preserve known-good recovery resources.
- [ ] Perform controlled UEFI boot testing only after the image and procedure are reviewed.
- [ ] Record device state, exact build identifiers, logs, results, and limitations.

## Phase 6 — Windows ARM64 boot and driver research
- [ ] Establish whether Windows ARM64 reaches the boot environment.
- [ ] Validate storage, USB, display, touch, networking, audio, power, and other components individually.
- [ ] Research and validate Adreno 650 graphics support for basic desktop use.
- [ ] Record driver provenance, architecture, licensing, and redistribution permissions.

## Phase 7 — Documentation and collaboration
- [ ] Keep confirmed results separate from hypotheses.
- [ ] Update compatibility status after each verified test.
- [ ] Publish only builds and instructions with clearly documented contents, limitations, and risks.

## Status definitions

- **Unknown** — not researched or insufficient evidence.
- **Researching** — investigation is in progress.
- **Experimental** — attempted, but not sufficiently validated.
- **Verified** — reproduced on the stated device and configuration with supporting evidence.
- **Not working** — tested and confirmed to fail under the documented conditions.

## Current state

Phase 1 is partially complete. Phase 2 is in progress: selected source files have been copied, but this is not a complete build tree. No UEFI firmware has been built, and no device boot test or Windows boot has been performed.
