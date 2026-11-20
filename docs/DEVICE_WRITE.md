# Device writes

**Copy → Sector-to-sector** copies between two whole devices or two
in-range RDB partitions with equal byte lengths and sector sizes.
**Restore Image** writes a RAW/HDF or single-file IFF DOLY image, or a split IFF DOLY set selected through its first part, to a whole
device or in-range RDB partition. The write path supports 512-byte sectors.
A destination extent ending above 4 GiB requires the opened driver to
advertise both NSD READ64 and WRITE64. Classic AmigaDOS file
positions limit each source file to less than 2 GiB. A split set can
represent a larger decoded extent. See [large image files](LARGE_IMAGE_FILES.md).

Dolly checks device geometry, the RDB extent, DMA settings, and logical
overlap. Whole-device Copy treats both devices as raw sectors. Dolly describes
the destination partition table, if recognized, in the warning review.
Partition-level operations require RDB. Whole-device Restore transfers raw
sectors regardless of the partition-table format. Filesystem-aware copying
is available through Smart Copy for matching OFS/FFS RDB partitions.
Clone and resize supports a limited FFS write prototype. For raw sources, Dolly automatically uses DOS Inhibit on active filesystems within
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
driver count increases beyond the approved count or the relevant mount state changes.

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
used for the volume overlap check. IFF DOLY is recognized by its container
signature. The decoded sector size must match the destination. After the
warning review, Dolly validates the whole image before it writes any image
sectors. The validation pass can be paused or cancelled.

Copy and Restore collect their warnings after Start. Smart Copy requires verified source isolation before showing the
validated bitmap plan.
The write review uses one window. The left list shows the warnings in order. The right panel shows
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
overrides. Batches entirely within the first 4 GiB use `CMD_WRITE`.
Batches crossing or above that boundary use `NSCMD_TD_WRITE64`.
Both are followed by `CMD_UPDATE`. **Verify writes** adds readback through
`CMD_READ` or `NSCMD_TD_READ64` and byte comparison. Destination rechecks
also use the qualified read command. The legacy `scsi.device` version gate
still applies, including the existing UAE-IDE exception and manual override.
TD64 and direct SCSI writes are pending. Progress advances
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
compatibility rules. The native build targets MC68000. Runtime validation includes an AmigaOS boot from a Smart Copy target in
WinUAE. See the [test matrix](TEST_MATRIX.md) for individual cases.


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

### Name conflicts after copying

After a whole-disk copy, Dolly can offer new device names for the
destination RDB, starting with `DH0 → DH0_0` and checking subsequent
suffixes against the known DOS names and other registered RDB units.
A whole-disk image restore gets the same offer on a detected conflict.
For full OFS/FFS volumes, including partition copies, Smart Copy, and
image restore, Dolly can offer `Work → Copy_of_Work`. It checks the boot
DosType and the root block type and checksum before offering a relabel.
An unformatted partition has no verified volume name to relabel.

The RDB and volume decisions are independent. Warning Review lists old
and proposed names. Cancel retains the names in that group. Dolly
applies accepted changes while the destination remains offline, and
always verifies the metadata write. Reload the partition table or
reboot before accessing the destination under its new device names.


A DOS device with no active volume is handled without a mount warning
when its live handler can be inhibited or the node has no handler.
For the destination, Dolly tries to remove the DOS node. If it remains,
only the same inhibited handler or an inactive node with no volume may
remain during copying and the post-copy rename review. A newly active
volume or handler blocks further writes. Dolly retries removal during cleanup for entries with no live handler.
If a live handler remains inhibited after a write, Dolly reports that a
reboot is needed to remount the destination. The RDB automount flag describes boot policy. The live DOS list
determines whether the volume is mounted for this operation.

The open count still covers the entire Exec driver. Unexplained
additional opens require confirmation. A reduction in the approved
count is accepted when the DOS mount checks still pass. New opens
above the reduced count block subsequent writes.

### Post-copy names and mounting in Alpha 3.6

The post-copy name review now shows all detected name conflicts in one
window. **Rename** stages the selected group and **Keep** retains its
names. Dolly applies approved groups after the whole review succeeds.
**Cancel** discards the staged name changes. The data copy has already
finished at this point.

**Mount destination volumes** is available in the write review. If a
post-copy name review appears, it carries the same choice. It is off by default on a fresh installation. **Mount destination
volumes by default** in Settings changes the initial choice for new
operations. The checkbox applies only after the data and approved name changes
finish. Dolly closes its transfer handles, restores source access,
then rereads the destination geometry, PART descriptor and OFS/FFS
root block. It checks their identity, the partition's automount flag,
and the live DOS namespace before installing a node with
MakeDosNode and AddDosNode.

The first implementation requires a compatible resident handler for
the exact DosType, a verified volume within the first 4 GiB and a
free DOS name and range. It skips unformatted volumes, conflicting
names, partitions with automount disabled and ranges still owned by
an old DOS handler. No handler is loaded from the copied disk. Mount
results are reported separately from copying.

The read-only **Clone and resize** growth preview also prepares the new
root block in memory. It preserves existing bitmap pointers, adds new
root-level references where needed and marks the prospective bitmap as
needing validation. For 512-byte filesystem blocks it also previews the
two boot sectors with the proposed root pointer and preserves the
original checksum validity. When the root moves, the preview follows its
hash chains and checks allocation, parent pointers and header checksums.
It rejects cycles and duplicate references, computes prospective parent
updates in memory and reports the count in Warning Review. No metadata
is written. The preview identifies nonzero bytes after the partition
boot header and reports them in Warning Review. Such bytes may contain
startup code with fixed block addresses. A checksum cannot establish
whether that code will still work after resizing.

For growth that needs more bitmap references, the preview extends the
last existing bitmap extension in memory when present and builds any
new extension blocks. It preserves the preceding chain, fills new page
references and terminates the last extension with a zero link.

The growth preview generates every new bitmap page and updates the
existing boundary page and the page that contains the new root, when
applicable. It reuses a single page buffer and leaves the old root
allocated. Warning Review shows how many pages were built.

The preview counts free bits in each affected bitmap page and checks
the resulting change against the planned allocation for new bitmap
metadata and the relocated root. Warning Review shows the projected
used and free space, including reserved blocks in the used total.

The preview checks the serialized bitmap references in the prospective
root, the last existing extension when changed, and every new extension.
It verifies the page order, unused slots and the next extension links.
Earlier existing extension blocks retain the links validated on read.

For a smaller target, the read-only preview reports the midpoint root
candidate and whether the source bitmap shows it as free or occupied.
It also reports the page and extension counts required to cover the
target. Relocating occupied blocks from outside the target is a separate
step.

The shrink preview builds boundary bitmap pages, reserves the proposed
root and truncates bitmap references in the root and final retained
extension. Occupied source blocks must fit unless the preview replaces
the original root or detaches bitmap metadata that lies outside the
target. The proposed root position must be free, unchanged, or occupied by
detached bitmap metadata. Detached bitmap pages and
extensions are marked free in memory. The old root remains allocated.

When shrinking moves the root, the preview validates its direct
directory entries and computes their new parent references and checksums.
For 512-byte filesystem blocks it also previews the new root pointer
in the two boot sectors, preserving the original checksum validity.
Warning Review flags possible custom startup data.

Warning Review reports projected used and free space after shrinking.
The preview verifies that the free block count increases by the number
of detached bitmap blocks and decreases by one if the root moves to a free block.
Reserved blocks remain included in the used count.

Warning Review counts detached bitmap blocks beyond the destination
separately from other occupied blocks. Only detached blocks inside the
destination increase its free-block count.


For 512-byte filesystem blocks, the original root may lie outside the
target. The preview verifies direct root entries and prepares the new
root and boot pointer in memory. It counts only the new root against
the target allocation when the original is outside.

If the proposed root reuses a detached bitmap page or extension, that
block stays allocated and is excluded from the reclaimed block count.

If required bitmap metadata lies beyond the destination, Warning Review
reports separate page and extension counts and the lowest affected
filesystem block relative to the start of the partition.

### Host prototype for FFS clone and resize

`dolly-image resize-ffs` creates a standalone raw partition image from
an extent of a raw disk or partition image. Offsets and lengths are
decimal 512-byte sectors. The optional final argument supplies the
filesystem's reserved block count and defaults to 2.

```sh
dolly-image resize-ffs disk.hdf smaller.raw 504 204120 194560
dolly-image resize-ffs disk.hdf larger.raw 504 204120 819200
```

The prototype supports DOS/1 and DOS/3 with 512-byte filesystem
blocks and an empty startup payload. It validates the source allocation
and root entries, copies the relevant extent, updates root references
and bitmap metadata, and reopens the result to check its size, root and
free-space count. The original root is freed in the clone when it moves.
A failed run removes the incomplete output. Existing files are protected.

The output is a partition image without an RDB. Layouts that require
relocating file blocks or retained bitmap metadata remain unsupported.
### Native Clone and resize prototype

Select two disjoint RDB partitions with matching DOS/1 or DOS/3, 512-byte
blocks and matching reserved block counts. The destination partition sets
the new size. Dolly confirms the overwrite in Warning Review and uses
the existing Copy mount checks, write and flush logic, and optional
sector read-back verification. It stages the altered metadata in memory
and presents a read-only virtual source to the transfer loop.

After the write, Dolly reads the destination filesystem metadata and
checks the root and allocation totals before offering rename or mount.
The partition table remains in place. Unsupported layouts still have
a read-only analysis where possible. DLRP recovery for resize needs a
persisted resize plan and is not offered by this prototype.
