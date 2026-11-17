# M0 — primary MBR and nested RDB

The experimental, read-only `dolly_partition_map_scan()` API detects primary MBR partitions and searches for RDB within partition type `0x76` (one common PiStorm/Emu68 layout).

Implemented validation: partition bounds, MBR overlap, RDB and PART checksums/cycles, containment of RDB children inside the MBR 0x76 parent, and header/PART descriptor overlap.

**Unsupported and not yet safe to assume:** GPT and EBR chains, true shared-offset hybrid MBR/RDB arrangements outside a 0x76 container, full reserved-area metadata (FSHD/LSEG/BADB), and Amiga device I/O. The `dolly-rdb` CLI still requires a manually provided offset. No disk writes exist.

A reported valid map is preliminary and **does not** certify the disk as healthy, bootable or safe to modify.

Test on the host with `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.
