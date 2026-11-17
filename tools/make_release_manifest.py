#!/usr/bin/env python3
"""Generate SHA-256 release manifest for offline verification.
This tool does not sign its output; distribute checksum/reference from a
trusted separate channel for authenticity against malicious modification.
"""
import argparse
import hashlib
import json
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path, help="release staging root")
    ap.add_argument("--release", required=True, help="exact Dolly release ID")
    ap.add_argument("--output", type=Path, required=True)
    ns = ap.parse_args()
    root = ns.root.resolve(strict=True)
    if not root.is_dir() or not ns.release.strip():
        ap.error("root must be a directory and release cannot be blank")
    entries = []
    output = ns.output.resolve()
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            ap.error(f"symlinks are not allowed in release packages: {path}")
        if not path.is_file():
            continue
        if path.resolve() == output:
            continue
        digest = hashlib.sha256()
        with path.open("rb") as src:
            for chunk in iter(lambda: src.read(65536), b""):
                digest.update(chunk)
        entries.append({"path": path.relative_to(root).as_posix(),
                        "bytes": path.stat().st_size,
                        "sha256": digest.hexdigest()})
    manifest = {"schema": 1, "release": ns.release,
                "algorithm": "sha256", "files": entries}
    ns.output.parent.mkdir(parents=True, exist_ok=True)
    ns.output.write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n",
                         encoding="utf-8")

if __name__ == "__main__":
    main()
