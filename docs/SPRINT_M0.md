# Dolly — Milestone M0 status

**Goal:** public, tested, non-destructive foundations for a future Motorola 68000 / AmigaOS 2.04 utility.

## Implemented
- [x] Public GitHub repository, MIT, Retroware and engineering journal
- [x] Portable read-only BlockSource callback API and POSIX file adapter
- [x] Byte-range validation and backend-error regression tests
- [x] Bounded preliminary RDB/PART parser with checksum and cycle checks
- [x] Primary MBR + PiStorm-style 0x76 nested RDB discovery, range checks
- [x] Separate `dolly-layout` and `dolly-rdb` read-only inspectors
- [x] IFF FORM DOLY structural scanner and `dolly-iff` inspector
- [x] 32/64-bit Amiga device read planner with NSD capability gate
- [x] Prototype `OpenDevice/DoIO` adapter exercised in fake Exec test
- [x] Synthetic CLI integration smoke test
- [x] Host CTest suite: 8/8 locally with GCC; 7/7 Clang before adding CLI smoke; 7/7 sanitizer suite before adding CLI smoke
- [x] CI configuration: GCC, Clang and sanitizer jobs (actual GitHub run status unconfirmed)
- [x] First RDB de_MaxTransfer classification and CLI warning, with 0x1FE00 as conservative reference
- [x] Standalone MaxTransfer classifier tested locally with GCC/ASan/UBSan; full integration build remains to be confirmed

- [x] Preliminary whole-device RDB/MBR coexistence warnings and overlap flags
- [x] Additional RDB/PART collision auditing and minimum checksum coverage validation (partial local regression tests)

## Recent M0 quality improvements (2026-11-17)

- [x] RDB/PART collision audit and CLI warnings (Issue #12; reserved metadata still incomplete).
- [x] Read-only detection of both MBR and disk-wide RDB, including overlapping ranges and damaged candidates.
- [x] Reject checksums that do not cover the required fields in RDSK and PART.
- [x] Request 64-bit host `off_t` for large-file access; real 32-bit host test remains open.
- [x] Add deterministic 600-iteration MBR/RDB mutation-stability test.
- [x] Keep assertions enabled in GCC/Clang CTest executables even in Release builds.

**Locally verified:** reconstructed earlier source snapshot + new RDB changes: GCC CTest **6/6 PASS**, GCC Release CTest **6/6 PASS**; standalone RDB collision audit under Clang ASan/UBSan **PASS**. **Not equivalent to** a complete current GitHub checkout or native 68000 validation.

## Early Sheep Inventory foundation (non-blocking M0 experiment)

- [x] Read-only streaming field providers for RDB/PART (12), primary MBR (7) and RDSK header (7), with provenance and units.
- [x] Preliminary host `dolly-inventory` CLI using MBR, RDSK and nested PART including de_Mask and de_NumBuffers.
- [x] Local GCC, Clang and ASan/UBSan standalone unit checks, including absent fields, overflow and cancellation.
- [ ] Full current-repository CTest/Actions validation after integration.
- [ ] Native 68000 / GadTools window and broader device/volume field providers (future UX milestone M5; Issue #13).

## Unfinished / gating native release
- [ ] Compile with installed real NDK and a 68000-compatible toolchain
- [ ] Test in WinUAE and on actual AmigaOS 2.04+
- [ ] Verify physical capacity detection, DMA alignment and controller Mask/MaxTransfer
- [ ] Compare RDB MaxTransfer to active mountlist/DosList and driver-specific limits
- [ ] Finish TD64 fallback strategy and media-change handling
- [ ] Handle hybrid MBR/RDB variants and full RDB reserved metadata
- [ ] Avoid large stack RDB structures on low-memory machines
- [ ] Implement OFS/FFS structure validation before Smart Clone or repair
- [ ] Finish complete DOLY chunk payload schemas, checksums and segment manifest
- [ ] Refine/import remainder of the 59-story product backlog into GitHub issues/projects

## Repeatable local commands

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/dolly-layout disk.img
./build/dolly-iff image.dly
```

**Safety policy:** no actual device writes are implemented. Reports are preliminary and do not certify bootability or data integrity.

See [engineering journal](ENGINEERING_JOURNAL.md), [AmigaOS I/O](AMIGA_IO.md) and [IFF format](IFF_FORMAT.md).
