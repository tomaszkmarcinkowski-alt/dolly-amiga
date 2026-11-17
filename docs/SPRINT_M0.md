# Milestone M0 — foundation

**Goal:** establish repository, CI, portable read-only image probing, then design and test 64-bit BlockSource interfaces.

## Sprint backlog
1. [x] Public repository created.
2. [x] Initial host-side read-only header probing prepared.
3. [x] Publish source and configure CI (workflow results need checking separately).
4. [x] Define preliminary BlockSource I/O API with explicit error semantics and host-side unit tests.
5. [ ] Document 68000 compiler/toolchain build and verify native executable.
6. [ ] Add malformed/truncated IFF FORM DOLY fixtures and tests beyond current envelope checks.
7. [ ] Begin non-destructive RDB discovery with boundary checks.
8. [x] Adopt MIT license; contributions policy documented.

## Definition of Done
- Feature builds and automated tests pass on supported CI host.
- No unreviewed writes or device access.
- Error paths and input-size boundaries tested.
- Docs reflect actual implemented scope, not planned behavior.
