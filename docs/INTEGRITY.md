# Dolly file integrity — built-in $VER and CRC32

**Approved design (2026-11-17):** The main executable performs its **own** CRC32 check after startup. The expected checksum and build ID are embedded **inside the very same executable**. No sidecar manifest, loader or separate verifier is required to start Dolly.

## Standard AmigaOS version identification

Classic AmigaDOS tools identify executable files by the embedded **`$VER:` string**, which includes a program name, numeric major/revision and date. Dolly now carries a provisional M0 string:

```text
$VER: Dolly 0.1 (17.11.2026)
```

An OS 2.x+ user should be able to run `C:Version FILE DOLLY:Dolly` and see the version, once a real Amiga executable has been built. **This has not been tested in WinUAE yet.** For separately distributed Amiga shared-library plugins, the standard `struct Library` fields `lib_Version`, `lib_Revision` and `lib_IdString` also apply. `version.library` exists in some later systems, but it is **not a required dependency** for Dolly's AmigaOS 2.04 minimum.

The $VER identifier and Dolly's internal 16-character build ID serve related but different purposes: compatibility with existing OS tooling versus exact release identity. The release build must derive both from the **same authoritative version metadata** to prevent mismatches.

## Embedded CRC32 record: no circular dependency

`dolly_selfcheck_record` in `src/dolly_selfcheck_embedded.c` is exactly **40 bytes**: 16-byte unique marker `DOLLY:CRC32:V1!!`, 8 ASCII-hex CRC32 characters and 16 bytes of release identification (space-padded). The initial state has the checksum `00000000` and `DOLLY-UNSTAMPED!`.

The post-link tool `tools/stamp_selfcheck.py` verifies that the binary contains one record and a standard `$VER: Dolly` string; writes the release/build identifier into the record; then computes CRC32 of the **entire executable**, treating **only the eight CRC characters** as eight ASCII zeroes; and writes the calculated CRC into those eight bytes. Every other byte, including `$VER:`, the release ID, executable header and program data, participates in CRC32.

When checking its *actual on-disk executable*, Dolly uses exactly the same rule. The file can be read in bounded chunks and the checksum field may cross chunk boundaries. `dolly_selfcheck_embedded_stream()` validates the embedded record and returns PASS only on a matching CRC. A program that was not stamped after linking is rejected.

**Packaging example** (requires a fully linked, otherwise finalized executable):

```sh
python3 tools/stamp_selfcheck.py build/Dolly --release Dolly-0.1
python3 tools/stamp_selfcheck.py build/Dolly --verify --release Dolly-0.1
```

The build ID shown is illustrative, not a published release. **Do not strip, compress, rewrite or alter the binary after stamping** without recalculating its CRC. For a floppy/packed executable, checksum handling must be compatible with the final on-disk representation and any post-processing.

## Startup policy and failure reporting

1. AmigaOS loads and begins executing the main Dolly program.
2. Before granting physical disk operations, Dolly locates its **actual executing file** in Shell or Workbench launch context (do not blindly assume `DOLLY:Dolly` points to that file).
3. It reads that file read-only, using `dolly_selfcheck_embedded_stream()` and the **record loaded with the program**.
4. If the file is absent/unreadable, the record is unstamped/malformed or CRC mismatches, show a clear wolf/sheep warning, the file name, feature impact and recommended reinstall. Keep unsafe disk operations disabled.
5. Optional files are checked for exact release, format and checksum as independently required; their failure disables only the corresponding feature, not the entire GadTools program.

The OS-specific file lookup/stream and final startup gate are **not integrated yet**. A successful standalone test of the C algorithm is not a native launch test. The check can identify accidental disk corruption **after** the executable has started, but cannot prevent a crash in damaged startup code or prove authenticity against malicious replacement.

## M0 status and further work

Implemented and published: version-tag C source, embedded CRC record, host post-link stamping tool, streaming CRC verifier, synthetic C tests and Python stamping tests. Locally reconstructed exact C subset passed GCC, Clang and GCC ASan/UBSan; post-link stamping and verification passed on a local host executable.

Remaining: unify release metadata generation; real m68k AmigaOS compilation, native own-file lookup, GadTools warning and disk-operation gating; optional asset checksum metadata embedded into executable rather than a runtime sidecar. Any host manifest tools in `tools/` are optional *packaging QA only*, not a file Dolly needs to launch.

See [Issue #19](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/19) and [Engineering Journal](ENGINEERING_JOURNAL.md).
