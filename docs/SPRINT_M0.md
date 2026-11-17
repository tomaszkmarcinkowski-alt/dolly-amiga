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

- [x] Read-only streaming field providers for RDB/PART (17), primary MBR (7) and RDSK header (7), with provenance and units.
- [x] Preliminary host `dolly-inventory` CLI using MBR, RDSK and nested PART including de_Mask and de_NumBuffers.
- [x] Local GCC, Clang and ASan/UBSan standalone unit checks, including absent fields, overflow and cancellation.
- [ ] Full current-repository CTest/Actions validation after integration.
- [ ] Native 68000 / GadTools window and broader device/volume field providers (future UX milestone M5; Issue #13).

- [x] Preliminary streaming Sheep Inventory advisory codes: MaxTransfer, RDB overlap, de_SectorPerBlock, zero Mask and missing DosType.
- [x] Standalone GCC/Clang tests for RDB and partition notice feeds, including early stop.
- [x] Signed BootPri from RDB and local GCC/Clang/ASan/UBSan unit tests of the Inventory stream.

## First native-only milestone — 2026-11-17

- [x] Separate `DOLLY_NATIVE_ONLY` CMake source list with no POSIX file adapter or physical-device backend.
- [x] Memory-only `dolly-native-smoke --selftest` source and M68k cross-compiler toolchain preset.
- [x] Host CI job added for the native-only source subset (GitHub run result not yet confirmed).
- [ ] Actual m68k AmigaOS compiler/NDK build, WinUAE run and hardware resource measurements.

## DMA policy and host native smoke — 2026-11-17

- [x] New `dolly_dma_policy` checks raw I/O buffer address mask, alignment, aperture overflow **and holes inside sparse masks**; 6000 deterministic oracle cases passed locally.
- [x] Experimental Amiga backend now refuses unverified DMA profiles and optionally requests `MEMF_24BITDMA`.
- [x] Mock Exec regressions cover missing READ64 capability, partial reads and injected I/O errors; reconstructed subset passes GCC/Clang/ASan/UBSan locally.
- [x] Native-only memory smoke extended with DMA policy checks; reconstructed subset passes GCC/Clang/ASan/UBSan locally.
- [ ] Complete current-`main` CTest/Actions verification and native m68k/AmigaOS execution.

## Guided DMA troubleshooting (2026-11-17)

- [x] Structured cause + up to three safe next steps, stable code and blocked-operation status for DMA failures.
- [x] Portable `dolly-dma-advice` CLI available in host and native-only build configuration; EN / PL ASCII fallback catalogs.
- [x] Added catalog unit tests and Python CLI smoke tests; complete current-main CTest execution still requires verification.
- [ ] Display these steps automatically in native CLI/GadTools with proper PL/EN Locale support.

- [x] Shared Sheep Inventory warnings now carry a safe suggested check; CLI displays it after warning text.
- [x] CLI smoke assertion added for a visible MaxTransfer troubleshooting hint; current full CTest result unverified.

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

## GUI architectural groundwork (future M3/M5; 2026-11-17)

- [x] Freeze MVP scope at **GadTools only**; ReAction/MUI backends move to independent post-MVP milestones; keep optional backend contract decoupled.
- [x] Define 640×256/Topaz8 geometry preflight and font preference order custom → actual Screen Font → Topaz/8.
- [x] Add host-side unit tests of seven view types, 640×256/200 and 5000 generated geometry/font cases (not yet confirmed in full CI).
- [x] Add portable screen event reducer and tests of preferences, font, close/reopen and coalescing.
- [x] Publish experimental AmigaDOS V36+ `StartNotify` adapter for ScreenMode/Screen Font prefs; an opt-in NDK compilation target is available.
- [ ] Verify native NDK build, add optional `screennotify.library` integration from its real SDK, connect actual GadTools event loop, and test visual pixel fit using WinUAE.
- [ ] Confirm all dialogs and long warnings fit 640×256 with real text widths and window borders.

This GUI work does not make M0 media operations safe or replace the pending native 68000 validation.

## DOLLY: resource root (M0) / installer (future packaging)

- [x] Adopt `DOLLY:` as the logical installation root for all application-owned resources.
- [x] Add bounded portable `dolly_path_resource()`; reject absolute/device paths, relative traversal, malformed components and excessive lengths.
- [x] Integrate the resource-path self-test in host CMake and the native-only memory smoke.
- [x] Reproduce and fix a NUL-terminator parsing bug before merge; local GCC/Clang/ASan/UBSan regression test **PASS** on the matching module.
- [ ] Build and validate the **classic Amiga Installer** script, session/persistent Assign and safe reinstallation (Issue #16). This does not gate M0.
- [ ] Build entire current `main` and validate the changed m68k smoke source with an actual AmigaOS SDK.


## Optional asset and sourced DMA-help milestone (2026-11-17)

- [x] Add compatibility gate for optional resources: exact current Dolly release/build, per-resource format version and verified integrity result.
- [x] Distinct missing, unreadable, unverified, wrong-release and wrong-format notices, with path and disabled capability; core may continue.
- [x] Implement in-memory profile matcher and sourced DMA advice composition, requiring a verified compatible database and unique controller/driver/hardware match.
- [x] Unit tests for compatible/outdated/unversioned/corrupt/ABI-mismatched resources and verified/unverified controller advice written.
- [ ] Compile/test current entire repository and validate these additions on an m68k AmigaOS build.
- [ ] Implement file-bound version verification in actual AmigaDOS loader and wire notices into the GadTools 640×256 UI.
- [ ] Build real source-cited controller profile datasets; independent controller/DMA verification is still required.

## CRC32 + release identity, profile-local settings (2026-11-17)

- [x] Frozen simple policy: **exact Dolly build/release ID + CRC32**, main binary and optional assets; ABI/schema for add-ons. Detects accidental corruption, **not intentional tampering**.
- [x] Added portable streaming CRC32 and host-side release generator/verifier, including file-size checks.
- [x] Optional asset gate requires expected release, per-file format/API, CRC32 and size. Wrong files disable only their dependent features; no silent execution.
- [x] Added safe device-profile settings proposal: may reduce a Dolly-local block-aligned transfer chunk, never increase it, never auto-authorize DMA or modify OS/RDB.
- [x] Added corresponding test cases and CMake targets; local isolated CRC32 C99 tests passed under GCC and Clang.
- [ ] Verify complete current `main` with CMake/CTest and native cross compiler. A reconstructed subset is not sufficient.
- [ ] Integrate **in-process main Dolly self-check after startup**, actual AmigaDOS program-file lookup and CRC32 file stream, file-bound version metadata and GadTools notices (no separate launcher).
- [ ] Device-proposal review/confirm/rollback and real controller validation.
- [ ] Separate future **Dolly Rescue** GadTools-only bootable floppy (Issue #20) does not gate M0.

- [x] In-process `dolly_selfcheck_embedded_stream()` and decision tests added to normal and native-only CMake modes.
- [ ] Wire self-check to the actual AmigaDOS Dolly executable before enabling disk operations; ensure failures block device actions without a second program.

## AmigaOS standard version + embedded executable CRC32 (2026-11-17)

- [x] Add a standard `$VER: Dolly 0.1 (17.11.2026)` tag so classic AmigaDOS `C:Version FILE` can identify the executable (host source, not WinUAE verified).
- [x] Add a 40-byte embedded CRC32/build-ID record and allocation-free, bounded self-check stream. Calculated CRC32 treats its own eight ASCII hex characters as ASCII zeroes.
- [x] Add post-link `tools/stamp_selfcheck.py` with a mandatory `$VER: Dolly` presence check, verify mode and corruption/duplicate-record tests.
- [x] Fix parser bug discovered in host testing: an unstamped record must fail immediately, rather than continue into the data reader. Local GCC/Clang/ASan/UBSan regression PASS.
- [x] Run Python stamping/verify tests on a local host binary (PASS); the actual Amiga hunk executable and `C:Version FILE` are still untested.
- [ ] Generate numeric OS version and embedded release ID from one canonical build source; avoid manual divergence.
- [ ] Locate/stream actual Dolly executable in AmigaOS, call selfcheck before disk I/O, and show actionable GadTools failure message.
