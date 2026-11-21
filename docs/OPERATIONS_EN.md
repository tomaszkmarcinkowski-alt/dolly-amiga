# Dolly operations, step by step

This guide covers the native Dolly Alpha 3.6 prerelease for the MC68000 and AmigaOS 2.04 or later. Control names match Dolly's English interface.

## Selecting a source and destination

1. Choose an **Operation**. Dolly enables the supported media types under **Source**. Once you choose a source, it enables the corresponding types under **Destination**. The boxes below the buttons show the selected endpoints.
2. **Physical device** selects an entire driver unit. **Partition** selects a range described by the partition table. **Logical device** selects an AmigaDOS device. Dolly reads its driver, unit and range from the DOS configuration. **File** selects a file through the ASL requester.
3. On **Start**, Dolly rechecks the geometry and range required by the operation. For raw I/O, it reads **Mask**, **MaxTransfer** and **BufMemType** from the RDB and applies any manual overrides from **Settings**. When the RDB information is incomplete, Dolly offers safe defaults and gives you the option to stop and review the settings.
4. Legacy `scsi.device` versions have a default addressing limit of 4 GiB, with a qualified UAE-IDE exception for reads. Dolly checks command support for the selected range. The legacy limit override alone cannot enable 64-bit commands. Writes beyond the boundary require NSD READ64 and WRITE64 support.
5. If the RDB declares sectors beyond the capacity reported by the driver, the selector shows the discrepancy. **Allow partitions outside range** lets you choose how supported read operations handle it. Zero-filling preserves the declared length. Partition writes require a destination range within the reported capacity.

## Preparing and completing a raw write

**Sector-to-sector**, **Smart Copy**, **Clone and resize** and **Restore Image** share this sequence.

1. Dolly checks the relationship between the source and destination ranges. For image restores, it checks the volumes that contain every image file. A confirmed overlap blocks the operation. It shows a separate warning when it cannot identify the image volume. You can suppress the performance warning for transfers within one unit in **Settings**.
2. Dolly automatically isolates mounted source volumes with `INHIBIT`. It inspects the destination range for existing filesystems.
3. **Warning Review** shows overwrite, size and other compatibility warnings. When Dolly recognizes an existing destination volume, the warning names it, recommends a backup and enables **Proceed** after ten seconds. The final write confirmation also identifies the destination whose data will be overwritten.
4. After the final approval, Dolly automatically unmounts the destination unit. A changed or busy DOS entry blocks the write. Dolly rechecks the device and partition geometry and compares the first and last sector fingerprints. It records the driver-wide open count and stops further writes if access increases or the mount state changes.
5. Dolly writes bounded batches and flushes the driver. **Verify writes** adds a readback and byte comparison for each batch. After a successful transfer, Dolly may offer name changes. It releases source inhibits and retries removal of inactive destination entries. The final transfer summary offers to mount eligible destination partitions after verification. The default is configurable in Settings.

See [Device writes](DEVICE_WRITE.md) and [AmigaOS I/O](AMIGA_IO.md) for driver requirements.

## Sheep Inventory

1. Choose **Inventory** with **Physical device** or **Partition**. Dolly inspects the selected unit's partition table and displays its driver, unit, name, geometry, capacity and ATA IDENTIFY data when available.
2. Select a row on the left to view partition details. Clicking a row in the header or details list copies its text to the clipboard. The left list selects the partition.
3. **Backup RDB** opens ASL and copies the validated range from LBA 0 through the reserved RDB area. The button is disabled when Dolly cannot establish a safe range. **Backup MBR** exports the full LBA 0 sector for supported layouts.
4. **Restore RDB** opens a ten-second confirmation. Dolly checks the backup against the destination RDB and qualifies the driver. After final approval, it takes the destination offline automatically, saves a mandatory recovery copy, rechecks the current metadata, writes the changed sectors and verifies the full reserved area. This operation recovers compatible RDB metadata. **Restore MBR** is planned.

See [Sheep Inventory](SHEEP_INVENTORY.md).

## Surface Scan

1. Select a device, partition or logical device. The default scan range covers the selected object. Set the first and last LBA or CHS and the retry count to narrow the scan or change how it handles errors.
2. Dolly validates the range and prepares the source. It reads sequential batches. If a batch fails, Dolly reads its sectors individually and applies the selected retry limit.
3. The main window shows progress. **Details** opens Sheep Counter. On its grid, **R** marks a sector recovered after a retry. **X** marks a tile with at least one unreadable sector.
4. After the scan, **Show Log** lists the device, geometry and events with absolute LBA and CHS where CHS can be derived. **Save Log** exports the text through ASL.

See [Surface Scan](SURFACE_SCAN.md).

## Create Image

1. Select a device, partition or logical device. **Destination** opens the ASL save requester. **RAW image** stores sectors in order and can use an extension such as `.raw` or `.hdf`. **IFF DOLY** offers **Not compressed**, **Fast** (PackBits) and **Compact** (LZSS with per-chunk encoding selection).
2. Dolly checks whether the output location conflicts with the source, checks available space, and applies the single-file limit of 2 GiB minus 1 byte. If the image would exceed that limit, it offers to split the image into IFF DOLY parts and reports the planned count. The space warning reports the estimated shortfall. For compressed images, it uses an upper estimate of the required space.
3. Dolly isolates the source, reads batches and writes to a staging location. IFF DOLY stores range metadata and checksums. A split set contains a first file such as `disk.dly`, numbered parts such as `disk.001` and `disk.002`, and set metadata in the parts.
4. If a part fails to write, you can choose another drawer for it. Dolly checks the new destination and retries that part from the beginning. It publishes the finished image after closing the files and refuses to overwrite an existing final file.

See [Create Image](CREATE_IMAGE.md), [large image files](LARGE_IMAGE_FILES.md) and the [IFF DOLY format](IFF_FORMAT.md).

## Restore Image

1. **Source** opens ASL. Dolly recognizes flat RAW/HDF and IFF DOLY images. Choose a whole device or an RDB partition as the destination.
2. For a split set, select the first file. Dolly checks the part list, numbering and set identity. If a part is missing or incorrect, use **Locate files...** to select a replacement from another drawer.
3. Dolly assesses the size, image kind and readable metadata. **Warning Review** explains discrepancies. If you approve a size difference, Dolly writes the common range. A smaller image leaves the end of the destination untouched. A larger image loses its tail during restore, which can leave an incomplete filesystem.
4. Dolly validates the full IFF DOLY image and its checksums before the first destination write. An invalid part stops the operation. Dolly then writes within the approved range and performs optional readback verification.

## Copy

### Sector-to-sector

1. Choose two whole devices or two RDB partitions within the reported capacity. Dolly requires equal lengths, equal sector sizes and separate ranges.
2. Once you approve the warnings, Dolly reads and copies every sector. A whole-disk copy includes the partition table and sectors that the filesystem marks free.

### Smart Copy

1. Choose **Partition** or **Logical device** independently for each endpoint. Mixed pairs work in both directions. Dolly resolves each logical device to an RDB partition and checks for overlap, equal lengths, compatible block sizes, matching reserved block counts and matching DosType.
2. Dolly isolates the source, checks OFS/FFS and reads the allocation bitmap. The status and progress bar show bitmap validation. An invalid bitmap or incompatible structure blocks the operation.
3. The plan in **Warning Review** shows allocated blocks including metadata, the size of the data to write, skipped blocks and the number of ranges. After approval, Dolly rechecks the RDB and compares the plan with the current bitmap.
4. Dolly writes allocated and reserved blocks at the same relative positions on the destination. Free blocks retain their previous contents. Optional verification covers the written ranges.

### Clone and resize

1. Choose **Partition** or **Logical device** independently for each endpoint. Each logical device must resolve to a single in-range RDB partition. The two partitions must be separate and have matching DOS/1 or DOS/3, 512-byte blocks and the same reserved block count. The destination partition size determines the resized filesystem size.
2. Dolly isolates the source and checks the bitmap and root entries. The write prototype requires an empty startup payload and a layout that keeps files and required bitmap blocks in their supported positions. Other supported layouts can be analyzed in read-only mode.
3. **Warning Review** shows the proposed size, new root and destination overwrite. After approval, Dolly rechecks both RDBs and stages the resized metadata in memory.
4. Dolly copies the resized filesystem to the destination partition, preserves the RDB and reopens the written metadata for verification. **Verify writes** also compares each written batch. A successful verification allows the name and mount review.
5. Resize recovery via DLRP and native resize testing in WinUAE are pending.

### Name changes after copying or restoring

1. After a whole-disk copy, Dolly can offer to change conflicting RDB device names, for example `DH0 → DH0_0`.
2. For a recognized complete OFS/FFS volume, Dolly can separately offer to change its label, for example `Work → Copy_of_Work`. An unformatted partition can have a device name in the RDB. A volume label requires a valid root block.
3. Each review shows the old and proposed names. Choose **Rename** to approve that group, or **Cancel** to retain the names. Dolly verifies approved changes while the destination is isolated.

## Disk Vet

The selectors offer **Analyze (read only)** and **Analyze & repair**. The second choice enables **Repair in place** or **File recovery**, with an ASL drawer selector for file recovery. This build provides the configuration controls. Filesystem analysis, repair and recovery are planned.

## Progress, interruption and DLRP files

- **Start** becomes **Pause** during an operation. **Stop** pauses it and lets you abort or resume. **Quit** during an active operation reports the block in the status bar.
- After an unexpected read or write error, Dolly can offer to save an **Operation Restore File** in IFF DLRP format for supported Create Image, Restore Image and sector-copy cases. The file records the operation state. Surface Scan and manual Stop or Quit do not generate these checkpoints.
- DLRP loading and resumption from the GUI are planned. Smart Copy needs a separate record of its ranges. Split images have separate per-part recovery.

See the [DLRP format](DLRP_FORMAT.md) and the [test matrix](TEST_MATRIX.md).
