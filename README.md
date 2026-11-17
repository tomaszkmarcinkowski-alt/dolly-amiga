# 🐑 Dolly — Disk Imaging, Cloning & Recovery for Amiga

**Public development repository • Milestone M0 • no usable Amiga release yet**

**No sector left behind. No sheep uncounted.**

Dolly is a free, community-developed disk imaging, disk cloning, backup/restore and filesystem recovery project for classic Commodore Amiga computers.

## Goals
- AmigaOS 2.04+, Motorola 68000 baseline, no FPU or MMU required.
- CLI plus GadTools GUI and optional ReAction GUI.
- Read/write ADF, HDF, RAW; IFF `FORM DOLY` image container with segmented/compressed data.
- RDB, nested/hybrid MBR/RDB, PiStorm/Emu68 layouts and later GPT.
- RAW cloning and filesystem-aware resizing for OFS/FFS (`DOS\0`–`DOS\7`).
- Read-only DiskDoctor analysis followed by controlled recovery/repair.
- Disk-based scratch, optional AddBuffers with consent and rollback, safe/experimental Berserk modes.
- Dedicated **Sheep Counter** 320×256 progress screen with error counters, sheep mascot and optional 8-bit audio.

## Current implementation (M0)

This is **experimental development code, not a usable Amiga release**.

- Portable, read-only `DollyBlockSource`, POSIX file adapter and separate experimental AmigaOS device adapter (OpenDevice/DoIO).
- Conservative device read planner with 4 GiB boundary checks and verified NSD capability requirement; **TD64 not implemented**.
- Host image probing: ADF size candidates, RAW/HDF candidates and IFF FORM DOLY recognition.
- Read-only RDB/PART inspector, primary MBR and nested 0x76 RDB discovery; preliminary range validation.
- Streaming `FORM DOLY` envelope/chunk parser (**not a full backup/restore format**).
- CLI tools: `dolly-inspect`, `dolly-rdb`, `dolly-layout`, `dolly-iff`.
- Synthetic regression tests plus a mocked Exec device. Host test status: **8/8 CTest suites locally** (GCC; native 68000 has **not** been tested).
- Public engineering journal for bugs, contradictions and unexpected hardware lessons.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

./build/dolly-inspect image.adf
./build/dolly-rdb drive.hdf 0 512
./build/dolly-layout pistorm.img 512
./build/dolly-iff backup.dly
```

**No native Amiga executable, disk writer, Smart Clone, filesystem repair or full DOLY payload format exists yet.** A synthetic/mock host test is *not* hardware validation.

See [AmigaOS I/O limitations](docs/AMIGA_IO.md) and [IFF format state](docs/IFF_FORMAT.md).

## Plans and community
See [project scope](docs/SPECIFICATION.md), the [60-story backlog](docs/BACKLOG.md), [M0 sprint](docs/SPRINT_M0.md), [roadmap](docs/ROADMAP.md), [Polish introduction article](docs/ARTICLE_PL.md), [Retroware](RETROWARE.md), [contributing](CONTRIBUTING.md) and the [Post-MVP Flock Manager epic](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7).

## Engineering journal

Read the chronological [Engineering Field Notes](docs/ENGINEERING_JOURNAL.md) for confirmed bugs, open hypotheses, fixes, regressions, hardware quirks and project easter eggs.

## RDB collision and hybrid diagnostics

The experimental host utilities also detect collisions between RDB partitions, `RDSK` and `PART` metadata. `dolly-layout` reports when MBR and a whole-device RDB are both present; neither view is automatically assumed safe for writes. This is preliminary inspection, not full hybrid-drive support. See [Issue #12](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/12).

## Motorola 68000 smoke milestone (source only)

An experimental, POSIX-free `dolly-native-smoke` target now exercises synthetic RDB parsing and read planning **in memory**; it cannot access disks. Native 68000 compilation and WinUAE execution are still pending. See [Native smoke test and cross-build instructions](docs/NATIVE_SMOKE.md).

## Sheep Inventory (early host prototype)

The preliminary `dolly-inventory IMAGE [BLOCK_BYTES]` command displays MBR/RDB partition details using a **streaming, allocation-free field API** with units, provenance, evidence levels (declared/observed/derived/estimated) and explicit unavailable values. The early field providers cover RDSK, PART (including signed Boot Priority) and MBR; device and filesystem support are planned. Shared non-destructive warning streams cover MaxTransfer and preliminary RDB overlap checks. A GadTools window and richer device/volume providers are future work. See [Sheep Inventory design](docs/SHEEP_INVENTORY.md) and [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13).

## MaxTransfer audit

Dolly's experimental read-only RDB tools now report per-partition `de_MaxTransfer` values and flag settings above conservative `0x1FE00`. See [MaxTransfer diagnostics](docs/MAXTRANSFER.md); no setting is changed automatically.

## Licensing
Dolly source code is licensed under the **[MIT License](LICENSE)**. Software will remain free of mandatory fees. Gifts of retro-computing hardware are wholly voluntary and confer no entitlement.

**Please do not use M0 software on valuable media; it is an incomplete prototype.**
