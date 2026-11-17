# Milestone M0 — foundation

**Goal:** establish repository, CI, portable read-only image probing and safe partition parsing.

## Sprint backlog
1. [x] Public repository and MIT license.
2. [x] Read-only host image probe, `BlockSource` and tests.
3. [x] Bounded read-only RDB/PART inspector and tests.
4. [x] Primary MBR scan + PiStorm-style MBR type 0x76 nested RDB, with containment and overlap detection.
5. [ ] Review extra hybrid MBR/RDB variations, GPT and EBR (explicitly unsupported today).
6. [ ] Verify native Motorola 68000 build and AmigaOS I/O adapter.
7. [ ] Expand damaged IFF FORM DOLY fixture coverage.
8. [ ] Verify GitHub Actions workflow status.

## Current host checks
`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

Local validation: **4/4 CTest suites passed**.

## Limitations
Only regular file images are supported; no AmigaOS device I/O. Not all RDB reserved structures are checked. GPT/extended-MBR are unsupported. No on-device writes. Native 68000 build remains unverified.
