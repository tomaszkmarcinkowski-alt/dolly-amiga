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
- Read-only host-side image header inspector (ADF size candidates, RAW/HDF candidate, IFF `FORM DOLY` envelope).
- Read-only 64-bit host file backend (`DollyBlockSource`) with exact/short reads and range checks.
- CTest regression cases including sparse-file access beyond 4 GiB.

**No Amiga executable or actual cloning, formatting, filesystem repair, device write or full DOLY parser exists yet.**

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/dolly-inspect image.adf
```

## Plans and community
See [project scope](docs/SPECIFICATION.md), [M0 sprint](docs/SPRINT_M0.md), [Retroware](RETROWARE.md) and [contributing](CONTRIBUTING.md).

## Licensing
Dolly source code is licensed under the **[MIT License](LICENSE)**. Software will remain free of mandatory fees. Gifts of retro-computing hardware are wholly voluntary and confer no entitlement.

**Please do not use M0 software on valuable media; it is an incomplete prototype.**
