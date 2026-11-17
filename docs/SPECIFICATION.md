# Dolly — baseline product specification (approved concept)

## Platform and architecture
Target Motorola 68000, AmigaOS 2.04+, with older CLI compatibility considered. GadTools baseline, ReAction when available. Portable parsing core reused by AmigaOS adapter and host tests. CLI, Wizard and Advanced GUI share operation state.

## Devices and formats
Device/unit selection, RDB partition selection; hybrid/nested MBR-RDB and PiStorm/Emu68 awareness, offset overrides and overlapping-range protection. Direct ADF/HDF/RAW image I/O; `FORM DOLY` IFF-based compressed and segmented images. Unknown geometry: suggested AUTO interpretation or fully adjustable MANUAL interpretation. No unsupported certainty claims.

## Clone and recovery
RAW sector clone; filesystem-aware resizable OFS/FFS `DOS\0`–`DOS\7` clone; DiskDoctor read-only analyze, recover to different medium, repair plan and opt-in repair. Bad-sector classification: free, file data, filesystem metadata, allocated/unknown, inconsistent, unknown. Do not call a damaged region safe merely because its bitmap entry is free without corroboration.

## Performance
Configurable read/write/verify retries and adaptive chunk sizes. RAM or disk-backed scratch. OFS/FFS scratch volume may receive an **AddBuffers proposal only with permission**, followed by best-effort restoration. Evaluate physical storage overlap and controller/bus sharing. Optimize for low RAM.

## UX
Localized messages through locale.library with fallback for older OS. Safe default mode and separately confirmed “Dolly Gone Berserk” experimental override; never bypass memory/bounds safety. Persistent human/technical status line. Sheep Counter fullscreen 320×256, current block marked by sheep, error/retry counters directly in cells, optional 8-bit 22 kHz sheep sample, auto-off on low Chip RAM. Wizard can transition to full GUI without losing operation state.

## Non-goals for initial milestone
No destructive operations, native Amiga UI or filesystem repair in M0. PFS/SFS/FAT smart cloning, full GPT write and plugin loading are later milestones.

## Open technical decisions
Exact format chunk schemas, crash-consistent segment manifest, UI rendering constraints, native toolchain portability and final code license require implementation-phase decisions and tests.
