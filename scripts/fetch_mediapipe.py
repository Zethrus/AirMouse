#!/usr/bin/env python3
"""Download MediaPipe Tasks C library for the host platform."""

from __future__ import annotations

import argparse
import platform
import sys
import urllib.request
import zipfile
from pathlib import Path

VERSION = "0.10.35"
INDEX = "https://pypi.org/pypi/mediapipe/json"


def wheel_url(system: str) -> tuple[str, str]:
    import json

    with urllib.request.urlopen(INDEX, timeout=60) as resp:
        data = json.load(resp)
    files = data["releases"].get(VERSION, [])
    if system == "Windows":
        needle = "win_amd64"
        inner = "mediapipe/tasks/c/libmediapipe.dll"
    else:
        needle = "manylinux"
        inner = "mediapipe/tasks/c/libmediapipe.so"
    for item in files:
        name = item.get("filename", "")
        if needle in name and name.endswith(".whl"):
            return item["url"], inner
    raise SystemExit(f"no MediaPipe {VERSION} wheel for {system} ({needle})")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dest",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "third_party" / "mediapipe" / "lib",
    )
    args = parser.parse_args()
    args.dest.mkdir(parents=True, exist_ok=True)

    system = platform.system()
    url, member = wheel_url(system)
    out_name = Path(member).name
    out_path = args.dest / out_name
    if out_path.exists() and out_path.stat().st_size > 1_000_000:
        print(f"already present: {out_path}")
        return 0

    print(f"downloading {url}")
    wheel_path = args.dest / "mediapipe.whl"
    urllib.request.urlretrieve(url, wheel_path)
    with zipfile.ZipFile(wheel_path) as zf:
        with zf.open(member) as src, out_path.open("wb") as dst:
            dst.write(src.read())
    wheel_path.unlink(missing_ok=True)
    print(f"wrote {out_path} ({out_path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
