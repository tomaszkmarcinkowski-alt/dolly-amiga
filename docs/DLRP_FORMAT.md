# Dolly Restore Pointer (DLRP) v1

A DLRP is an operation checkpoint for unexpected failures in imaging
and, once implemented, Restore and Copy/Clone. The live Create Image
operation offers one after a read, write or container-finalization error
when the staged output is available. Saving preserves the committed
prefix; any uncommitted tail must be removed before saving. Cancel
discards the staged output. Surface Scan and manual Stop/Quit do not
offer checkpoints.

## IFF representation

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

## Requirements for the future resume reader

Validate version, chunk order, bounds, cursor, CRC and value ranges before
opening endpoints. Resolve source aliases again and verify the endpoint,
device identity, capacity, geometry, extent, and available anchors. If
identity remains uncertain, require recovery or an explicit restart.
Recheck destination ownership and free space. For RAW/HDF compare the
retained prefix against the original source before appending. For DOLY
validate complete chunks and their CRCs, verify the decoded prefix against
source data, and reconstruct the writer state. Refuse a changed staged
length or destination path. The future write-side format must also carry
target identity, write verification mode and the potentially modified
range at a failed batch. Require destructive confirmation and repeat
destination checks before writing after resume.

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

The target CRC anchors describe the media **before** the transfer. A
future resume reader must verify the already-written prefix against
the source and account for the potentially modified range before
trusting the cursor. That reader is still planned.
