#!/usr/bin/env python3
"""Inspect selected Qualcomm UFS nodes in a tar export of a live Linux DT.

This is an inventory helper, not an ACPI generator. It does not infer ACPI
GSIs, create AML, or declare that a device is Windows-compatible.
"""
from __future__ import annotations

import argparse
import io
import tarfile
from pathlib import PurePosixPath


NODES = (
    "soc/ufshc@1d84000",
    "soc/ufsphy_mem@1d87000",
    "soc/ufshc_card@1da4000",
    "soc/ufsphy_card@1da7000",
    "soc/ufscardice@1db0000",
)
STRING_PROPERTIES = (
    "compatible", "status", "reg-names", "interrupt-names", "clock-names",
    "reset-names", "phy-names", "name",
)
CELL_PROPERTIES = ("reg", "interrupts", "clocks", "resets", "phys")


def decode_strings(raw: bytes) -> str:
    return ", ".join(part.decode("utf-8", "replace") for part in raw.rstrip(b"\0").split(b"\0"))


def decode_cells(raw: bytes) -> str:
    if len(raw) % 4:
        return f"<malformed: {len(raw)} bytes, not a multiple of 4>"
    values = [int.from_bytes(raw[i:i + 4], "big") for i in range(0, len(raw), 4)]
    return " ".join(f"0x{x:08X}" for x in values)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("tar_path", help="live-device-tree.tar from the tablet collector")
    args = parser.parse_args()

    with tarfile.open(args.tar_path, "r:*") as archive:
        members = {}
        for member in archive.getmembers():
            path = str(PurePosixPath(member.name))
            if path.startswith("./"):
                path = path[2:]
            if member.isfile():
                members[path] = member

        found_any = False
        for node in NODES:
            props = {PurePosixPath(path).name: path for path in members
                     if str(PurePosixPath(path).parent) == node}
            if not props:
                continue
            found_any = True
            print(f"## /{node}")
            for prop in STRING_PROPERTIES:
                if prop in props:
                    raw = archive.extractfile(members[props[prop]]).read()
                    print(f"- {prop}: {decode_strings(raw)}")
            for prop in CELL_PROPERTIES:
                if prop in props:
                    raw = archive.extractfile(members[props[prop]]).read()
                    print(f"- {prop} cells: {decode_cells(raw)}")
            print()

        if not found_any:
            print("No expected UFS nodes found. Check tar root/layout and device tree.")
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
