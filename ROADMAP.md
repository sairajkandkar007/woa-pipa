# Project Roadmap

This roadmap describes research milestones, not promises of functionality. Mark a task complete only when evidence and test conditions are recorded.

## Phase 1 — Research foundation
- [ ] Collect reliable Xiaomi Pad 6 (pipa) hardware and boot-chain references.
- [ ] Review available kernel source and device-tree information.
- [ ] Document current bootloader, partition, firmware, and recovery facts.
- [ ] Create a source register with dates and confidence levels.

## Phase 2 — UEFI and boot feasibility
- [ ] Study EDK II / Project Mu and relevant Qualcomm platform ports.
- [ ] Identify platform-specific initialization, ACPI, storage, USB, and display requirements.
- [ ] Record what is known, unknown, and experimentally verified.
- [ ] Define a safe test plan before attempting device modifications.

## Phase 3 — Windows ARM64 driver research
- [ ] Map required components: storage, USB, touch, display, Wi-Fi, Bluetooth, audio, sensors, and power management.
- [ ] Research Adreno 650 graphics options for basic desktop use.
- [ ] Verify driver provenance, architecture, licensing, and redistribution permissions.
- [ ] Track each component in `docs/driver-status.md`.

## Phase 4 — Controlled testing
- [ ] Establish reproducible test procedures and logging.
- [ ] Record device state, build identifiers, test steps, results, and limitations.
- [ ] Keep known-good recovery information available before risky experiments.
- [ ] Publish only builds and instructions that can be clearly identified and responsibly shared.

## Phase 5 — Documentation and collaboration
- [ ] Review issues and contributions.
- [ ] Separate confirmed results from hypotheses.
- [ ] Update compatibility status after each verified test.
- [ ] Publish a release only when its contents, limitations, and installation risks are documented.

## Status definitions

- **Unknown** — not researched or insufficient evidence.
- **Researching** — investigation is in progress.
- **Experimental** — attempted, but not sufficiently validated.
- **Verified** — reproduced on the stated device and configuration with supporting evidence.
- **Not working** — tested and confirmed to fail under the documented conditions.
