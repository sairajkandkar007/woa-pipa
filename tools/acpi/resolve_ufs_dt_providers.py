#!/usr/bin/env python3
"""Resolve provider phandles referenced by selected pipa UFS nodes in a DT tar.

This is an evidence inventory only. It does not infer clock rates, reset
sequencing, ACPI GSIs, or generate AML. DT phandle argument cells are printed
raw alongside the provider's #*-cells value for manual/provider-specific review.
"""
from __future__ import annotations

import argparse
import tarfile
from pathlib import PurePosixPath

TARGETS = (
    "soc/ufshc@1d84000",
    "soc/ufsphy_mem@1d87000",
)
REFS = {
    "clocks": ("clock-names", "#clock-cells"),
    "resets": ("reset-names", "#reset-cells"),
    "phys": ("phy-names", "#phy-cells"),
}
PHANDLE_PROPS = ("phandle", "linux,phandle")
STRING_PROPS = ("compatible", "status", "clock-output-names", "reset-names", "clock-names")


def strings(raw: bytes) -> list[str]:
    return [v.decode("utf-8", "replace") for v in raw.rstrip(b"\0").split(b"\0") if v]


def cells(raw: bytes) -> list[int] | None:
    if len(raw) % 4:
        return None
    return [int.from_bytes(raw[i:i + 4], "big") for i in range(0, len(raw), 4)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("tar_path")
    args = ap.parse_args()

    with tarfile.open(args.tar_path, "r:*") as archive:
        nodes: dict[str, dict[str, bytes]] = {}
        for member in archive.getmembers():
            if not member.isfile():
                continue
            path = str(PurePosixPath(member.name))
            if path.startswith("./"):
                path = path[2:]
            parent = str(PurePosixPath(path).parent)
            prop = PurePosixPath(path).name
            stream = archive.extractfile(member)
            if stream:
                nodes.setdefault(parent, {})[prop] = stream.read()

    by_handle: dict[int, str] = {}
    for node, props in nodes.items():
        for prop in PHANDLE_PROPS:
            values = cells(props[prop]) if prop in props else None
            if values and len(values) == 1 and values[0] not in (0, 0xFFFFFFFF):
                by_handle[values[0]] = node

    found = False
    for target in TARGETS:
        props = nodes.get(target)
        if not props:
            continue
        found = True
        print(f"## /{target}")
        for prop in STRING_PROPS:
            if prop in props:
                print(f"- {prop}: {', '.join(strings(props[prop]))}")
        for prop, (names_prop, count_prop) in REFS.items():
            if prop not in props:
                continue
            raw_cells = cells(props[prop])
            names = strings(props[names_prop]) if names_prop in props else []
            if raw_cells is None:
                print(f"- {prop}: malformed property length ({len(props[prop])} bytes)")
                continue
            i = 0
            idx = 0
            while i < len(raw_cells):
                handle = raw_cells[i]
                provider = by_handle.get(handle)
                label = names[idx] if idx < len(names) else f"index-{idx}"
                if provider is None:
                    print(f"- {prop}[{label}]: phandle 0x{handle:08X} unresolved")
                    i += 1
                    idx += 1
                    continue
                provider_props = nodes[provider]
                count_values = cells(provider_props.get(count_prop, b""))
                arg_count = count_values[0] if count_values and len(count_values) == 1 else 0
                args_cells = raw_cells[i + 1:i + 1 + arg_count]
                comp = ", ".join(strings(provider_props.get("compatible", b"")))
                print(
                    f"- {prop}[{label}]: provider=/{provider}; "
                    f"compatible={comp or 'unknown'}; {count_prop}={arg_count}; "
                    f"args=" + (" ".join(f"0x{x:08X}" for x in args_cells) or "(none)")
                )
                i += 1 + arg_count
                idx += 1
        print()
    if not found:
        print("No selected UFS host/PHY nodes found; check DT tar layout.")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
