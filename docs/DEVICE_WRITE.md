# Device writes

**Copy → Sector-to-sector** copies between two whole devices or two
in-range RDB partitions with equal byte lengths and sector sizes.
**Restore Image** writes a flat RAW/HDF image to a compatible whole
device or RDB partition. The first write path supports 512-byte sectors
and destination byte offsets below 4 GiB. Classic AmigaDOS file
positions limit the RAW/HDF source to less than 2 GiB.

Dolly checks device geometry, the RDB extent, DMA settings, and logical
overlap. Whole devices may have a validated RDB or no partition table.
MBR/GPT, hybrid tables, IFF DOLY decoding, filesystem-aware copying and
resizing remain outside this path. Mounted targets are refused. Dolly
also refuses a driver open count that it cannot account for. This can
block a target whose driver also serves another mounted unit.

## Confirmation and transfer

Restore compares the image and destination size, object kind,
filesystem IDs and RDB attributes. A metadata mismatch requires
**Allow Restore compatibility override** and timed approval. A size
mismatch, invalid range, unsupported partition table, or unreadable
extent blocks the operation. The image file's actual open handle is
used for the volume overlap check.

Both operations require a ten-second destructive confirmation. After
approval, Dolly checks geometry and partition selection again, compares
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
