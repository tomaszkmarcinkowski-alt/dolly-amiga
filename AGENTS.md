# Dolly — instructions for Codex

## Project mission
Dolly is a classic Amiga disk imaging, cloning, recovery and diagnostics project with a humorous sheep personality and professional non-destructive diagnostics. Baseline: **MC68000, AmigaOS 2.04+, GadTools**, including **640x256 / Topaz 8**. Preserve compatibility with low-memory Amigas. Never require FPU/MMU, ReAction or MUI for the MVP.

## Project state at handoff (18 November 2026)
- Repository: `tomaszkmarcinkowski-alt/dolly-amiga`.
- Latest development branch: `feature/m146-partition-details`; **M1.46 is not yet cross-compiled or tested in WinUAE**.
- **M1.41 main-window layout is approved and frozen.** Do not modify placement, height, margins, horizontal-only resizing, Stop/Quit positions, placeholder buttons or progress percentage formatting without explicit user direction. Only advance the SHINEPEN `Dolly Mx.xx` build label as versions change.
- **M1.45 Sheep Inventory was tested** on a four-partition UAE-IDE HDF and correctly displays four partition names in an enabled selectable LISTVIEW. Earlier incorrect `0:`/garbage names and `RDB: 0 partition(s)` originated in numeric format strings; use `%lu` with explicit `(unsigned long)` for unsigned integer display and `%ld` for signed long. See the relevant GitHub history.
- Physical device selection derives driver/unit candidates from AmigaDOS `DosList`. Registration is not proof of responding media. Explicit Query uses read-only `TD_GETGEOMETRY`, SCSI INQUIRY and optionally READ CAPACITY(10). Capacity requires a validated sector size; no blanket DMA safety conclusions.
- M1.44 picker shows Device name, Size and Geometry with provenance. Read-only partition scan supports validated 512-byte-sector RDB/MBR with explicit limitations. Do not fabricate unsupported partitions, geometry or health states.
- M1.46 adds a right-side partition details panel to Sheep Inventory: raw DosType, signed BootPri, MaxTransfer, Mask, filesystem block size, sectors per block, cylinders and partition size, with presence flags for optional DosEnvec fields. Selecting a partition must not trigger additional physical I/O.
- The main Inventory operation starts a separate read-only window; other operations may still be synthetic/demo behavior. **No write/copy/restore/repair backend is certified.**

## Key files
- `amiga/dolly_two_window_demo.c` — main GadTools GUI / operation handling, build identity.
- `amiga/dolly_amiga_endpoint_picker.c` — Source/Destination picker; device/unit/partition and image-file choices.
- `amiga/dolly_amiga_device_query.c` — explicit, read-only device geometry and identity query.
- `amiga/dolly_amiga_partition_scan.c` — bounded native RDB/MBR partition scan.
- `amiga/dolly_amiga_inventory_view.c` — separate Sheep Inventory window and selection details.
- `src/dolly_rdb.c`, `src/dolly_inventory.c` — portable parsing and Inventory metadata foundation.
- `docs/GUI_ARCHITECTURE.md`, `docs/SHEEP_INVENTORY.md`, `docs/TEST_MATRIX.md`, `docs/ROADMAP.md` — architecture, status, regression checks and future plans.

## Development and review rules
1. **Read existing code before editing.** Prefer small, reviewable changes on a new version branch. Keep README and docs consistent with reality.
2. Treat user-approved layout and previously working AmigaOS behavior as regression constraints. In particular, do not disable an interactive LISTVIEW to avoid crashes/garbage or misclassify broken data as empty media.
3. Preserve strict bounds and proper Intuition/GadTools ownership: copy `IntuiMessage` fields **before** `GT_ReplyIMsg`; detach/replace LISTVIEW labels safely; maintain node and string storage for the lifetime of the gadget; release gadgets before freeing their resources.
4. Read-only first: do not issue write/format commands, silently modify RDB or mount configuration, auto-probe arbitrary units, or treat inferred DMA constraints as verified. Display unknown/unsupported states distinctly from zero or empty results.
5. Keep C99 and 68000/AmigaOS 2.04 compatibility. Existing native build uses `-m68000 -Wall -Wextra -Wpedantic -Werror`; avoid undefined behavior and unsafe sprintf/strncpy truncation. Format 32-bit AmigaOS integers with explicit matching types.
6. Test host code with `cmake`/`ctest` where applicable, and cross-build the native target in the user's established Docker m68k toolchain. **Do not claim WinUAE or real-hardware success unless the user confirms it.**
7. Flag limitations of the native partition reader (currently 512-byte sectors, bounded blocks, partial hybrid/NSD/TD64 support), and do not assume reported metadata equals active filesystem/controller settings.
8. Preserve Dolly personality: sheep-themed `Sheep Counter` and `Sheep Inventory`, system theme pens rather than hardcoded colors, professional diagnostics.

## Existing developer workflow (user machine)
WSL repo: `/home/alt/projekty/dolly-amiga-memory`
Docker image: `amigadev/crosstools:m68k-amigaos`
Native target: `dolly-two-window-demo` in `build-m68k-m128`.
WinUAE copy path used by user: `/mnt/c/Dolly-Test/dolly-two-window-demo`.
Codex Cloud might not contain this SDK/Docker image by default; inspect the environment rather than assuming it can compile m68k code.

## First Codex task
1. Inspect the M1.46 diff against M1.45 and the Inventory selection/painting path.
2. Audit optional DosEnvec field offsets and reported values against the actual RDB on-disk format; add regression fixtures if possible.
3. Fix any compilation, GadTools selection, repaint, clipping, size/stack or memory-lifetime problems **without changing the approved main-window GUI**.
4. Run available host tests, cross-compile when an m68k AmigaOS SDK is available, and report honestly what cannot be verified.
5. Prepare a focused patch/PR for user review, not a silent merge to main.

This file records engineering context, not proof of M1.46 test success.
