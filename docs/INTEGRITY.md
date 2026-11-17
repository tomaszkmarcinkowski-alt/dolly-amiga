# Dolly integrity — self-check after startup (version + CRC32)

**Accepted decision (2026-11-17):** Dolly itself checks the CRC32 and exact release version of its *own executable file* immediately after its process starts. **No separate bootstrap, launcher or verification executable.** The same version + checksum policy applies to optional resources.

## Startup sequence

1. AmigaOS loads and launches Dolly normally.
2. Before exposing any disk/physical-device operation, the **main Dolly executable** locates and opens its **actual program file** read-only. Do not assume `DOLLY:Dolly` is the file currently executing: a Shell/Workbench may launch a copy elsewhere. Native path resolution must handle both invocation modes and detect ambiguity.
3. Dolly obtains its expected exact release/build ID, file size and CRC32 from the distribution's release manifest and compares release identity with its own compiled build identifier.
4. Dolly streams its executable bytes through `dolly_selfcheck_stream()` using a bounded buffer, comparing actual size/CRC32 with manifest values.
5. On success, the application's normal initialization may continue. On any missing manifest, unreadable self-file, version/size/CRC mismatch or unidentified program path, **the disk-operation gate stays disabled** and the main program presents a clear failure message. The final native GUI flow may allow a limited Help/About or exit; it must not silently proceed with potentially unsafe I/O.
6. Only requested optional resources are subsequently validated; missing/incompatible additions are disabled without preventing base GadTools/core startup.

**The manifest is a data file, not a second executable.** There is no circular checksum embedded inside the executable. The main program has a compiled-in release ID while expected CRC32/size reside in the release manifest.

## Limitations

- The verification runs **after** AmigaOS has already loaded the binary, not before its first instruction. It cannot prevent an early crash if the code itself was corrupted.
- CRC32 catches accidental corruption and mixed releases, **not deliberate attacks**, especially if the manifest can be changed at the same time.
- The checked *disk file* might not precisely match the loaded executable in memory (media changes/altered file after load), another reason this is best-effort integrity checking rather than tamper-proofing.
- Correct behavior when the self-file is unavailable: **clear, blocking warning and no disk I/O** rather than claiming success. Optional plugin failures remain non-blocking.
- Native AmigaDOS startup integration and locating the actual loaded program file are still open. It may be necessary to use process/program directory context carefully; simply reading `DOLLY:Dolly` can verify the wrong binary.

## Current implementation

- `include/dolly_selfcheck.h`, `src/dolly_selfcheck.c`: C99 self-check state machine with read callback, bounded streaming CRC32 and human-readable failures.
- `tests/test_selfcheck.c`: tests of valid data, changed content, truncation, build-version mismatch and interrupted reads.
- `dolly_checksum`: streaming CRC32; `tools/make_release_manifest.py` and `tools/verify_release_manifest.py` are host-side package utilities and remain useful.
- CMake includes tests in normal and native-only source modes. **No claim of real AmigaOS native execution or final production startup integration yet.**

Example concept:

```text
Owce wykryly problem z programem!
Plik: [actual Dolly executable path]
Problem: niezgodna suma kontrolna CRC32.
Skutek: funkcje odczytu/zapisu urzadzen zostaly zablokowane.
Rozwiazanie: zainstaluj ponownie pliki z jednego, zgodnego wydania Dolly.
```

See [Issue #19](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/19) and [Engineering Journal](ENGINEERING_JOURNAL.md).
