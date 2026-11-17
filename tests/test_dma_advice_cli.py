#!/usr/bin/env python3
"""Smoke test for explanatory, read-only DMA guidance CLI."""
import subprocess
import sys

def run(exe, *args):
    return subprocess.run([exe, *args], capture_output=True, text=True)

def main(exe):
    known = (
        "DMA_PROFILE_UNKNOWN", "DMA_ADDRESS_MASK", "DMA_ALIGNMENT",
        "DMA_ADDRESS_OVERFLOW", "DMA_MEMORY_UNAVAILABLE",
    )
    for code in known:
        for language in ("en", "pl"):
            r = run(exe, code, language)
            assert r.returncode == 10, (code, language, r)
            assert f"[{code}]" in r.stdout, r.stdout
            assert "1. " in r.stdout and "2. " in r.stdout, r.stdout
            assert "3. " in r.stdout, r.stdout
    passed = run(exe, "DMA_OK", "en")
    assert passed.returncode == 0 and "[DMA_OK]" in passed.stdout
    assert "Operation stopped" not in passed.stdout
    invalid = run(exe, "DMA_UNKNOWN", "pl")
    assert invalid.returncode == 20
    print("DMA advice CLI smoke tests passed")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_dma_advice_cli.py DOLLY_DMA_ADVICE")
    main(sys.argv[1])
