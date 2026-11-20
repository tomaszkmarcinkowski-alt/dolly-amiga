# Create Image prototype

Select **Create Image**, choose a Physical device, Partition or Logical
device as Source, and select a new file with the Destination ASL
requester. Choose **RAW image** or **IFF DOLY**, then select the DOLY
compression mode if applicable and press **Start**. Sheep Counter displays
the number of copied sectors and the main window shows progress.

RAW and HDF both contain the source sectors in order. A whole-disk HDF
includes the partition table and any other sectors in the reported
capacity. A partition or logical volume image contains only its extent.
This writer supports files up to 2 GiB minus one byte through the classic
AmigaDOS API. See [large image files](LARGE_IMAGE_FILES.md) for the
filesystem reference table and the destination preflight. An oversized
single image shows the planned count of IFF DOLY parts and offers **Confirm split**
in Warning Review. Part boundaries follow source sectors, so compression
changes the stored size without changing the part count. A separate warning
shows the shortfall if the volume may run out of space.
IFF DOLY saves the same source sectors in chunks with checksums and
range metadata. Fast uses PackBits for repeated bytes. Compact also
searches for repeated sequences with LZSS. Use the `.dly` extension. Its single-file
size limit includes container overhead. See [IFF FORMAT](IFF_FORMAT.md).
ADF remains a planned option.

The source uses the same [read-only RDB and override policy](AMIGA_IO.md#automatic-settings-and-manual-override) as [Surface Scan](SURFACE_SCAN.md). Save to RAM: or a DOS filesystem. A resolved destination on a different driver or
unit is accepted without a requester. If the destination device cannot
be identified, Dolly asks whether to continue at the risk of overwriting
source data. A destination volume on the same unit is allowed
only when its physical extent can be identified and is disjoint from the
raw source. If the ranges overlap or cannot be compared, Dolly blocks
the transfer and shows the reason in an OK-only requester. A disjoint range
on the same unit generates a speed warning. **Hide
same-device speed warning** in Settings suppresses the message; range
checks still run. Output begins in an exclusive
`.part` drawer in the destination directory and is published under the
requested filename after all sectors are copied and the file closes.
Existing destinations are refused. Stop pauses the operation and offers
Abort or Resume. Abort discards the staged output. If a read, write or
container-finalization failure interrupts an active image, Dolly offers
an Operation Restore File and retains the committed prefix when the
checkpoint saves successfully. See [DLRP](DLRP_FORMAT.md).

The event loop transfers one sector-aligned batch per pass. The default
ceiling is 16 KiB, raised to one sector if necessary and bounded by
the resolved MaxTransfer. The source backend owns
its aligned DMA bounce buffer and the image writer reuses the ordinary
transfer buffer. An image setting does not override the driver transfer
limit or DMA mask. User-facing buffer tuning and asynchronous double
buffering are deferred until measurements show a benefit worth the
memory and cancellation complexity.

## Emulator verification

In FS-UAE with an A600 and cycle-exact MC68000, the prototype imaged a
12,096-sector RDB partition to RAM:. A byte comparison against the
matching source range confirmed all 6,193,152 bytes.

For two sequential reads of the same 1 MiB range at each batch size,
4 KiB took 4.68 s and 3.16 s, 8 KiB took 2.58 s and 2.58 s, and 16 KiB
took 2.32 s and 2.24 s. These timings cover source reads on the
emulated controller. They exclude file writes and GUI updates.


## Imaging a running Amiga system

Create Image opens the source through the read-only sector adapter.
Before copying data it maps the selected driver, unit and byte range to
active AmigaDOS filesystems and requests approval for mounted volumes. An active handler with no
DOS volume can be inhibited without a separate warning. It releases the suspension on completion, failure or
cancellation. If inhibition cannot be verified, explicitly approving a
live read can leave an image with inconsistent filesystem contents.

The output filesystem must stay available to DOS. Dolly blocks a
destination file whose drawer uses a handler that the imaging source
must inhibit. An unformatted partition is included as raw bytes.
