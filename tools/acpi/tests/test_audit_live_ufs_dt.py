#!/usr/bin/env python3
"""Regression tests for the live-DT UFS inventory parser.

The fixture is synthetic and validates parser behavior only; it is not hardware
evidence and must never be used to generate or enable ACPI resources.
"""
from __future__ import annotations

import io
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "audit_live_ufs_dt.py"


def make_tar(files: dict[str, bytes]) -> bytes:
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode="w") as archive:
        for name, payload in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(payload)
            archive.addfile(info, io.BytesIO(payload))
    return stream.getvalue()


class LiveDtAuditTests(unittest.TestCase):
    def run_audit(self, tar_bytes: bytes) -> subprocess.CompletedProcess[str]:
        with tempfile.NamedTemporaryFile(suffix=".tar") as tmp:
            tmp.write(tar_bytes)
            tmp.flush()
            return subprocess.run(
                [sys.executable, str(SCRIPT), tmp.name],
                text=True,
                capture_output=True,
                check=False,
            )

    def test_reports_host_resources_and_properties(self) -> None:
        fixture = make_tar({
            "./soc/ufshc@1d84000/compatible": b"qcom,ufshc\0",
            "./soc/ufshc@1d84000/status": b"ok\0",
            "./soc/ufshc@1d84000/reg-names": b"ufs_mem\0ufs_ice\0",
            "./soc/ufshc@1d84000/reg": bytes.fromhex(
                "00000001 01d84000 00000000 00003000 "
                "00000001 01d90000 00000000 00008000"
            ),
            "./soc/ufshc@1d84000/interrupts": bytes.fromhex(
                "00000000 00000109 00000004"
            ),
        })
        result = self.run_audit(fixture)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("## /soc/ufshc@1d84000", result.stdout)
        self.assertIn("qcom,ufshc", result.stdout)
        self.assertIn("ufs_mem, ufs_ice", result.stdout)
        self.assertIn("0x01D84000", result.stdout)
        self.assertIn("0x00000109", result.stdout)

    def test_reports_phy_and_disabled_card_controller(self) -> None:
        fixture = make_tar({
            "soc/ufsphy_mem@1d87000/compatible": b"qcom,ufs-phy-qmp-v4\0",
            "soc/ufsphy_mem@1d87000/status": b"ok\0",
            "soc/ufshc_card@1da4000/compatible": b"qcom,ufshc\0",
            "soc/ufshc_card@1da4000/status": b"disabled\0",
        })
        result = self.run_audit(fixture)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("qcom,ufs-phy-qmp-v4", result.stdout)
        self.assertIn("disabled", result.stdout)

    def test_missing_expected_nodes_fails(self) -> None:
        result = self.run_audit(make_tar({"soc/example/status": b"ok\0"}))
        self.assertEqual(result.returncode, 2)
        self.assertIn("No expected UFS nodes found", result.stdout)

    def test_malformed_cell_length_is_reported(self) -> None:
        fixture = make_tar({
            "soc/ufshc@1d84000/compatible": b"qcom,ufshc\0",
            "soc/ufshc@1d84000/reg": b"abc",
        })
        result = self.run_audit(fixture)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("<malformed: 3 bytes", result.stdout)


if __name__ == "__main__":
    unittest.main()
