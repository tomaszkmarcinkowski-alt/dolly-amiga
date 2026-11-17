# M0 — host image inspector
A read-only C99/POSIX inspector is implemented. It currently recognizes **size candidates** for ADF DD/HD, unstructured RAW/HDF candidates, and the outer IFF FORM DOLY envelope. It neither validates DOLY chunks nor recognizes RDB/OFS/FFS. No native Amiga binary exists yet.

Build:
```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
Next: portable BlockSource abstraction, native Amiga I/O adaptation and fuzz-safe RDB discovery.
