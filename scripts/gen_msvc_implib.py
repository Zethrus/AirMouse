#!/usr/bin/env python3
"""Build an MSVC import library from a DLL (needs dumpbin + lib on PATH)."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def parse_exports(dll: Path) -> list[str]:
    dumpbin = shutil.which("dumpbin")
    if not dumpbin:
        raise SystemExit("dumpbin.exe not on PATH (run from a VS developer prompt)")
    proc = subprocess.run(
        [dumpbin, "/exports", str(dll)],
        check=True,
        capture_output=True,
        text=True,
        errors="replace",
    )
    names: list[str] = []
    in_table = False
    for line in proc.stdout.splitlines():
        if "ordinal" in line.lower() and "name" in line.lower():
            in_table = True
            continue
        if not in_table:
            continue
        if line.strip() == "" or line.startswith("  Summary"):
            if names:
                break
            continue
        parts = line.split()
        # ordinal hint RVA name
        if len(parts) >= 4 and parts[0].isdigit():
            names.append(parts[-1])
        elif len(parts) >= 3 and parts[0].isdigit():
            names.append(parts[-1])
    if not names:
        raise SystemExit(f"no exports parsed from {dll}")
    return names


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    args = parser.parse_args()
    dll = args.dll.resolve()
    if not dll.exists():
        raise SystemExit(f"missing {dll}")

    lib_exe = shutil.which("lib")
    if not lib_exe:
        raise SystemExit("lib.exe not on PATH")

    names = parse_exports(dll)
    def_path = dll.with_suffix(".def")
    def_path.write_text(
        "LIBRARY libmediapipe\nEXPORTS\n" + "\n".join(f"    {n}" for n in names) + "\n",
        encoding="utf-8",
    )
    implib = dll.with_suffix(".lib")
    subprocess.run(
        [lib_exe, f"/def:{def_path}", f"/out:{implib}", "/machine:x64"],
        check=True,
    )
    print(f"wrote {implib} ({len(names)} exports)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
