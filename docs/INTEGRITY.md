# Dolly integrity policy — exact release version + CRC32

**Decision (2026-11-17):** lightweight file verification for the Motorola 68000 baseline. The executable and every optional asset must match the **exact Dolly release/build ID**, expected byte length and **CRC32** from a release manifest. Optional plugins additionally check their own ABI/schema version.

## What this achieves
- Detects most accidental corruption, incomplete copying, mixed-version installations and wrongly packaged files.
- A faulty optional asset is **never used**: Dolly names the exact DOLLY: path, explains the affected feature and continues with GadTools/core features.
- A faulty main executable blocks launch of the normal program with a clear error; the bootstrap/installer can suggest reinstalling the matching release.

## Critical limitation
CRC32 is **not an authentication or anti-malware mechanism**. A malicious party who can replace both files and manifest can adjust the checksum. No self-contained program can reliably prove the authenticity of its own binary before its first instruction executes. Therefore the executable itself needs a **separate startup checker/installer/bootstrap** using the expected manifest. That prelaunch component is not implemented yet; documentation must not claim that main executable self-verification already works.

## Current M0 components
- `dolly_checksum`: streaming, allocation-free C99 CRC32 over bytes, including standard `123456789 → CBF43926` regression.
- `tools/make_release_manifest.py`: generate JSON manifest with exact release, CRC32 and byte count for all staged files. Runs on packaging host, not AmigaOS.
- `tools/verify_release_manifest.py`: offline package validation before distribution; checks version and every file's size/CRC32.
- `dolly_resources_scan_optional`: read-only decision layer rejecting absent/unreadable/wrong-release/wrong-ABI/wrong-size/wrong-CRC files. An OS file-probing adapter must still calculate the actual checksum and bind it to the actual file. Merely setting a boolean `integrity_ok` from untrusted metadata is insufficient.
- Tests are registered in CMake; **current full CTest and native 68000 results not independently confirmed**.

## Example packaging flow
```sh
python3 tools/make_release_manifest.py build/staging \
  --release Dolly-0.1.0+release-id --output build/staging/manifest.json
python3 tools/verify_release_manifest.py build/staging \
  build/staging/manifest.json --release Dolly-0.1.0+release-id
```

This assumes a prepared staging directory. The version string is illustrative, not a claim that this release exists. Production packaging must generate it once, from the actual release metadata, and compile the same identifier into the executable.

### Planned AmigaOS verification
Build a small **read-only native checker** which can validate the main Dolly file **before launching it**, using a bounded buffer and known expected release/CRC32. Then validate each *requested* optional plugin/resource before loading it. On failure never guess a path or load it anyway. The main program remains usable with other optional features absent.

See [Issue #19](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/19), [installation](INSTALLATION.md), [Engineering Journal](ENGINEERING_JOURNAL.md).
