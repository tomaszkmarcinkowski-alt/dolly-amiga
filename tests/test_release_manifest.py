#!/usr/bin/env python3
"""Release-manifest CRC32 and version regression; no hardware access."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

def main(script):
    with tempfile.TemporaryDirectory(prefix="dolly-manifest-") as temp:
        root = Path(temp)
        package = root / "release"
        package.mkdir()
        (package / "Dolly").write_bytes(b"123456789")
        docs = package / "Docs"
        docs.mkdir()
        (docs / "Help.guide").write_bytes(b"sample-help")
        output = root / "manifest.json"
        subprocess.run([sys.executable, script, str(package),
                        "--release", "Dolly-build-test", "--output", str(output)],
                       check=True)
        manifest = json.loads(output.read_text(encoding="utf-8"))
        assert manifest["release"] == "Dolly-build-test"
        assert manifest["algorithm"] == "CRC32/ISO-HDLC"
        files = {entry["path"]: entry for entry in manifest["files"]}
        assert files["Dolly"]["size"] == 9
        assert files["Dolly"]["crc32"] == "CBF43926"
        assert len(files) == 2
        # The manifest records original bytes, so later corruption is detectable.
        (package / "Dolly").write_bytes(b"12345678x")
        import zlib
        assert f"{zlib.crc32((package / 'Dolly').read_bytes()):08X}" != files["Dolly"]["crc32"]
    print("Dolly CRC32 release manifest tests passed")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_release_manifest.py path/to/make_release_manifest.py")
    main(sys.argv[1])
