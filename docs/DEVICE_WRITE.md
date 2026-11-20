# Device writes

**Copy → Sector-to-sector** copies between two whole devices or two
in-range RDB partitions with equal byte lengths and sector sizes.
**Restore Image** writes a flat RAW/HDF image to a whole
device or in-range RDB partition. The first write path supports 512-byte sectors
and destination byte offsets below 4 GiB. Classic AmigaDOS file
positions limit the RAW/HDF source to less than 2 GiB.

Dolly checks device geometry, the RDB extent, DMA settings, and logical
overlap. Whole-device Copy treats both devices as raw sectors. Dolly describes
the destination partition table, if recognized, in the warning review.
Partition-level operations require RDB. Whole-device Restore transfers raw
sectors regardless of the partition-table format. IFF DOLY decoding,
filesystem-aware copying and resizing
remain outside this path. For raw sources, Dolly uses DOS Inhibit on active filesystems within
the selected range, then releases the temporary inhibits on completion,
failure or cancellation. The file volume serving a Restore image stays
available for reading. Restore therefore requires the image to reside
on a different unit when the destination must be unmounted.

On a raw destination, Dolly attempts to inhibit active handlers and unmount all DOS partitions
on the selected physical unit. It asks each handler to shut down and
removes the device entry only when the handler has released it. A
successful unmount persists for the session, including after a
cancelled transfer. Reboot to load the current destination partition
table. A failure adds a decision to the same review before proceeding at the user's risk.
The Exec open count is shared across units on the driver. Extra opens at
preflight add a separate **Driver access** item after overwrite approval, even
if the destination has no registered mounts. Dolly checks the selected units' mounts separately and stops if the
approved driver count or the relevant mount state changes.

## Confirmation and transfer

Restore compares the image and destination size, object kind,
filesystem IDs and RDB attributes. Metadata warnings, including missing,
damaged or out-of-range RDB, require explicit approval. A size difference
has its own confirmation and lists the image size, destination size and
difference. Dolly writes only the common sector range. A smaller image
leaves the destination tail untouched and may leave extra capacity unusable.
A larger image loses its tail on restore, which may leave data incomplete
and filesystems corrupt or unreadable. Dolly does not resize partition or
filesystem structures. An invalid write range or unreadable extent
blocks the operation. The image file's actual open handle is
used for the volume overlap check.

Copy and Restore collect their warnings after Start and open one review
window. The left list shows the warnings in order. The right panel shows
details and the action for the current warning. Cancel aborts the whole
operation. The review has no countdown. After approval of every item, Dolly checks geometry and partition selection again, compares
first- and last-sector CRCs, and checks for a busy target. A qualified
ROM or UAE-IDE `scsi.device`, or an NSD driver advertising both
`CMD_WRITE` and `CMD_UPDATE`, is required. Dolly also checks that
`CMD_UPDATE` succeeds before submitting any write. The offline and
open-count checks detect AmigaDOS registration and unexpected Exec
opens. External access that bypasses those interfaces requires an
offline test setup.

The application processes one bounded, sector-aligned batch per step.
Its DMA buffer uses the applicable RDB settings, defaults and manual
overrides. The writer sends `CMD_WRITE` followed by `CMD_UPDATE`.
**Verify writes** adds readback and byte comparison. Progress advances
only after the whole batch succeeds. Pause and Stop take effect
between requests. A failed batch is never retried automatically.

## Interrupted operations

On unexpected I/O failure Dolly reports the first destination LBA of
the failed batch and offers to save an IFF DLRP checkpoint. The
checkpoint records the committed prefix and the range that may have
been modified by the failed write. It can be saved to RAM or a verified
device other than the destination. Stop does not generate a checkpoint.
Resume support remains planned.

## Verification

Host tests cover the portable transfer engine, command selection,
short writes, cache flush and readback failures, and Restore
compatibility rules. The native build targets MC68000. Runtime
validation on disposable Amiga media is still pending.


The filesystem mapping is independent of the partition table format.
Geometry and metadata probes used to discover endpoints precede this
mapping. Surface Scan and partition-table backup use the same raw-read
inhibition policy. Restore RDB offers inhibition and unmount before running its
strict offline checks. DOS file operations keep the file's serving filesystem online.

Whole-disk RAW/HDF restore can reproduce an RDB layout that declares
a partition beyond the driver-reported capacity. The overwrite warning shows
the destination capacity discrepancy. The transfer writes only the common
addressable sector range. It preserves the RDB
as stored in the image and does not extend or repair the affected partition.
