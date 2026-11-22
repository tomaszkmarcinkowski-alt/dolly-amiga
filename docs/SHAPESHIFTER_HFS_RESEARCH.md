# HFS and HFS Plus media for ShapeShifter

This is a feasibility assessment for Dolly. ShapeShifter 3.11's manual and source were inspected. The workflows below still require a Macintosh guest test.

## What ShapeShifter presents to Mac OS

| ShapeShifter mode | What to copy in Dolly |
| --- | --- |
| FileDisk | The raw file from byte zero through its 512-byte sector count. The original FileDisk driver uses the file's length and DOS seeks, with no ShapeShifter-specific header. |
| DeviceDisk | The selected trackdisk-compatible device, unit, first sector and sector count. The driver adds the configured start byte to guest I/O positions and exposes the extent as a Macintosh volume through its HD20-style driver. A DOS device selection normally derives the extent from DosEnvec. The manually configured extent can differ from the whole Amiga partition. |
| Direct SCSI | The disk as seen by the Mac, including the Apple Partition Map (APM), driver descriptors and driver partitions where present. The Apple hard disk emulation option adapts selected SCSI mode commands for Apple HD SC Setup. APM and driver sectors are on-disk data. |

The official source files are `MacEmulAFD.asm`, `MacEmulADD.asm` and `MacEmulSCSI.asm`. In the English guide, see *Creating a simulated hard disk*, *Creating a dedicated Macintosh partition* and *Accessing SCSI disks directly*. ShapeShifter documents 512-byte DeviceDisk blocks. [ShapeShifter 3.11](https://shapeshifter.cebix.net/) · [Manual archive](https://ftp.fau.de/aminet/misc/emu/ShapeShifter311.lha) · [Source archive](https://ftp.fau.de/aminet/misc/emu/ShapeShifter_src.lha)

### Disks larger than 4 GiB

The following limits are from the stock ShapeShifter 3.11 source.
They have not been tested with a large disk in a Macintosh guest.

| Mode | Addressing and practical limit |
| --- | --- |
| FileDisk | `OpenFileDisks` reads the classic AmigaDOS `fib_Size` and divides it by 512. `FTDoIO` rejects negative 32-bit guest positions and uses classic DOS `Seek`. Use sector-aligned files below 2 GiB, at most 4,194,303 sectors or 2,147,483,136 bytes. |
| DeviceDisk | `DTDoIO` rejects negative relative byte positions, adds `dsStartByte` with 32-bit arithmetic and passes the absolute offset to ordinary `CMD_READ` and `CMD_WRITE`. Keep the guest volume below 2 GiB and the entire host extent within the first 4 GiB. For 512-byte sectors the end-exclusive condition is `start_lba + sector_count <= 8,388,608`. A large physical disk can meet both conditions with an early partition. A small partition starting at or crossing 4 GiB can wrap to a lower LBA. The driver may impose a lower limit. |
| Direct SCSI | `SCSIDoCmd` copies up to the 12-byte command buffer and `SCSISendIt` submits `HD_SCSICMD`. Standard `READ(10)` and `WRITE(10)` carry block addresses, so this route has no 4 GiB byte-offset boundary in ShapeShifter. Access above it depends on the host driver and controller, the guest SCSI driver, and Mac OS. The stock command buffer does not support the 16-byte commands used for larger LBAs. |

[ShapeShifter 3.11 source](https://ftp.fau.de/aminet/misc/emu/ShapeShifter_src.lha) · [AmigaOS TrackDisk64](https://wiki.amigaos.net/wiki/TrackDisk64_Standard) · [AmigaOS SCSI Device](https://wiki.amigaos.net/wiki/SCSI_Device) · [T10 block command addressing](https://www.t10.org/ftp/t10/document.06/06-034r5.pdf)

### Limits enforced in Dolly

- Restore Image and sector-to-sector Copy of recognized Macintosh layouts to an
  RDB partition require the entire destination partition to end at or before
  4 GiB. With 512-byte sectors the last permitted LBA is 8,388,607. The
  complete incoming layout must fit before Dolly writes any sectors.
- A flat Macintosh volume and each allocated Apple Partition Map entry must
  be smaller than 2 GiB. Dolly checks each entry's full declared length,
  including any space outside its logical data area. `Apple_Free` entries
  describe unused space and are exempt.
- ShapeShifter file transfers use a single RAW file. The maximum for 512-byte
  sectors is 4,194,303 sectors (2,147,483,136 bytes). Create Image offers the
  entire virtual disk when it fits, or an eligible Macintosh volume. A
  containing RDB partition can exceed the single-file limit.

The guest's mounted volume limit is independent of the transport. Apple
documents a 2 GB HFS volume limit before System 7.5, 4 GB in System 7.5,
and up to 2 TB on some configurations from 7.5.2. On stock 68k Mac OS
8.1, use HFS Plus for data and HFS Standard for the startup volume.
[Apple HFS volume limits](https://savagetaylor.com/til/TA27115.html) · [Apple Mac OS 8.1 startup disk](https://savagetaylor.com/til/TA38399.html)

## Image and partition transfers

- **FileDisk to DeviceDisk and back.** Copy the exact guest-visible extent, including its boot sectors, bitmap, data and backup header. For a FileDisk destination, emit a single flat RAW file. Preserve the RDB outside the selected Amiga partition. If the physical partition is larger, a DeviceDisk explicitly bounded to the original image size can carry an exact clone without expanding the filesystem. Verify that both start and count match the intended extent. A shorter destination requires a validated resize before transfer.
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

### Export from Dolly on AmigaOS

Choose **Create Image**, select the dedicated RDB partition as the source,
and choose **RAW image**. At Start, a validated Apple Partition Map opens
**Export Macintosh disk**. Select the whole disk or an individual recognized
HFS-family volume. Each volume row identifies its APM entry, label,
filesystem type and size. The standard staged file workflow handles the
selected range. Cancel closes the selection before any output is created.

Dolly keeps the complete containing RDB partition under its ordinary
source and overlap checks. It checks the Apple layout again after the
destination preflight, before creating the staging drawer. The selected
RAW output must fit the 2 GiB minus one sector limit. A FileDisk
subrange cannot generate a DLRP checkpoint because the current format
describes the selected RDB partition as a whole.

### Desktop export prototype

The portable APM reader lists recognized HFS-family entries with their
one-based map entry number, label and logical data extent. Offsets are
relative to the inspected span, which can begin at a nonzero LBA within
a raw device. An invalid or unreadable map publishes no volume entries.

Given a RAW file of the complete dedicated partition, `dolly-image` can
exercise both export modes:

```sh
dolly-image export-apple partition.raw volume.hfv
dolly-image export-apple partition.raw volume.hfv 3
dolly-image export-apple partition.raw disk.hda --whole
```

The default extracts the single recognized volume. For multiple volumes,
the command lists the candidates and requires an APM entry number. The
volume output copies exactly the entry's logical data range. `--whole`
copies the full input extent, including padding beyond the Apple disk's
declared capacity. Both modes require a valid APM layout and a new output
path.

### Import a FileDisk on AmigaOS

Select **Restore Image** with a flat HFS-family image as the source and a
dedicated 512-byte-sector RDB partition as the destination. Choose
**Build disk** and pick a compatible Apple driver template in ASL.
The **Restore raw** choice is also available for a plain sector copy.

Dolly checks both open source files against the destination and refuses
to truncate the assembled disk. If it fits, the **Overwrite destination**
entry in Warning Review shows the device, unit, partition, current
filesystem evidence, RDB DosType and write range. **Proceed** becomes
available after ten seconds. After approval, Dolly takes affected AmigaDOS
filesystems offline, checks the partition and media again, then streams
the Apple disk into the selected extent. It preserves the outer RDB and
sectors after the assembled image. Dolly does not mount the imported
volume in AmigaOS. The two-file import does not yet produce DLRP files.

### Prepare an import image on the desktop

```sh
dolly-image import-apple volume.hfv driver-template.hda prepared.hda
```

Supply a trusted 512-byte-sector APM template containing a map entry,
one Macintosh SCSI driver entry (`Apple_Driver` or `Apple_Driver43`) and
one `Apple_HFS` entry with data at the start of that partition. The
portable builder retains the driver area and its descriptor, updates
the disk and volume lengths, and streams the FileDisk after the driver.
Gaps outside the driver and map entries are zeroed. The input volume
keeps its exact length and bytes. The builder checks geometry and bounds
and leaves driver and guest compatibility to verification in ShapeShifter.

The command creates a new RAW disk image. The native import uses the same
portable builder directly through the existing Restore write path.

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

### Configure the ShapeShifter guest test

ShapeShifter 3.11 requires a 68020 or later and a compatible original
Macintosh ROM, version 124 and 512 KiB or 1 MiB.
Use a bootable HFS Standard system disk and a separate HFS Plus data disk
for the Mac OS 8.1 test. The Amiga Kickstart ROM serves the host only.

ShapeShifter's **DeviceDisk** accepts a device, unit, start sector and
sector count and presents an HD20 volume. For an Apple volume inside an
RDB partition, set its start to the RDB partition's first LBA plus the
Apple entry's physical start and logical data start. Set the sector count
to the Apple entry's logical data count. This presents the inner volume
as a flat extent, as FileDisk does. With a 512-byte test layout whose
RDB partition starts at LBA 32 and Apple volume starts at relative LBA 96,
DeviceDisk starts at LBA 128.

For a separate test of the complete Apple disk and its driver, use
**Direct SCSI** with a target that already exposes that exact range as a
whole disk. ShapeShifter's Direct SCSI configuration selects a device
and unit and forwards SCSI requests. Pointing it at the parent physical
unit exposes the parent disk from LBA 0. The DeviceDisk range fields do
not apply to Direct SCSI. [ShapeShifter manual](https://ftp.fau.de/aminet/misc/emu/ShapeShifter311.lha) · [ShapeShifter source](https://ftp.fau.de/aminet/misc/emu/ShapeShifter_src.lha)

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

1. Read-only identification now reports HFS Standard, HFS Plus, wrapped HFS Plus, HFSX and APM in the flat-file selection status and in Restore compatibility review. The probe checks 512-byte sectors, up to 128 map entries, partition overlap and the bounds of Driver Descriptor Record and Apple partition data ranges. An unrecognized result does not establish that a partition is empty. Restore keeps its existing raw-transfer workflow and prompts before writing. Inventory integration and reporting per-volume APM details remain follow-ups.
2. Implement and test FileDisk import with APM and a compatible Macintosh driver, and both export modes, through the existing device I/O. Verify that all sectors outside the selected RDB partition remain unchanged. Rejection cases include an incorrect transfer offset, an overrun and a small destination. Validate the resulting media separately in ShapeShifter with HFS Standard and a Mac OS 8.1 HFS Plus data volume.
3. Prototype a no-relocation shrink on a clean, staged volume. Add controlled growth and relocation only after size, metadata and guest compatibility tests. Keep resize confirmation separate from copying a same-size extent.
