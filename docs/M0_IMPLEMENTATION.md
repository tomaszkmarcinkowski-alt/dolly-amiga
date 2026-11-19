# M0 — implementation and handoff

> Historical project record. See the [documentation index](README.md) for the current release.

M0 delivered a portable, read-only inspection foundation for Dolly.
The host tools probe ADF and RAW/HDF candidates, inspect RDB and MBR
including nested RDB, audit ranges and capacity, and validate the IFF
envelope. The BlockSource API and mocked Exec tests cover aligned
device reads and 64-bit offsets. The [M0 checkpoint](SPRINT_M0.md)
records the original scope and then-current test results.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Later development added the [native MC68000 build](NATIVE_SMOKE.md),
[Sheep Inventory](SHEEP_INVENTORY.md), [Surface Scan](SURFACE_SCAN.md)
and [Create Image](CREATE_IMAGE.md). The [roadmap](ROADMAP.md) and
[test matrix](TEST_MATRIX.md) give the current status.
