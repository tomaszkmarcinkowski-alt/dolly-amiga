#!/usr/bin/env python3
"""Offline verification of the release's CRC32 manifest. Host/packaging only.
Cannot authenticate a maliciously modified manifest or executable.
"""
import argparse
import json
from pathlib import Path
import zlib

def main():
    p = argparse.ArgumentParser()
    p.add_argument("root", type=Path)
    p.add_argument("manifest", type=Path)
    p.add_argument("--release", required=True)
    a = p.parse_args()
    root = a.root.resolve(strict=True)
    manifest = json.loads(a.manifest.read_text(encoding="utf-8"))
    if (manifest.get("schema") != 1 or
        manifest.get("algorithm") != "CRC32/ISO-HDLC" or
        manifest.get("release") != a.release):
        p.error("manifest schema, algorithm or release differs")
    paths = set()
    for entry in manifest["files"]:
        relative = entry["path"]
        parts = Path(relative).parts
        if (not parts or Path(relative).is_absolute() or
            any(part in (".", "..") for part in parts) or
            ":" in relative or "\\" in relative or relative in paths):
            p.error("unsafe or duplicate manifest path")
        paths.add(relative)
        file = root.joinpath(*parts)
        # Reject escaping through a symlink in any parent component.
        try:
            file.resolve(strict=True).relative_to(root)
        except (OSError, ValueError):
            p.error(f"missing/unsafe path: {relative}")
        if file.is_symlink() or not file.is_file():
            p.error(f"missing/invalid file: {relative}")
        size = file.stat().st_size
        crc = 0
        with file.open("rb") as data:
            for chunk in iter(lambda: data.read(4096), b""):
                crc = zlib.crc32(chunk, crc)
        if size != entry["size"] or f"{crc & 0xffffffff:08X}" != entry["crc32"]:
            p.error(f"CRC32/size mismatch: {relative}")
    print(f"Dolly package CRC32 OK ({len(paths)} files, release {a.release})")

if __name__ == "__main__":
    main()
