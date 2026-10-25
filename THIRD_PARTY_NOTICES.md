# Third-party notices and acknowledgements

This project studies and may adapt ideas, interfaces, build practices, and platform-integration approaches from upstream projects. This file records the sources known to have informed the research. It does not imply that every listed project’s source code has been copied into this repository.

## Project Aloha — mu_aloha_platforms

- Repository: https://github.com/Project-Aloha/mu_aloha_platforms
- Purpose here: reference for Qualcomm Project Mu UEFI platform structure, SM8250/Kona platform integration, ACPI table update patterns, and firmware build workflows.
- Upstream license: BSD 2-Clause; see [LICENSE](https://github.com/Project-Aloha/mu_aloha_platforms/blob/main/LICENSE).
- Copyright notices in the upstream license include Andrei Warkentin, Apple Inc., ARM Limited, Bingxing Wang, DuoWoA authors, AlohaWoA authors, Intel Corporation, Linaro Limited, Microsoft Corporation, Samuel Tulach, The Android Open Source Project, The Linux Foundation, and Travis Geiselbrecht. Preserve the upstream license and copyright notice with any source files redistributed from that repository.

No claim is made that the Xiaomi Pad 6 (pipa) is supported by Project Aloha. Its supported device targets are reference material only; pipa-specific hardware resources must be independently verified.

### BSD 2-Clause license text (Project Aloha)

Copyright (c) Andrei Warkentin. All rights reserved.
Copyright (c) Apple Inc. All rights reserved.
Copyright (c) ARM Limited. All rights reserved.
Copyright (c) Bingxing Wang. All rights reserved.
Copyright (c) DuoWoA authors. All rights reserved.
Copyright (c) AlohaWoA authors. All rights reserved.
Copyright (c) Intel Corporation. All rights reserved.
Copyright (c) Linaro Limited. All rights reserved.
Copyright (c) Microsoft Corporation. All rights reserved.
Copyright (c) Samuel Tulach. All rights reserved.
Copyright (c) The Android Open Source Project. All rights reserved.
Copyright (c) The Linux Foundation. All rights reserved.
Copyright (c) Travis Geiselbrecht. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

## WOA-Project — mu_andromeda_platforms

- Repository: https://github.com/WOA-Project/mu_andromeda_platforms
- Purpose here: upstream lineage identified by Project Aloha for its Qualcomm platform implementation.
- Relationship: credited as an upstream project to investigate when tracing code lineage. Do not assume its license applies to all files here; check the exact source file and revision before copying or redistributing code.

## Microsoft — Project Mu and EDK II

- Project Mu: https://github.com/microsoft/mu
- EDK II: https://github.com/tianocore/edk2
- Purpose here: firmware architecture and build-system references.
- License: these are separate projects with their own license files and component-level notices. Follow the license of the specific component used; this notice does not relicense them.

## Qualcomm platform and device sources

- Xiaomi Pad 6 kernel source: https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss
- Purpose here: device-tree, kernel configuration, and driver-source evidence used to investigate pipa hardware.
- License: kernel source is governed by its own repository and file-level licenses. Preserve notices and comply with the applicable license when using or redistributing any source.

## Scope of this notice

This is an attribution and license-tracking document, not legal advice. It does not grant rights beyond the applicable upstream licenses. Before importing any source file, record its origin, revision/commit, copyright header, and license; retain required notices and check whether additional third-party components are involved. The repository’s original documentation and newly written code are not automatically relicensed by the licenses listed above.
