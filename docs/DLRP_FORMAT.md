# Dolly Restore Pointer (DLRP)

A DLRP is an operation checkpoint for unexpected failures in imaging
and supported Restore Image and sector-copy operations. Smart Copy resume
requires a separate record of its range plan. The live Create Image
operation offers one after a read, write or container-finalization error
when the staged output is available. Saving preserves the committed
prefix; any uncommitted tail must be removed before saving. Cancel
discards the staged output. Surface Scan and manual Stop/Quit do not
offer checkpoints.

## Version 1 Create Image checkpoints

Use `FORM DLRP` with big-endian integer fields and conventional IFF
length and even-byte padding. Strings are AmigaDOS byte strings without
a terminating NUL. The order is HEAD, GEOM, SRCE, DRVR, NAME, optional
imaging chunks OUTP, DEST and PART, zero or more EVNT, then CR32.

| Chunk | Fields |
|---|---|
| HEAD (108 bytes) | u32 version (1), operation (1 reserved (legacy scan), 2 = create image), endpoint kind, driver unit, physical sector bytes, effective batch bytes. u64 start LBA, extent sectors, completed sectors, disk capacity in sectors. u32 errors, retries, remaining fallback sectors, retry pending (0/1). u64 omitted events. u32 anchor flags, first-sector CRC32, last-sector CRC32, effective Mask, BufMemType, MaxTransfer, selected partition index. |
| GEOM (36 bytes) | u32 capacity source, cylinders, heads, sectors per track, I/O hint presence mask. u64 selected partition start and length in bytes, zero when not applicable. |
| SRCE / DRVR / NAME | Source selection path, driver name, and device identification returned by Query. |
| OUTP (24 bytes) | u32 requested image format, exact staged byte count, DOLY codec, planned DOLY file bytes. u64 completed DOLY sectors. DOLY-specific values are zero for RAW/HDF. |
| DEST / PART | Absolute final destination path and absolute path to the retained staged image. Both must refer to the original output drawer. |
| EVNT (20 bytes, repeatable) | u64 absolute LBA, u32 sector count, event kind and block-status code, in chronological order. |
| CR32 (4 bytes) | CRC-32/ISO-HDLC over all preceding file bytes, including the FORM header and IFF padding. Excludes the CR32 chunk itself. |

HEAD uses the stable endpoint codes 1 (unit), 2 (partition) and 4
(logical); OUTP uses RAW 0, HDF 1, IFF DOLY 3, Fast 4 and Compact 5.
Anchor flags bit 0 and bit 1 indicate whether the first and last source
sectors were read successfully. Anchors aid identity checks. They cannot
establish media identity or detect changes elsewhere on the device.
The I/O presence mask uses bit 0 for Mask, bit 1 for BufMemType and bit
2 for MaxTransfer.

The cursor is the first sector after the committed prefix. Only a fully
written batch contributes to the
cursor; the file is closed before committing its checkpoint. The staged
DOLY stream can have an unfinished FORM length. OUTP records the exact
append offset and immutable encoding choices for reconstruction. The
checkpoint is staged and renamed into place after successful close;
existing files are refused.

## Resume validation

The image reader checks version, chunk order, bounds, cursor, CRC and
value ranges before opening endpoints. It resolves source aliases
again and checks device identity, capacity, geometry, extent, and all
recorded anchor sectors. At least one saved anchor is required. It
rechecks destination ownership and free space, opens the existing
stage exclusively and compares the full retained prefix against the
source before offering Resume. The final filename must be unused.
The valid staging path is the original destination with `.part/image`
appended. If a resumed append is stopped or fails, Dolly attempts to
truncate back to the original cursor while retaining the checkpoint.

For a single-file DOLY checkpoint, the existing FORM contains the
planned length, and the staged file ends at the last committed chunk.
Dolly checks the header, the block sequence and the CRC of each chunk,
compares the decoded prefix with the source, then reattaches the writer
with the recorded chunk size and compression. On completion, compressed
images receive their final FORM length. Rollback restores the planned
length if finalization had started. Split images use per-part recovery.

## Version 2 device-write checkpoints

Copy and Restore use HEAD version 2 and operation codes 3 and 4.
Chunk order is HEAD, SRC2, DST2, SRCE, DEST, DRVR, TDRV, NAME, TNAM,
CR32. Version 1 Create Image continues to use the format above.

| Chunk | Fields in order |
|---|---|
| HEAD (56 bytes) | Six u32 values: version, operation, sector bytes, batch bytes, verify flag, terminal transfer state. Three u64 values: committed bytes, potentially modified bytes beginning at that cursor, total bytes. Two u32 values: read status and write status. |
| SRC2 / DST2 (76 bytes each) | Four u32 values: endpoint kind, unit, partition index, effective Mask. Three u64 values: absolute byte start, length, capacity. Seven u32 values: BufMemType, MaxTransfer, image format (source file only), first and last sector CRC32, capacity source, cylinders. Two u32 values: heads and sectors per track. |
| SRCE / DEST | Source and destination selection paths. A file source uses the canonical path when its lock can resolve it. |
| DRVR / TDRV / NAME / TNAM | Source driver, target driver, source device name, target device name. The source driver and name are empty for a file. |
| CR32 | Same checksum coverage as version 1. |

The target CRC anchors describe the media **before** the transfer.
Dolly requires the original last-sector CRC to match, even when that
sector falls inside the uncertain batch. If it changed, version 2
cannot distinguish a late partial write from replacement media.
The original first-sector CRC is also checked when the checkpoint
records zero committed and zero uncertain bytes.
For an IFF DOLY Restore, version 2 stores the first decoded sector
CRC in both source anchor fields. The subsequent full image check
validates container integrity. These anchors and the prefix comparison
can detect changes in the checked regions, but do not identify media
uniquely or establish the original contents of an unwritten suffix.

Resume reopens the saved endpoints using the ordinary write preflight,
including overlap and mount checks. Warning Review shows the exact
destination write cursor and the uncertain range. Before issuing a
write, Dolly compares the committed prefix byte for byte with the
source and reads the entire uncertain range. Restore also runs its
ordinary full IFF DOLY validation when applicable. Dolly rechecks the
last anchor immediately before replay. The uncertain batch is rewritten
in full and verified by readback, regardless of the saved verify flag.
The flag governs subsequent batches. A retry that fails before making
progress can reuse the same DLRP when its original anchors still match.
