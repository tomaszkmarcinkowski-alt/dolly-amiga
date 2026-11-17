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
See [project scope](docs/SPECIFICATION.md), [M0 sprint](docs/SPRINT_M0.md), [roadmap](docs/ROADMAP.md), [Retroware](RETROWARE.md), [contributing](CONTRIBUTING.md) and the [Post-MVP Flock Manager epic](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7).

## Engineering journal

Read the chronological [Engineering Field Notes](docs/ENGINEERING_JOURNAL.md) for confirmed bugs, open hypotheses, fixes, regressions, hardware quirks and project easter eggs.

## Licensing
Dolly source code is licensed under the **[MIT License](LICENSE)**. Software will remain free of mandatory fees. Gifts of retro-computing hardware are wholly voluntary and confer no entitlement.

**Please do not use M0 software on valuable media; it is an incomplete prototype.**
