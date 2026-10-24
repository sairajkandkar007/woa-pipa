#!/usr/bin/env python3
"""Synthetic regression tests for UFS DT provider-phandle resolution."""
from __future__ import annotations

import io
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "resolve_ufs_dt_providers.py"


def make_tar(files: dict[str, bytes]) -> bytes:
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode="w") as archive:
        for name, payload in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(payload)
            archive.addfile(info, io.BytesIO(payload))
    return stream.getvalue()


class ProviderResolverTests(unittest.TestCase):
    def run_tool(self, payload: bytes) -> subprocess.CompletedProcess[str]:
        with tempfile.NamedTemporaryFile(suffix=".tar") as tmp:
            tmp.write(payload)
            tmp.flush()
            return subprocess.run(
                [sys.executable, str(SCRIPT), tmp.name],
                text=True, capture_output=True, check=False,
            )

    def test_resolves_clock_provider_and_argument_cells(self) -> None:
        payload = make_tar({
            "soc/ufshc@1d84000/clock-names": b"core_clk\0iface_clk\0",
            "soc/ufshc@1d84000/clocks": bytes.fromhex(
                "00000011 00000003 00000012 00000000"
            ),
            "soc/clock-controller/compatible": b"qcom,gcc-kona\0",
            "soc/clock-controller/phandle": bytes.fromhex("00000011"),
            "soc/clock-controller/#clock-cells": bytes.fromhex("00000001"),
            "soc/clock-controller2/compatible": b"qcom,rpmh-clk\0",
            "soc/clock-controller2/phandle": bytes.fromhex("00000012"),
            "soc/clock-controller2/#clock-cells": bytes.fromhex("00000001"),
        })
        result = self.run_tool(payload)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("provider=/soc/clock-controller", result.stdout)
        self.assertIn("compatible=qcom,gcc-kona", result.stdout)
        self.assertIn("args=0x00000003", result.stdout)
        self.assertIn("provider=/soc/clock-controller2", result.stdout)

    def test_unresolved_phandle_is_reported_without_guessing(self) -> None:
        payload = make_tar({
            "soc/ufshc@1d84000/clock-names": b"core_clk\0",
            "soc/ufshc@1d84000/clocks": bytes.fromhex("0000abcd 00000001"),
        })
        result = self.run_tool(payload)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("phandle 0x0000ABCD unresolved", result.stdout)

    def test_missing_target_fails(self) -> None:
        result = self.run_tool(make_tar({"soc/example/status": b"okay\0"}))
        self.assertEqual(result.returncode, 2)
        self.assertIn("No selected UFS host/PHY nodes", result.stdout)


if __name__ == "__main__":
    unittest.main()
