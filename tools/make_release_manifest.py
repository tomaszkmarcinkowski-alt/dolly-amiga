#!/usr/bin/env python3
"""Create an exact-version + CRC32 release manifest (accidental corruption).
Not cryptographic authentication; executable cannot trust self-controlled
metadata to prove that nobody deliberately changed the distribution.
"""
import argparse
import json
from pathlib import Path
import zlib

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path, help="release staging directory")
    ap.add_argument("--release", required=True, help="exact Dolly release/build ID")
    ap.add_argument("--output", type=Path, required=True)
    ns = ap.parse_args()
    root = ns.root.resolve(strict=True)
    if not root.is_dir() or not ns.release.strip():
        ap.error("release directory and nonempty release identifier required")
    output = ns.output.resolve()
    entries = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            ap.error(f"symlinks not allowed in release: {path}")
        if not path.is_file() or path.resolve() == output:
            continue
        crc = 0
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(4096), b""):
                crc = zlib.crc32(chunk, crc)
        entries.append({"path": path.relative_to(root).as_posix(),
                        "size": path.stat().st_size,
                        "crc32": f"{crc & 0xffffffff:08X}"})
    manifest = {"schema": 1, "release": ns.release,
                "algorithm": "CRC32/ISO-HDLC", "files": entries}
    ns.output.parent.mkdir(parents=True, exist_ok=True)
    ns.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
if __name__ == "__main__":
    main()
