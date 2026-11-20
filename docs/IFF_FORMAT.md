# DLY / IFF FORM DOLY

Use the `.dly` file extension and the IFF form identifier `DOLY`.
Create Image writes single-file version 1 (uncompressed) or version 2
with per-chunk compression. Its contents are the selected source sectors in ascending order.

## Version 1 layout

All numeric fields are unsigned big-endian. An IFF chunk begins with
a four-byte ID and a four-byte data length that excludes this header
and any padding. IFF chunks are word-aligned. Version 1 uses even
payload lengths throughout, so the writer emits no pad bytes.

The file starts with `FORM`, a 32-bit length equal to the file size
minus 8, and `DOLY`. The following chunks appear in order.

| Chunk | Payload offset | Size (bytes) | Field |
| --- | ---: | ---: | --- |
| `HEAD` | 0 | 4 | Version, currently 1 |
| `HEAD` | 4 | 4 | Reserved flags, currently 0 |
| `GEOM` | 0 | 8 | First absolute device LBA |
| `GEOM` | 8 | 8 | Number of sectors in the selected source range |
| `GEOM` | 16 | 4 | Bytes per sector (power of two, 512–32768) |
| `GEOM` | 20 | 4 | Maximum raw bytes per data chunk (a multiple of the sector size) |
| `CHNK` | 0 | 8 | First sector relative to the selected source range |
| `CHNK` | 8 | 4 | Raw byte count |
| `CHNK` | 12 | 4 | CRC-32/ISO-HDLC of the raw bytes |
| `CHNK` | 16 | Raw byte count | Raw sector data |

There is exactly one `HEAD` and one `GEOM`, followed by one or more
`CHNK` chunks. Data chunks start at relative sector 0 and cover the
range contiguously. Each contains the maximum raw byte count except
the final chunk, which may be shorter. Add the first absolute LBA
from `GEOM` to a chunk's relative sector to locate it on the device.

The writer plans the exact file size before it opens the destination.
The final file is limited to 2,147,483,647 bytes, including all IFF
headers, and to one million chunks including `HEAD` and `GEOM`.
The destination is published only after all chunks are written and the
staged file closes successfully.

## Version 2 compression

`HEAD` has version 2 and flags 0. `GEOM` keeps its version 1 layout.
A version 2 `CHNK` retains the first 16 payload bytes and inserts a
32-bit codec ID at offset 16. Stored bytes begin at offset 20, and
their count is the IFF chunk length minus 20. The CRC still covers the
uncompressed sector data. Odd-length chunks have a zero pad outside
the IFF chunk length.

| ID | Codec | Stored representation |
| ---: | --- | --- |
| 0 | Raw | Exactly the declared raw byte count |
| 1 | PackBits | Control 0–127 copies the following 1–128 bytes. Control 129–255 repeats the next byte `257 - control` times. 128 is a no-op. |
| 2 | LZSS | Groups of up to eight tokens, preceded by a control byte. Bits are read least-significant first. A zero bit is one literal byte. A set bit is a big-endian 16-bit reference with distance minus 1 in the upper 12 bits and length minus 3 in the lower 4 bits. Copying may overlap. Unused control bits are zero. |

Each chunk starts with an empty history. **IFF DOLY Fast** tries
PackBits. **IFF DOLY Compact** tries LZSS and PackBits and keeps the
smaller stored representation. The writer uses raw data whenever
compression would fail to reduce padded payload size. At a 16 KiB
batch size, Fast adds 16 KiB and Compact adds 24 KiB of workspace to
the existing read buffers.

The worst-case file size must fit the same signed DOS limit. The writer
updates the FORM length after the last chunk and publishes only after
that update and a successful close.

## Inspection

`dolly-iff image.dly` checks the IFF envelope, lengths and chunk
boundaries. It skips unknown chunks and uses bounded memory. A valid
envelope may still contain missing, invalid or corrupt image data.
The CLI currently does not validate version-1 field values, enforce
coverage, check payload CRCs or implement image restore.

The read-only `dolly_dly_open()` API exposes a decoded `DollyBlockSource`
starting at offset zero and reports the original geometry separately. It
validates the current writer's version 1 and version 2 layout, flags, chunk
bounds, codec IDs, zero padding and complete sector coverage on open. Reads
check each touched chunk's CRC before reporting its bytes as valid. An
untouched payload has not been verified. The source must stay stable while
the decoded view is open.

The reader uses one four-byte index entry per 256 chunks. It caches one
decoded chunk of at most 32 KiB and reserves at most another 32 KiB for
compressed input. Version 2 follows the current writer profile with stored
compressed data smaller than the raw chunk. Larger version 1 raw chunks use
a 4 KiB streaming buffer.

Restore Image detects FORM DOLY by signature and checks its sector size
against the selected destination. It uses the decoded byte count in the
normal Restore warnings. After approval, it verifies every chunk in
cancellable steps before issuing any image writes, including chunks past
the end of a smaller destination. The transfer continues to verify chunks
as it reads them for writing. The stored first LBA is source metadata.
Decoded sector zero maps to the start of the selected destination.

Segmentation, resume and error maps are future format work. New codecs or layouts require an explicit schema.

## References

- [AmigaOS IFFParse Library](https://wiki.amigaos.net/wiki/IFFParse_Library)

Operation checkpoints are described separately in [Dolly Restore Pointer
(DLRP)](DLRP_FORMAT.md).
