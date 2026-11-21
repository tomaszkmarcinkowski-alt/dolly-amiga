# HFS and HFS Plus media for ShapeShifter

This is a feasibility assessment for Dolly. ShapeShifter 3.11's manual and source were inspected. The workflows below still require a Macintosh guest test.

## What ShapeShifter presents to Mac OS

| ShapeShifter mode | What to copy in Dolly |
| --- | --- |
| FileDisk | The raw file from byte zero through its 512-byte sector count. The original FileDisk driver uses the file's length and DOS seeks, with no ShapeShifter-specific header. |
| DeviceDisk | The selected trackdisk-compatible device, unit, first sector and sector count. The driver adds the configured start byte to guest I/O positions and exposes the extent as a Macintosh volume through its HD20-style driver. A DOS device selection normally derives the extent from DosEnvec. The manually configured extent can differ from the whole Amiga partition. |
| Direct SCSI | The disk as seen by the Mac, including the Apple Partition Map (APM), driver descriptors and driver partitions where present. The Apple hard disk emulation option adapts selected SCSI mode commands for Apple HD SC Setup. APM and driver sectors are on-disk data. |

The official source files are `MacEmulAFD.asm`, `MacEmulADD.asm` and `MacEmulSCSI.asm`. In the English guide, see *Creating a simulated hard disk*, *Creating a dedicated Macintosh partition* and *Accessing SCSI disks directly*. ShapeShifter documents 512-byte DeviceDisk blocks. The original FileDisk and DeviceDisk code uses signed 32-bit guest byte positions. DeviceDisk forms an absolute offset with 32-bit arithmetic and ordinary trackdisk commands. Start with sector-aligned guest lengths below 2 GiB and validate the absolute configured extent against the host driver. Dolly's own 64-bit I/O does not extend ShapeShifter's address range. [ShapeShifter 3.11](https://shapeshifter.cebix.net/) · [Manual archive](https://ftp.fau.de/aminet/misc/emu/ShapeShifter311.lha) · [Source archive](https://ftp.fau.de/aminet/misc/emu/ShapeShifter_src.lha)

## Image and partition transfers

- **FileDisk to DeviceDisk and back.** Copy the exact guest-visible extent, including its boot sectors, bitmap, data and backup header. For a FileDisk destination, emit flat RAW. An IFF DOLY backup can be decoded by Dolly into that same extent. Preserve the RDB outside the selected Amiga partition. If the physical partition is larger, a DeviceDisk explicitly bounded to the original image size can carry an exact clone without expanding the filesystem. Verify that both start and count match the intended extent. A shorter destination requires a validated resize before transfer.
- **Whole SCSI disk to image and back.** Preserve block zero, APM, driver partitions and all guest sectors. On a dedicated Amiga partition, store the entire Apple layout inside the extent described by the outer RDB. Offer a separate, explicit operation to extract an Apple volume from a whole-disk image. For that operation, validate the partition entries and their logical data spans before copying a subrange. [Apple TN1189, Secrets of the Partition Map](https://developer.apple.com/library/archive/technotes/tn/tn1189.html)
- **Detection and safety.** Read at the start of the selected guest range. Identify HFS with its MDB at byte 1024 and 'BD', or native HFS Plus with the volume header at byte 1024 and 'H+'. Validate geometry and bounds. For 'BD', check for an embedded HFS Plus signature and extent and validate the inner header. Report HFSX separately. Validate an APM structurally and keep the disk/volume distinction visible in Warning Review. Amiga RDB DosType and a file extension are insufficient evidence. Keep Disk Copy, NDIF and UDIF containers outside the first RAW implementation. [Inside Macintosh: Files](https://vintageapple.org/inside_r/pdf/Files_1992.pdf) · [Apple TN1150](https://developer.apple.com/library/archive/technotes/tn/tn1150.html)
- **Quiesce the Macintosh guest.** Exit ShapeShifter or cleanly detach the guest volume before copying it. AmigaDOS INHIBIT can control the host filesystem handler and does not stop ShapeShifter's own raw device I/O. Retain Dolly's ordinary overlap and mount checks. Verify an equal-size round trip by byte comparison and by mounting or booting it in the guest as appropriate. For stock Mac OS 8.1 on 68k, retain HFS Standard as the startup format and use HFS Plus for data. [ShapeShifter guide and source](https://shapeshifter.cebix.net/) · [Apple TIL TA38399, archived](https://savagetaylor.com/til/TA38399.html)

## Resize while copying

| Layout | Feasibility and a safe initial scope |
| --- | --- |
| HFS Standard | An offline, clean-volume shrink is feasible. Start with cases where all allocated extents fit after shrinking. Preserve allocation block size and start, update the bitmap and MDB counts, and place the alternate MDB correctly. If the truncated region contains data, relocate blocks and update both forks and their catalog/overflow extents. Growth depends on bitmap room before the allocation area and the 16-bit allocation block count. If either constraint fails, a rebuild into a fresh larger filesystem is a separate path. |
| HFS Plus | A clean, non-journaled, unwrapped volume is the simplest early case. Resize the allocation file and total/free block counts, reserve space at the new end and move the alternate volume header. Shrink requires any live extents in the truncated region to be moved, including metadata where applicable. Wrapped HFS Plus also needs consistent outer MDBs, bitmap and bad-block extents, plus updated embedded bounds. Preserve the wrapper during sector copies. Treat journaled HFS Plus and HFSX as separate resize cases. |
| Whole disk with APM | Resize the chosen inner volume first for shrink, then update its APM bounds. For growth, reserve contiguous space and approve the layout before growing the inner volume. Preserve driver and other partition extents. Growing the container alone adds no free space to the volume. First implement resize on one extracted flat volume and leave in-place APM changes for a later stage. |

For an initial implementation, resize a staged copy on the desktop and verify it before writing the final target. GParted documents offline shrink for HFS and HFS Plus with the start held fixed, with no growth support for either. Apple's HFS sources and format documentation are useful references for the metadata and order of operations. Dolly's MIT implementation can use published format specifications. Importing Apple or GNU source would require reviewing its separate license. A fresh-volume copy must preserve resource forks, Finder metadata, boot information and the blessed System Folder where applicable. Validate the result with a filesystem checker and classic Mac OS before using it as the Restore source. [GParted support matrix](https://gparted.org/features.php) · [Apple HFS sources](https://github.com/apple-oss-distributions/hfs) · [Apple TN1150](https://developer.apple.com/library/archive/technotes/tn/tn1150.html)

## Convert between a FileDisk and a virtual SCSI disk

1. **FileDisk → dedicated RDB partition.** Build a complete guest disk within the selected partition. Its guest block zero contains the Driver Descriptor Record, which declares the virtual capacity and locates the Macintosh driver. Add an Apple Partition Map with entries for the map, the compatible 68k SCSI driver and the Macintosh volume. Copy the FileDisk bytes into the volume's data area. Keep the outer RDB and adjacent Amiga partitions outside the write range. The target needs room for both the unchanged volume and the new metadata and driver. If it is too small, a separate validated filesystem resize is required.
2. **Dedicated RDB partition → file.** Ask which of two outputs to create. **Macintosh volume (FileDisk RAW)** extracts the data area of the selected Apple volume, including any HFS wrapper. If the map contains multiple eligible volumes, ask which one to extract. **Entire virtual SCSI disk (RAW)** copies the complete RDB partition extent, including the driver descriptor, APM, driver and any other Apple partitions. The first output is the flat volume format used for a normal ShapeShifter FileDisk.

An offline test wrapped a 4 MiB HFS Standard FileDisk with Disk Jockey Jr
using `djjr convert to-device input.hfv output.hda`. The result contained
a valid driver descriptor, an APM, the Apple HD SC 7.3.5 SCSI driver and
an unchanged `Apple_HFS` volume. The volume started at LBA 96 in this
fixture, adding 48 KiB to the input. `djjr convert to-volume` returned
a byte-for-byte match, and the input hash remained unchanged. This test
covers the disk layout and conversion, with a ShapeShifter guest test still pending.

Dolly can build the layout from Apple's published format and a Macintosh
driver supplied by the user or licensed for distribution. Macintosh HD SC
Setup can also provide a reference Apple disk layout for testing.
[Disk Jockey Jr](https://diskjockey.onegeekarmy.eu/djjr/) · [Apple driver descriptor](https://dev.os9.ca/techpubs/mac/Devices/Devices-125.html) · [Apple TN1189](https://developer.apple.com/library/archive/technotes/tn/tn1189.html)

Dolly performs raw block I/O through the existing device and unit, for
example `scsi.device` unit 0. The first physical sector of the selected
RDB partition is LBA 0 of the inner disk. Dolly checks every transfer
against the partition's start and length, then adds the start LBA to
the relative sector number. It also checks that the inner map, driver
and volume fit within that range and that the underlying device can
address it. The Macintosh SCSI driver is stored as data inside the
partition. The conversion requires no AmigaDOS mount or additional
Amiga device driver.

### Confirm overwriting the destination

Before importing, probe the selected destination in read-only mode. Use
recognized on-disk structures to identify its current filesystem. If the
destination already contains an Apple Partition Map, inspect the volumes
inside it within their validated bounds. Report the RDB DosType separately
as the configured type. If the probe cannot confirm a filesystem, show
**Filesystem not recognized** or **Filesystem could not be verified** as
appropriate. An inconclusive probe does not establish that the partition
is empty.

Always show an **Overwrite destination partition** entry in Warning Review
for this import. Under **DESTINATION**, show the device, unit, RDB partition
number and name, and the detected filesystem or probe result. Explain that
writing the Apple disk layout and volume will overwrite existing contents
and can make existing data unrecoverable. Recommend making a backup first.
Reuse the existing ten-second destructive confirmation. Enable **Proceed**
after the countdown and keep **Cancel** available throughout. Require
approval before any write. Recheck the destination identity and range before
writing. Test recognized, unrecognized and unreadable metadata, and verify
that cancellation leaves the destination byte-for-byte unchanged.

## Proposed project sequence

1. Add read-only identification and bounds reporting for plain HFS, HFS Plus, wrapped HFS Plus and APM disk images in Inventory and Restore preflight. Sector copy and staged backups can reuse Dolly's existing transfer engines. The present preflight knows RDB and selected Amiga filesystem signatures, so it cannot yet enforce Apple volume versus disk compatibility.
2. Implement and test FileDisk import with APM and a compatible Macintosh driver, and both export modes, through the existing device I/O. Verify that all sectors outside the selected RDB partition remain unchanged. Rejection cases include an incorrect transfer offset, an overrun and a small destination. Validate the resulting media separately in ShapeShifter with HFS Standard and a Mac OS 8.1 HFS Plus data volume.
3. Prototype a no-relocation shrink on a clean, staged volume. Add controlled growth and relocation only after size, metadata and guest compatibility tests. Keep resize confirmation separate from copying a same-size extent.
