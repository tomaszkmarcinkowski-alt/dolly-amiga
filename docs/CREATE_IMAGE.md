# Create Image prototype

Select **Create Image**, choose a Physical device, Partition or Logical
device as Source, and select a new file with the Destination ASL
requester. Choose RAW, HDF, IFF DOLY, IFF DOLY Fast or IFF DOLY Compact,
then press **Start**. Sheep Counter displays
the number of copied sectors and the main window shows progress.

RAW and HDF both contain the source sectors in order. A whole-disk HDF
includes the partition table and any other sectors in the reported
capacity. A partition or logical volume image contains only its extent.
This writer supports images smaller than 2 GiB on AmigaOS 2.04.
IFF DOLY saves the same source sectors in chunks with checksums and
range metadata. Fast uses PackBits for repeated bytes. Compact also
searches for repeated sequences with LZSS. Use the `.dly` extension. Its single-file
size limit includes container overhead. See [IFF FORMAT](IFF_FORMAT.md).
ADF remains a planned option.

The source uses the same [read-only RDB and override policy](AMIGA_IO.md#automatic-settings-and-manual-override) as [Surface Scan](SURFACE_SCAN.md). Save to RAM: or a DOS filesystem whose handler can be identified
as a different unit on the source driver. Output begins in an exclusive
`.part` drawer in the destination directory and is published under the
requested filename after all sectors are copied and the file closes.
Existing destinations are refused. A stopped copy, read failure, or
short/failed write discards the staged output, with a status message if
cleanup fails.

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
