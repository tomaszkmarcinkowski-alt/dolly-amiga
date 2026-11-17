# Milestone M0 — foundation

**Goal:** establish repository, CI, portable read-only image probing, then design and test 64-bit BlockSource interfaces.

## Sprint backlog
1. [x] Public repository created.
2. [x] Initial host-side read-only header probing prepared.
3. [x] Publish source and configure CI (workflow outcome not independently confirmed here).
4. [x] Preliminary host BlockSource API with 64-bit offset and unit tests.
5. [x] Initial bounded read-only RDB/PART parser, shifted-container support and synthetic regression tests.
6. [ ] Validate overlap among partitions and RDB reserved regions; parse nested MBR 0x76 automatically.
7. [ ] Document 68000 compiler/toolchain build and verify native executable.
8. [ ] Expand malformed/truncated IFF FORM DOLY fixture coverage.
9. [x] Adopt MIT license and contributor policy.

## Local host validation
- `cmake -S . -B build && cmake --build build`
- `ctest --test-dir build --output-on-failure`
- 3/3 local CTest suites passed after integrating RDB parser.

## Scope clarification
`dolly-rdb` currently scans only a supplied ordinary file; no native Amiga device I/O, filesystem check or disk repair. Shifted RDB requires a user-supplied **byte offset**.

## Definition of Done
- Feature builds and automated tests pass on supported CI host.
- No unreviewed writes or device access.
- Error paths and input-size boundaries tested.
- Docs reflect actual implemented scope, not planned behavior.
