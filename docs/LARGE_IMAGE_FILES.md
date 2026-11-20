# Large image files

Disk addressing, filesystem file size, AmigaDOS file APIs and the image
format impose separate limits. The ability to address sectors beyond 4 GiB
does not determine the largest file a mounted volume can store.

| Mounted filesystem or environment | Single-file limit or large-file support | Reference |
| --- | --- | --- |
| Classic OFS/FFS, AmigaOS 2.04–3.1, ROM handlers V37–V40 | The documented classic file API ceiling is 2 GiB − 1 byte. Smaller volumes impose smaller limits. | [Seek](https://developer.amigaos3.net/autodocs/dos.library/Seek.html) |
| Classic FFS on AmigaOS 3.5/3.9, V44/V45 generation | Large disk addressing is independent of the 2 GiB − 1 byte classic API ceiling. No qualified large-file path established for Dolly. | [AmigaOS 3 DOS autodocs](https://developer.amigaos3.net/autodocs/dos.library/) |
| Classic FFS V46 (3.1.4) and V47 (3.2.x), including DOS7 | Classic file size reporting and positioning still have a 2 GiB − 1 byte ceiling. DOS7 and support for large disks do not qualify large files. | [Examine](https://developer.amigaos3.net/autodocs/dos.library/Examine.html), [Hyperion 3.2](https://www.hyperion-entertainment.com/index.php/where-to-buy/direct-downloads/327-amigaos-32) |
| SFS/00 | AmigaOS 4 documents 4 GiB − 2 bytes. The exact maximum of each classic 68k handler revision still needs qualification. | [AmigaOS filesystem table](https://wiki.amigaos.net/wiki/UserDoc:AmigaOS_File_Systems) |
| SFS/02, classic 68k SFS 1.275 | Supports files above 4 GiB. The classic release coverage reports **1 TB for a partition**. The AmigaOS 4 table documents **1 TB for a single file** under SFS2. The same single-file ceiling is plausible for the 68k build, pending direct confirmation. | [SFS 1.275 release notes](https://amiga-news.de/en/news/AN-2007-08-00084-EN.html), [Classic SFS2 announcement](https://amiga-news.de/en/news/AN-2006-03-00018-EN.html), [AmigaOS filesystem table](https://wiki.amigaos.net/wiki/UserDoc:AmigaOS_File_Systems) |
| PFS3 AIO, upstream normal build | Source defines `MAX_FILE_SIZE` as **4 GiB − 1 byte**. The safe classic-API ceiling remains **2 GiB − 1 byte**. | [PFS3 AIO source](https://github.com/tonioni/pfs3aio/blob/master/struct.h#L858-L872) |
| PFS3 AIO, upstream large-file build | Source defines `MAX_FILE_SIZE` as **512 GiB − 1 byte**. Files of 4 GiB and more require the large-file handler and a PFS2-format volume with large files enabled. The compiled ceiling is subject to actual volume capacity and allocation limits. | [PFS3 AIO source](https://github.com/tonioni/pfs3aio/blob/master/struct.h#L858-L872), [setlargefile](https://github.com/tonioni/pfs3aio/blob/master/docs/tools.md#setlargefile) |
| AmigaOS 4.1 FFS/FFS2, DOS3/DOS7 | 4 GiB − 2 bytes with the corresponding handler and 64-bit file APIs. This row provides a cross-platform reference. | [AmigaOS filesystem table](https://wiki.amigaos.net/wiki/UserDoc:AmigaOS_File_Systems), [AmigaOS 4 DOS APIs](https://wiki.amigaos.net/wiki/Programming_AmigaOS_4:_DOS_-_The_Data_Administrator) |

The classic FFS rows give the documented limit of the portable file API.
They do not assert the on-disk maximum of every FFS binary. DiskType
identifies the volume format. It can be shared by different handlers and
versions and cannot serve as a capability flag. The PFS3 AIO source values
apply to the reviewed upstream build variants. Older distributed binaries
need separate verification.

## Dolly

At Start, Create Image opens the destination drawer lock and calls
`Info()` against that lock, so assigns resolve to the actual volume.
A destination reported as write protected is rejected before staging.
A single image that exceeds the signed 32-bit file limit or the IFF DOLY
chunk limit adds an item to Warning Review. It reports the required size,
dos.library version and DiskType when available. **Confirm split** changes
the current operation to the displayed number of IFF DOLY parts. A RAW
or HDF selection switches to uncompressed IFF DOLY after confirmation. **Cancel** aborts before
creating output. The largest usable file on an unqualified handler remains
unknown. Restore rejects ambiguous legacy sizes and checks EOF at the
reported absolute offset.

Create Image compares the planned output with free space derived from
the mounted volume's reported block counts. The warning lists the volume
capacity, free bytes and estimated shortfall. For compressed output it
uses the original size as the upper bound on payload and includes the IFF
headers in the shortfall calculation. Compression can reduce the actual
output size. An unavailable or invalid space report also produces an item
in Warning Review. **Continue** allows space to be freed during the operation.
Filesystem allocation and directory overhead can cause the write to stop
even when the space estimate was sufficient.

For large single files, a future backend should use documented
64-bit size and position operations on the opened file handle. The
[packet registry](https://wiki.amigaos.net/wiki/AmigaDOS_Packets)
lists the relevant packet numbers. A successful size query proves
only that operation. Validate positioning and readback above 2 and 4 GiB,
and qualify the write path and volume limit before offering a large
single-file image. A file appearing in Workbench or an ASL requester
does not establish that `ExamineFH()` and `Seek()` can represent it.
Do not determine the maximum by filling the destination with a probe file.

Split output is a directory named as selected in ASL, staged under a
temporary name until every part closes successfully. The manifest is
embedded in every part as an IFF `MSET` chunk with its own CRC-32.
The manifest records the format, starting LBA, sector size, total sectors
and part geometry. Parts are numbered in hexadecimal, starting at 1.
Each part is a
standalone FORM with its own chunk checksums and the starting LBA of that
part. The raw payload of a part is at most 256 MiB, rounded down to a
whole transfer batch. A part may also contain IFF headers. For Restore Image, select the first file, for example `disk.dly`.
After file selection, the main-window status shows the selected part
number, total part count and decoded byte size from the manifest. It
checks the manifest CRC during this preview. Other parts and image data
are validated at Start. If a later numbered part is selected, Dolly
offers to open the first file and checks that it belongs to the same set.

Restore checks that all expected files in the first part's directory can
be opened and that their headers and geometry match. It lists missing,
mismatched and malformed parts. For a numbered set,
**Locate files...** lets the user select replacements individually in ASL,
including from different volumes. Each replacement must match its expected
part number, set identifier and geometry. Restore applies the
destination overlap and unmount checks to all source volumes. It then
validates all chunk headers and checksums in bounded steps before writing
any destination sectors. Both progress views track verification. Pause
and Stop are available between steps. A validation error reports the
affected part and stops the operation before writing.

The scheduler yields to the GUI between verification batches. For
ordinary Dolly output, chunk sizes follow the source transfer buffer.
Imported IFF DOLY v1 files can have larger raw chunks. A single such
chunk must pass its full CRC before Dolly trusts any bytes from it,
so reaction to Pause or Stop can take longer for those files.

Older sets remain readable via `part00000001.dly` or `manifest.txt`,
depending on the format. Split output does not offer a single-file
DLRP checkpoint. See the [IFF specification](IFF_FORMAT.md#multipart-manifest).

### Changing the output drawer after a write failure

For a split image, a write or close failure offers **Choose drawer...**.
Dolly checks the new drawer for source overlap, write protection and
free space for the entire part. Once the user accepts any warnings
and the new staging drawer exists, Dolly deletes the incomplete part
and retries it from its first sector. Completed parts stay in their
original staging drawers. The remaining parts use the new drawer.
Each drawer is published after the complete set has been written.
If publication across multiple drawers fails, Dolly attempts to roll
back the completed renames and retains the staged files.

When restoring a set spread across drawers, open the first part and
use **Locate files...** to find the other parts. ASL displays the expected
filename in its title and pre-fills the filename field.

### Desktop command-line tool

To build just the desktop executable and run its round-trip test,
use CMake and Python 3.

```text
cmake -S . -B build-image -DDOLLY_IMAGE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-image --config Release
ctest --test-dir build-image -C Release --output-on-failure
```

The [desktop CI workflow](../.github/workflows/image-tool.yml) uses this
configuration on Linux, macOS and Windows and uploads an executable
for each runner after its tests pass. The full host build also includes
the executable. It offers three commands.

```text
dolly-image split disk.hdf output-directory --fast
dolly-image verify output-directory/disk.dly
dolly-image join output-directory/disk.dly restored.raw --sparse
```

`split` accepts sector-aligned RAW or HDF and defaults to uncompressed
IFF DOLY. Pass `--fast` for the same PackBits compression used by Dolly.
An optional number sets the 512-byte sectors per part. The default is 524,288 sectors (256 MiB of payload). A small
image can be written as a standalone DOLY file. `verify` and `join`
handle standalone DOLY and both embedded-manifest layouts. They validate
part order, geometry and data CRCs. The tool expects all parts in the same
directory. With `join --sparse`, zero-filled ranges are skipped when
writing the RAW file. The file retains its full logical size, including
trailing zeros. Physical space savings depend on the output filesystem. It requires a new output directory or file and removes partial
output on a detected error. The tool does not write to block devices.
