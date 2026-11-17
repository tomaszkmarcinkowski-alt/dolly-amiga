# M0 — read-only RDB inspector

`dolly-rdb IMAGE [BASE_OFFSET_BYTES] [DEVICE_BLOCK_BYTES]` operates on an ordinary **file**. It never opens a physical Amiga device and does not write.

- The base offset specifies the beginning of the RDB container, **in bytes**, not a physical disk LBA.
- For an MBR partition containing RDB, the user must currently supply the container's byte offset manually; automatic MBR parsing is not yet implemented.
- The scan checks the first 16 logical blocks relative to the specified base, accepting only a checksummed `RDSK` whose declared block size matches the provided device block size.
- `PART` blocks are followed with checksum verification, a 128-entry list limit, cycle detection, geometry checks and end-of-image range validation.
- Only `PART` structures are parsed. `FSHD`, `LSEG`, `BADB`, RDB boot behavior and overlap among *valid* partitions are **not** yet validated. A successful parse must not be taken as proof that a disk is healthy, bootable or safe to modify.
- The host scanner has a 512–32768 byte power-of-two device-block size limit. It makes no claim of native Amiga support yet.
- Synthetic fixture tests cover correct shifted RDB, wrong checksum, cyclic PART list and out-of-range PART.

## Examples

```sh
./build/dolly-rdb backup.hdf
./build/dolly-rdb pistorm.img 1048576 512
```

The second example means an RDB container beginning at byte 1,048,576; it does not automatically determine whether the MBR actually places such a container there.

Next tasks: MBR bounds parser, overlapping ranges, RDB reserved-block validity, error reporting per partition, API abstraction compatible with Amiga 68000 native build.
