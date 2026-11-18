# Surface Scan

Surface Scan reads the sectors of the selected source and discards the
data. It reports sectors that could not be read. It does not mount,
repair, format or write the source, and successful reads do not verify
filesystem consistency or predict device health.

## Establish the scan range

1. Resolve the actual driver and unit. A physical selection covers the
   device capacity reported by the driver. A partition selection covers
   only its recorded byte range. Re-read the partition map at Start
   and require the same entry name and range before starting.
2. For a logical DOS device, capture its registered startup record
   under the DOS list lock. Use the trusted block-aligned cylinder range
   from that record after checking the unit's capacity and sector size.
   Reject missing or incompatible geometry rather than scanning the
   whole unit as a fallback.
3. Validate sector alignment, nonzero length and the full range against
   the reported capacity. Use the validated RDB parameters and any
   manual overrides to configure the raw read. See the
   [automatic settings and manual override](AMIGA_IO.md#automatic-settings-and-manual-override).

## Large-offset reads

Requests entirely below 4 GiB use CMD_READ. A request crossing the
boundary or starting above it uses NSD READ64 when it is advertised
by a valid device-query response. The qualified UAE-IDE fallback uses
SCSI READ(10), which takes a sector address instead of a 32-bit byte
offset. The fallback is bounded to the ATA LBA28 address range
(128 GiB with 512-byte sectors). A range that needs large offsets and
has neither available path is rejected before the scan starts.
Active `scsi.device` versions below 43 (including AmigaOS 3.1 and earlier)
are limited to requests within 4 GiB. A newer driver needs a valid
NSD READ64 advertisement for larger offsets. The SCSI READ(10)
fallback remains specific to UAE-IDE.

## Read and classify sectors

Advance from the first LBA to the end of the validated range one
request at a time. The resolved MaxTransfer and a 16 KiB batch ceiling
(or one sector if larger) govern the size of reads into the DMA-checked buffer. After an I/O
error or a short transfer on a multi-sector request, read its whole
range sector by sector. Retry a failed individual sector once. If it
still fails, mark it unreadable, write its absolute LBA and failure
class to the Shell, and continue. Count the sector once even if
several requests failed. A sector recovered on retry counts as read.
An unsupported command, invalid request, unavailable device, media
change or backend range failure stops the scan.

## Progress and result

The main window shows the percentage of sectors examined, including
sectors classified as unreadable. Sheep Counter shows examined and
total sector counts, the unreadable sector count and a map of the
selected extent. The map reflows 192 flat tiles as the window
resizes, with tile sizes and gaps adjusted to the screen pixel aspect
ratio. R marks a cell with a sector
recovered by an individual retry. X marks a cell with at least one
unreadable sector and takes precedence over R. The Shell summary
reports retry and unreadable sector counts, and the earlier lines
locate the unreadable LBAs.
The parameters window defaults to the first and last sector of the
selected object, with one retry per unreadable sector. Both limits are
inclusive. Enter absolute decimal LBA or C/H/S (cylinder and head start
at zero, sector starts at one). The + and - buttons adjust by one sector
or retry. The retry budget accepts 0–255. Pause and Stop are handled
between requests. Stop pauses the scan and offers Abort or Resume. Hiding Sheep Counter affects only
the view. A completed scan with unreadable sectors is reported as
such, and an incomplete scan keeps its actual percentage.

The first implementation issues bounded synchronous reads between
GUI event passes. A device request still in progress can delay the
next event pass. Sheep Counter updates changed tiles during the scan
and refreshes its counters when the percentage or error count changes,
after eight updates, or when the scan stops. The progress bar repaints
only its changed segment during incremental updates. Opening, repainting,
or resizing the window reconstructs the display from the scan state.

## Scan log

Sheep Counter enables **Show Log** when a scan completes, stops, or
fails during reading. The log records failed batches, individual retry
attempts, recovered and unreadable sectors, and read failures that
abort the scan. Addresses are absolute device LBAs. The report header includes the
device name, reported geometry and device capacity, separately from
the scanned extent. Events also show CHS derived from the reported
device geometry, with cylinders and heads starting at 0 and sectors
starting at 1. If that geometry is missing or does not cover an LBA,
the CHS is shown as unavailable. A failed batch shows the position of
its first sector and the number of sectors in the batch. The range is retried with
individual reads.

The log stays in memory until the next Surface Scan or application
exit. Low memory leaves a retained prefix and an explicit omitted
event count in the export. The log viewer pages through retained
events and copies a clicked row to the clipboard. **Save Log** uses
an ASL requester and exports every retained event and the summary as
plain text. Existing files are refused. The export is written to a
temporary `.part` file and renamed after a successful close.

## Verification

The MC68000 build passed. In FS-UAE on AmigaOS 2.05, the GUI scanned
the 32,760-sector UAE-IDE unit and the DOS logical device DH1: to
100% with no unreadable sectors. Sheep Counter displayed real
processed and total counts while the unit scan was running. A
separate native probe of the partition path read 12,096 sectors
starting at absolute LBA 9,072, with no errors or retries.

A host test injected a failed batch, a recoverable read error,
a persistent I/O error and a persistent short read. The scan retried
three sectors and reported the two permanently unreadable LBAs once
each. Error injection was not performed on physical media.

A native probe in FS-UAE on AmigaOS 2.05 opened a 5 GiB UAE-IDE
image as a whole device, checked the contents of four distinct
sectors across the 4 GiB boundary, and completed a scan of the MBR
partition covering those sectors with no errors or retries. The
probe did not scan the entire image.
