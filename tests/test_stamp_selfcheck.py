#!/usr/bin/env python3
"""Host regression for post-link CRC field stamping (no sidecar manifest)."""
import subprocess
import sys
import tempfile
from pathlib import Path

MAGIC = b"DOLLY:CRC32:V1!!"
RECORD = MAGIC + b"00000000" + b"DOLLY-UNSTAMPED!"

def run(tool, file, *options):
    return subprocess.run([sys.executable, str(tool), str(file), *options],
                          capture_output=True, text=True)

def main(tool):
    assert len(MAGIC) == 16 and len(RECORD) == 40
    with tempfile.TemporaryDirectory(prefix="dolly-selfcrc-") as temp:
        file = Path(temp) / "Dolly"
        file.write_bytes(b"AmigaHunkExample" + RECORD +
                         b"\0$VER: Dolly 0.1 (17.11.2026)\0" + b"tail")
        assert run(tool, file, "--verify").returncode != 0
        stamp = run(tool, file, "--release", "Dolly-M0-test")
        assert stamp.returncode == 0, stamp.stderr
        good = run(tool, file, "--verify", "--release", "Dolly-M0-test")
        assert good.returncode == 0, good.stderr
        assert run(tool, file, "--verify", "--release", "Wrong").returncode != 0
        assert run(tool, file, "--release", "Dolly-M0-test").returncode != 0
        stamped = bytearray(file.read_bytes())
        pos = stamped.index(MAGIC)
        assert stamped[pos+16:pos+24] != b"00000000"
        assert stamped[pos+24:pos+40] == b"Dolly-M0-test".ljust(16, b" ")
        assert b"$VER: Dolly 0.1" in stamped
        stamped[-1] ^= 1
        file.write_bytes(stamped)
        assert run(tool, file, "--verify").returncode != 0
        file.write_bytes(b"x" + RECORD + b"y" + RECORD +
                         b"\0$VER: Dolly 0.1 (17.11.2026)\0")
        assert run(tool, file, "--release", "Dolly-M0-test").returncode != 0
        file.write_bytes(b"x" + RECORD + b"y")
        no_version = run(tool, file, "--release", "Dolly-M0-test")
        assert no_version.returncode != 0 and "$VER" in no_version.stderr
    print("Post-link embedded CRC32 stamping/verification tests passed")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_stamp_selfcheck.py stamp_selfcheck.py")
    main(Path(sys.argv[1]))
