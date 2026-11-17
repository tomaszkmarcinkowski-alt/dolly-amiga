# M0 — experimental read-only RDB inspector

`dolly-rdb IMAGE [BASE_OFFSET_BYTES] [DEVICE_BLOCK_BYTES]` scans a regular file. The offset is specified in **bytes**.

In addition, the internal `dolly_partition_map_scan()` API now recognizes primary MBR entries and scans for RDB in type `0x76` containers. It checks MBR range overlap, bounds, RDB checksum/list cycles, nested PART extents, RDB header and PART descriptor overlap.

**Important limits:** `dolly-rdb` CLI is still the manual-offset inspector; there is no new MBR-aware CLI yet. GPT and EBR chains are deliberately rejected as unsupported. Shared-offset legacy hybrid RDB/MBR layouts are not yet handled. RDB FSHD/LSEG/BADB and all reserved-block rules are not yet verified. There is no disk write support.

The MBR parser assumes the selected logical block size for LBA units; verify device block size if used with non-512 logical sectors.

Local CTest covers good nested RDB, overlapping primary entries, child outside its container, bad PART checksum, protective GPT record, and image truncation.
