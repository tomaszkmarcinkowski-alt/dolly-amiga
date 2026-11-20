# AmiPart source review

Reviewed ChuckyGang/AmiPart at commit
[`0f77f10`](https://github.com/ChuckyGang/AmiPart/tree/0f77f10d206415fdc5602d48a83ce412ab84322d).
The upstream project uses the MIT license. Retain John Hertell's copyright
and permission notice if Dolly incorporates substantial portions of the code.
This review adds no AmiPart code to Dolly.

## Discovery and object identity

| AmiPart code | What it does | Relevance to Dolly |
| --- | --- | --- |
| [`Devices_Scan` and `Devices_GetUnitsForName`](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/devices.c#L193) | Discovers drivers from AmigaDOS startup messages and Exec's loaded DeviceList. An explicit unit probe uses a per-driver upper bound, progress cancellation between units, and INQUIRY, RDB and geometry data. Shows driver version and revision and the RDB LAST DISK / LAST LUN flags. | Dolly already has both discovery sources and bounded unit probing. Useful additions are version/revision, separate driver and RDB capacity records, and diagnostics for LAST flags. A per-driver range can help an explicitly selected slow or unusual controller. An opt-in view of loaded device candidates can expose names that Dolly’s storage filter misses. Keep manual unit entry and validate startup BPTRs through Dolly's existing helpers. |
| [`match_partition` and `UnmountPartition`](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/quickformat.c#L443) | Matches the RDB partition name and its Exec driver/unit against a DOS startup message before sending shutdown packets. | The driver/unit qualification prevents similarly named partitions on another device from being affected. Dolly already resolves the opened file's lock to its actual handler and compares driver, unit and range. Keep that check for aliases and images reachable through assigns. The AmiPart mount-list exporter is not a resolver for those file paths. |
| [`PartClone_ValidateFootprint` and `PartClone_FindGap`](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/partclone.c#L166) | Compares physical extents even when the per-partition geometries differ and finds a free destination span. | Useful reference for a Clone and resize placement planner. Dolly should retain checked 64-bit byte arithmetic and explicit physical and filesystem block units. |

## Controller compatibility

- `BlockDev_Open` and the raw I/O helpers in `rdb.c` combine
  TD_GETGEOMETRY, SCSI READ CAPACITY(10), NSD capability queries, legacy
  TD64, CMD_READ/WRITE, and SCSI READ(10) where supported. The reader
  remembers a working path. This is a useful reference for capability
  detection. A transport fallback should distinguish an unsupported
  command from a media error and validate the returned status and length.
  [BlockDev and raw I/O](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/rdb.c#L240)
- The source documents concrete compatibility cases. Some `lide.device`
  passthrough commands transfer a whole 512-byte sector. `warpATA.device`
  may report error 20 for legacy TD64. An A4000 driver accepted TD_WRITE64
  but failed the operation. A3000 SDMAC cases include a reported
  four-byte DMA shift with SCSI WRITE and a CMD_READ caching issue. These
  are controller and driver-version test cases for Dolly. They do not
  establish a safe write path for every driver with the same name.
  [Capacity and passthrough buffer](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/rdb.c#L99)
  · [Write fallback cases](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/rdb.c#L768)
- `ATA_Identify` has a SAT ATA PASS-THROUGH(12) fallback after (16)
  and a full-sector buffer. Detected CHS placeholders, LBA28/LBA48
  and transfer-mode fields are useful evidence for Inventory. The
  fallback still relies on `HD_SCSICMD` and driver support.
  [ATA IDENTIFY implementation](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/ataident.c#L49)
- AmiPart can infer a boundary by probing the last readable block.
  A readable block alone cannot distinguish correct addressing from
  offset wraparound, sector aliasing or misleading emulated geometry.
  Treat this as optional read-only evidence and keep Dolly's current
  driver-reported write bounds.
  [Capacity probe](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/rdb.c#L181)

## Filesystem resize during copy

AmiPart separates three operations. `RDB_GrowToDevice` extends the disk's
RDB geometry. The per-filesystem engines update on-disk filesystem
structures. `PartClone_PartToPart` handles a sector copy and selected
filesystem fixups.

| Filesystem | Implemented growth | Implemented shrink and conditions |
| --- | --- | --- |
| [OFS/FFS DOS0–DOS7](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/ffsresize.c) | Relocates the root to the new midpoint, updates pointers and bitmap coverage, and marks the bitmap for filesystem validation after mount. The clone path passes the exact source filesystem block count. | Walks the allocation bitmap and can relocate supported metadata. Rejects occupied user data in the removed range and a new root location that cannot safely be used. |
| [PFS/PDS variants](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/pfsresize.c) | Adjusts the root cluster size and allocation fields after verifying reserved bitmap/index capacity. Supported layouts depend on root metadata as well as the RDB DosType. | Requires the removed user-block tail to be free. The stored size, free count and allocation cursor must fit the resulting geometry. The clone-to-larger path clears a size-consistency flag and leaves growth for a later operation. |
| [SmartFileSystem](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/sfsresize.c) | Updates bitmap coverage and both root blocks, including absolute byte positions and sequence/checksum fields. Cloning to a larger partition adjusts its RDB span towards the source size and grows only the remainder to match the new envelope exactly. | Requires free space in the removed range apart from known metadata, space for the new end root, and the retained bitmap below the new end. A bitmap previously moved high in the partition can prevent shrink. |

[`ShrinkInfo_Run`](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/shrinkinfo.c#L28)
is the best starting point for read-only analysis. It reports the minimum
supported size using filesystem allocation data. Free-space totals alone
cannot prove that the end can be removed. AmiPart's direct clone rejects
a smaller destination. Its separate shrink engines do not pack or move
arbitrary user data out of the tail.
[Clone and post-copy fixups](https://github.com/ChuckyGang/AmiPart/blob/0f77f10d206415fdc5602d48a83ce412ab84322d/src/partclone.c#L702)

A staged adaptation for Dolly can start with read-only OFS/FFS allocation
analysis, then a clone to a larger offline RDB partition with exactly
matched filesystem block geometry. For shrink during copy, analyze the
full source, stage metadata edits against a writable copy or overlay,
and commit only a verified result that fits the destination. Validate
disk bounds, adjacent partitions, the new root and bitmap, and include
read-back and mount checks. Keep the source readable and unchanged.
Current raw Restore size handling remains a sector-range operation.

The intended coverage is OFS/FFS, PFS3/PFS2 and SFS. Each family needs its own
layout checks and fixture coverage before enabling writes. OFS/FFS can
validate the common planning and write path first. PFS and SFS need
family-specific grow/shrink qualification, including reserved bitmap
capacity, absolute SFS offsets and remount checks with real filesystem
handlers.

## Host checks

AmiPart built as a Linux CLI and passed its 12-case seeded grow/shrink
loop on OFS/FFS and SFS (seeds 20261120–20261131). Three targeted
PFS fixtures also passed (seeds 20261132–20261134). The test loop uses
synthetic PFS/SFS volumes. Its FFS test code rebuilds the bitmap between
operations to stand in for the AmigaOS validator. The results cover
image-file structure and content checks rather than native handler or
controller qualification.
