# DLY / IFF FORM DOLY — M0 envelope and future container

## Confirmed project choices
- File extension `.dly`; IFF FORM identifier is exactly `DOLY` (not `DLYI`).
- Future DLY containers hold chunked image data, codec IDs, hashes, error maps, RDB/MBR metadata, and recovery state.
- For large images, the architecture will split data into independent IFF files or segments plus manifest. **Segment layout is NOT final**.

## What works now
`src/dolly_iff.c` provides *structural* validation for one complete `FORM DOLY` file:
- 12-byte IFF FORM prefix and big-endian 32-bit form length.
- Nonnegative signed-range FORM/chunk lengths, strict exact file length.
- 2-byte chunk padding and bounds checks.
- Streaming chunk visitor and unknown-chunk skipping.
- Bounded number of entries; no payload-sized allocations.
- Read-only CLI: `./build/dolly-iff image.dly`.

A structurally valid FORM with zero chunks **is not a usable Dolly backup**. No mandatory chunk set, codec, decompressor, encryption, checksum, segment index, crash recovery or image restore exists yet. Never claim such a file is a verified disk image.

## Future schema candidates (not yet ratified)
- `HEAD`: schema version, flags, UUID
- `GEOM`: byte capacity, sector size, device/partition offset
- `CODE`: codec catalog and parameters
- `CHNK`: independently checksummed data chunks
- `BMAP`: read/verify/retry map
- `ALOC`: filesystem allocation analysis confidence
- `ERRO`: error details and classification
- `INDX`: chunk index and positions
- `ANNO`, `AUTH`, `DATE`: human metadata

All 64-bit numeric fields will have an explicitly documented byte order. The parser must *not* infer safety of an image merely because the IFF envelope is well formed. Exact chunk meanings and limits remain design work.

## References
- [EA IFF 85 overview](https://en.wikipedia.org/wiki/Interchange_File_Format)
- [AmigaOS IFFParse Library](https://wiki.amigaos.net/wiki/IFFParse_Library)
