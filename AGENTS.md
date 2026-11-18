# Dolly — instructions for Codex

## Project mission
Dolly is a classic Amiga disk imaging, cloning, recovery and diagnostics project with a humorous sheep personality and professional non-destructive diagnostics. Baseline: **MC68000, AmigaOS 2.04+, GadTools**, including **640x256 / Topaz 8**. Preserve compatibility with low-memory Amigas. Never require FPU/MMU, ReAction or MUI for the MVP.

## Current development snapshot (18 November 2026)
- Integration base: `feature/m146-partition-details`.
- **The M1.41 main-window layout is approved and frozen.** Preserve placement, size, margins, horizontal-only resizing, Stop/Quit positions, placeholders and percentage formatting.
- Sheep Inventory adds device and partition details, clipboard copying and RDB/MBR metadata backup. Restore remains disabled.
- Surface Scan is an accepted read-only UAE-IDE prototype with 64-bit reads, retries and LBA/CHS logs.
- Create Image reads the source and writes a staged RAW, HDF or DLY file to a separately verified destination. DLY v1 is raw and v2 supports PackBits and LZSS.
- Host tests, the MC68000 build and FS-UAE checks are recorded in `docs/TEST_MATRIX.md`. Physical-controller qualification remains open.

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

## Original M1.46 handoff checklist (historical)
1. Inspect the M1.46 diff against M1.45 and the Inventory selection/painting path.
2. Audit optional DosEnvec field offsets and reported values against the actual RDB on-disk format; add regression fixtures if possible.
3. Fix any compilation, GadTools selection, repaint, clipping, size/stack or memory-lifetime problems **without changing the approved main-window GUI**.
4. Run available host tests, cross-compile when an m68k AmigaOS SDK is available, and report honestly what cannot be verified.
5. Prepare a focused patch/PR for user review, not a silent merge to main.

Use the test matrix for per-platform evidence and open verification.
