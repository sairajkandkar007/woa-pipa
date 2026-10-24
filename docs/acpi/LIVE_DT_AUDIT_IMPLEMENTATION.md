# Live device-tree parser validation

Date: 2026-10-24

## What was implemented

- Added regression tests for the live UFS DT inventory parser in `tools/acpi/tests/test_audit_live_ufs_dt.py`.
- Added GitHub Actions workflow `.github/workflows/pipa-dt-audit.yml` to run those tests and Python syntax compilation on pushes, pull requests, and manual dispatches.
- The tests use synthetic tar fixtures and cover:
  - UFS host properties and resource cells;
  - UFS PHY and disabled card-controller nodes;
  - a missing-node failure;
  - malformed cell data.

## Local validation

The four regression tests passed locally:

```text
Ran 4 tests in 3.366s
OK
```

Python byte-compilation also passed.

The parser was also run against the user's supplied `live-device-tree.tar` from `pipa-acpi-data.zip`. It found the expected nodes:

- `/soc/ufshc@1d84000`: status `ok`, controller range `0x01D84000 / 0x3000`, shared `ufs_ice` range `0x01D90000 / 0x8000`, interrupt specifier `<0 0x109 0x4>`, reset name `core_reset`, PHY name `ufsphy`.
- `/soc/ufsphy_mem@1d87000`: status `ok`, PHY range `0x01D87000 / 0xE00`, and the same named `ufs_ice` range.
- `/soc/ufshc_card@1da4000`: status `disabled`.
- `/soc/ufsphy_card@1da7000`: status `disabled`.
- `/soc/ufscardice@1db0000`: separate card ICE block at `0x01DB0000 / 0x8000`.

The parser prints raw clock/reset/PHY phandle cells; it does not resolve phandles into providers, decode interrupt specifiers into ACPI GSIs, or infer a Windows driver contract. These remain explicit follow-up tasks.

## Safety boundary

This is inventory and regression-test infrastructure only. It does not generate AML or change `UFS0` from disabled to enabled. Keep the ACPI device hidden until resource translation, Windows driver matching, and controller initialization requirements are documented and independently reviewed.
