# Dolly — staged testing matrix

## Restore Image development verification

| Scenario | Result |
| --- | --- |
| Published IFF DOLY test binary | The downloaded MC68000 file matched the compiled executable by SHA-256 and opened its main window through `WBStartup` on FS-UAE / A600 / AmigaOS 2.05. Manual ASL selection, Warning Review and Pause/Stop interaction in WinUAE remain to be checked. [Test build](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/releases/tag/iff-doly-restore-test-20261120) |
| Physical-device Restore Image | The maintainer reported completion without an error. Contents of that destination have not yet been compared with the source. |
| IFF DOLY → emulated IDE disk | FS-UAE 3.1.66, A600, Kickstart and Workbench 2.05. A native test harness invoked the normal transfer and warning actions on a disposable 4 MiB HDF with write read-back enabled. All 131,072 decoded PackBits bytes matched the source, and the entire tail, including its sentinel, remained unchanged. |
| Writer-generated IFF DOLY v1 and v2 Compact | Two additional A600 / AmigaOS 2.05 FS-UAE sessions restored writer-generated images to separate 4 MiB IDE HDFs with write read-back enabled. The v1 image contained eight raw chunks. The Compact image contained seven LZSS chunks and one raw fallback chunk. Each restored all 131,072 decoded bytes correctly and preserved the entire destination tail. The subsequent corrupt-image attempt in each session wrote zero bytes. |
| Corrupt IFF DOLY | A separate emulator session rejected a CRC error in the middle of the same-sized image. The transfer reported zero bytes written, and the entire target matched its pre-test copy. |
| Reopen in the same session | FS-UAE 3.1.66 reproduced corrupt INQUIRY and READ CAPACITY responses after a multi-sector transfer, including with cycle-exact enabled. With the ROM V37 retry, both queries reported 8,064 sectors. The second transfer rejected the corrupt image before writing. A host comparison confirmed the entire HDF still matched the previously restored prefix and untouched tail. |

The harness acknowledged warning actions programmatically. These runs establish
native disk I/O and byte integrity for the tested virtual media.

## Alpha 3.3 verification

| Scope | Result |
| --- | --- |
| Host | 36/36 CTest cases passed. Restore coverage includes matching sizes, both directions of size mismatch, bounded writes, RDB diagnostics, and rejection of encoded IFF DOLY input. |
| Build | The GadTools executable cross-compiled for MC68000 with GCC 10 and the AmigaOS toolchain. |
| Native runtime | Verify aligned writes and cancellation on disposable media, both Restore size warnings, and Inhibit/unmount behavior for affected AmigaDOS devices, including a booted volume. WinUAE confirmation for Alpha 3.3 is pending. |

## Alpha 3 verification

| Scope | Observed result | Remaining coverage |
| --- | --- | --- |
| Host CTest | 36 tests, including native Exec mocks, image/target assessment, Restore batch transfer, DLRP serialization and LBA/CHS parsing. | Real-driver behavior. |
| Native build | MC68000 GUI, write adapter and Restore source targets cross-compiled. | Runtime checks of the new scan dialog, Pause/Resume, the Restore timer and checkpoint save. |
| Disk-unit enumeration | User confirmed that the disk-units test build reports the correct units and Query results for scsi.device Unit 0 with a valid RDB and two FFS partitions, and uaehf.device Units 0 and 1 with no RDB. | Additional controller and media configurations. |

### Main-window test build

- MC68000 cross-build and 36/36 host CTest cases passed.
- The maintainer confirmed that an older preferences file triggers the
  version warning and loads defaults.
- Pending on AmigaOS: check the idle and active progress states, Pause/Resume,
  Details opening Sheep Counter, horizontal resizing on 640×256 Topaz 8 and
  a proportional font, and applying a font choice without restarting.
- [Download the test executable](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/releases/tag/main-window-test-20261119).

### Alpha 3.1 warning review and resize

- MC68000 cross-build passed. Host CTest passed 36/36.
- Pending on AmigaOS are warning order, navigation and Cancel, failed
  unmount handling, and source inhibition cleanup. Also verify resizing
  in both directions at 640×256 with Topaz 8 and a proportional font.

### Sector copy and RAW/HDF Restore

- Host coverage includes bounded batches, flush/readback failure, refusal
  of missing NSD flush support, and rejection of renamed IFF DOLY images. A detected MBR/PC signature is advisory for whole-device RAW transfers.
- Pending native runtime checks on disposable media: copy and restore byte
  comparison, offline/busy rejection, Stop, optional verify and DLRP capture
  after a forced failure.

### 512 MiB RDB image on emulated IDE

The supplied HDF contains 1,048,576 sectors of 512 bytes and two valid
PART blocks. UDH0 spans LBA 4032–526175 and UDH1 spans LBA
526176–1048319. In FS-UAE 3.1.66 on A600 with the supplied Kickstart
37.350, the emulator logged translated IDE CHS of 917/9/127. The native
probe saw TD_GETGEOMETRY fail, then received last LBA 1,048,130 from
READ CAPACITY(10), yielding exactly 917 × 9 × 127 = 1,048,131 sectors.

A regression check of the native parser against the supplied RDB
reads both entries with either capacity and flags only UDH1 when
using the reported IDE limit. It also rejects a corrupted checksum.
The image was attached read-only.

## Historical emulator verification through Alpha 2

| Scope | Observed result | Remaining coverage |
| --- | --- | --- |
| Host CTest | 32/32 passed | GitHub Actions is tracked separately. |
| Native build | MC68000 `dolly-two-window-demo` compiled with the m68k toolchain. | Physical Amiga certification. |
| Sheep Inventory | FS-UAE A600 / AmigaOS 2.05 exercised RDB/MBR detail views and clipboard. | Real controllers and broader disk layouts. |
| Surface Scan | Whole-unit and partition checks, a 5 GiB image with distinct test data across 4 GiB, retry/error injection, LBA/CHS log export and Sheep Counter resize. | Interactive ASL selection for log export, all-media coverage and intermittent READ CAPACITY(10) sentinel on reopen. |
| Create Image | FS-UAE wrote a 6,193,152-byte partition HDF to RAM: and matched the source byte for byte. DLY v1 for that extent passed independent CRC32 verification for 378 data chunks. | Interactive ASL selection, output to a separate physical unit and ADF creation. |
| DLY v2 | Both modes decoded byte for byte with independent CRC verification on a 192-sector mixed fixture. Cycle-exact A600: Fast 66,276 bytes in 7.40 s, Compact 38,164 bytes in 9.56 s, raw DLY reference 98,508 bytes. | Larger media and additional profiles. |

The sections below retain historical plans and results.


Created after acceptance of M1.19 on MC68000 / Kickstart 2.04 /
Workbench 640x256. **Planning document only:** listing a test does not
mean it was run. User deferred RTG tests and memory profiling until
later. Keep the accepted 640x256 native GUI as the reference.

## Recording results

For each test write: ID, branch + commit, exact WinUAE configuration,
date, steps, expected and actual behavior, result
(PASS/FAIL/BLOCKED/NOT RUN) and screenshot or console evidence if failed.
Retest changed and adjacent behaviors with each GUI milestone.

## A. Immediate 640x256 regression tests (priority: now)

| ID | Scenario | Expected result | Current evidence |
|---|---|---|---|
| GUI-01 | Launch and exit | Main window only at startup; clean exit, no #8000000B | User confirmed M1.18/M1.19 |
| GUI-02 | Operation selection | Description changes; destination enabled only for Copy/Restore/Create image | User accepted baseline |
| GUI-03 | Physical device | Two touching equal-width lists, parent-child gating | User accepted baseline |
| GUI-04 | Partition | Three touching equal-width lists, device -> unit -> partition | User accepted baseline |
| GUI-05 | Logical device | Single full-width list and selection | User accepted baseline |
| GUI-06 | File Source | ASL accepts full and current-directory filenames; path displayed; metadata size or unavailable | User accepted baseline |
| GUI-07 | File Destination | RAW/HDF/ADF/IFF DOLY Cycle, independent pending file and explicit Use selection | User accepted baseline |
| GUI-08 | Cancel/rollback | ASL Cancel leaves previous selection; chooser Cancel does not commit | Additional explicit regression planned |
| GUI-09 | Toggle type repeatedly | No leftover LISTVIEW pixels, labels or overlapping gadgets | User confirmed earlier fix |
| GUI-10 | Resize Source horizontally/vertically | Gadget widths and list heights follow window; no old-size flashes | User accepted M1.19 at 640x256 |
| GUI-11 | Resize Destination horizontally/vertically | Same behavior as Source, independent selection | User accepted M1.19 at 640x256 |
| GUI-12 | Resize with selection in progress | Selected entries and enabled states remain; no accidental commit | Explicit regression planned |
| GUI-13 | **NEW: pending-selection inset** in Physical | Inset panel below list, above Use selection; device then unit text updates | **NOT RUN** |
| GUI-14 | **NEW: pending-selection inset** in Partition | Inset shows device / unit / partition; panel never overlaps list or buttons | **NOT RUN** |
| GUI-15 | **NEW: pending-selection inset** in Logical | Inset shows selected mounted device | **NOT RUN** |
| GUI-16 | **NEW: pending-selection inset resize** | Panel follows window width and stays above buttons on vertical resize | **NOT RUN** |
| GUI-17 | File mode regression after new inset | Existing File path/metadata/format unchanged; no duplicate inset | **NOT RUN** |
| GUI-18 | Main window resize | Horizontal only; Blocks does not touch Start; recessed Status remains bottom | User accepted earlier milestone |
| GUI-19 | Sheep Counter lifecycle | Closed while idle, available while running, Hide never cancels; progress resumes | Prior GUI and CTest coverage |
| GUI-20 | Source/target conflict | Same file/unit/overlapping partition rejected before Start | Existing CTest coverage + future GUI check |
| GUI-21 | Create image / Restore | Create image destination File; Restore source File; no actual file writer in demo | Existing model + prior GUI |

The **first next test** should be GUI-13..17 on the current WinUAE
reference setup, alongside GUI-03..11 regression. Do not record a PASS
until results are observed.

## A.1 Picker commit and double-click regression (next 640x256 run)

| ID | Input | Expected |
|---|---|---|
| GUI-22 | Physical device: Device 0, Unit 0, Use selection | Main Source/Destination shows whole unit; chooser closes; Start gated correctly |
| GUI-23 | Partition: Device 0, Unit 0, Partition 1, Use selection | Main label includes partition index; chooser closes |
| GUI-24 | Logical device: volume 0, Use selection | Main label shows chosen mounted volume |
| GUI-25 | Physical: double-click selected Unit | Equivalent to Use selection, commits once |
| GUI-26 | Logical: double-click selected Volume | Equivalent to Use selection, commits once |
| GUI-27 | Partition: double-click partition only | Equivalent to Use selection; parent Device/Unit double-clicks do not commit |
| GUI-28 | Fast clicks on different Unit/Volume rows | Never commit when rows differ |
| GUI-29 | Resize/switch type between clicks | Double-click timer invalidated, no accidental commit |
| GUI-30 | File picker ASL and Use selection | File remains explicit-confirm only, no regression |
| GUI-31 | Cancel after partial hierarchy | Previous selection in main window unchanged |
| GUI-32 | Source and Destination for Copy | Both independently commit; target conflicts still refused |

This test group was added after user reported that Physical and
Partition selections did not appear in the main window. The picker
now registers complete LISTVIEWIDCMP, CYCLEIDCMP and BUTTONIDCMP
event masks and routes button/double-click through one commit function.
**Pending compilation and WinUAE verification.**

## B. Next implementation: real, read-only discovery

| ID | Test | Pass criteria |
|---|---|---|
| DISC-01 | Enumerate mounted AmigaDOS logical volumes | Real names without changing mount state |
| DISC-02 | Resolve logical device startup info | Device/unit mapping where available, explicit unknown otherwise |
| DISC-03 | Probe physical device units | Existing units only, correct error for absent/no-media |
| DISC-04 | Read synthetic RDB partition fixture | Correct offsets/sizes/names, never write |
| DISC-05 | Read synthetic MBR partition fixture | Correct offsets/sizes/names, never write |
| DISC-06 | Malformed RDB/MBR | No phantom partitions, no crash or unbounded traversal |
| DISC-07 | Partial I/O, device error and timeout | Clear status, cleanup, main GUI remains usable |
| DISC-08 | Inventory refresh with picker open | No dangling list entries; selection not silently changed |
| DISC-09 | Logical vs physical aliases | Detect same underlying disk/partition before any future write |

**Gate:** read-only API paths verified; corrupt or missing data cannot
crash the application. Actual write access remains disabled.

## C. Future format, recovery and imaging tests

| ID | Scenario | Pass criteria |
|---|---|---|
| FMT-01 | RAW/HDF/ADF selection | Chosen output format preserved, not inferred from extension alone |
| FMT-02 | ADF geometry validation | Invalid source geometry rejected |
| FMT-03 | IFF DOLY fixtures | Correct FORM/chunk layout, endian and padding; unknown chunks handled |
| FMT-04 | File signature probing | Separate verified content type from filename hint |
| FMT-05 | Relative filename handling | Correct original directory context, no ambiguous write path |
| FMT-06 | Large file / >32-bit block count | No signed overflow, existing 64-bit planner tests reused |
| FMT-07 | Nonexistent/empty/truncated file | Accurate error, no false success |
| FMT-08 | Same source and target, including aliases | Refuse before creating any output |
| FMT-09 | Existing destination | Explicit overwrite confirmation, Cancel has no side effects |
| FMT-10 | Read errors, retries, cancellation | Status, counters, abort and resource release consistent |
| FMT-11 | Create image progress | Main and Sheep Counter reflect actual processed blocks |
| FMT-12 | Partial output / volume full | Report partial data, no silent success |

Future disk-write tests must be isolated to explicitly authorized
**sacrificial test media**, with verified alias detection and safeguards.

## D. Deferred compatibility and optimization

| ID | Scenario | Status |
|---|---|---|
| RTG-01 | 800x600 / 1024x768 and graphics-card Workbench modes | DEFERRED by user |
| RTG-02 | Extreme resizing, font sizes, palettes and pixel aspect | DEFERRED |
| RTG-03 | Multiple windows, screen switches and refresh storms | DEFERRED |
| MEM-01 | Peak Chip/Fast usage with picker, ASL, Sheep Counter | DEFERRED |
| MEM-02 | Long reopen/resize runs and leaks | DEFERRED |
| PERF-01 | Progress/scan CPU time and redraw cost | DEFERRED |
| COMP-01 | OS 3.x, 68020/040/060 and Fast RAM variants | DEFERRED |
| RELEASE-01 | Full host CTest, Docker -Werror build and WinUAE smoke | Prior to release |

## Existing automation (do not duplicate without reason)

The repository already defines CTest cases such as
\`gui_content_minimum\`, \`endpoint_selection_safety\`,
\`operation_session_hide_does_not_cancel\`,
\`gadtools_640x256_font_fallback\`,
\`screen_preferences_relayout\`, \`rdb_read_only\`,
\`partition_map_read_only\`, \`file_probe\`, \`iff_form_doly\`,
\`io_64bit_planning\`, \`amiga_exec_mock\`,
\`inventory_streaming\`, \`rdb_collision_audit\`,
\`crc32_integrity_stream\` and other conditional tests.
Existing CTest targets do not automatically imply that native GUI
behavior or future read/write paths have been tested.

Host command:
\`\`\`sh
cmake -S . -B build-native-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native-tests -j2
ctest --test-dir build-native-tests --output-on-failure
\`\`\`

Native UI acceptance remains on the established WSL2 -> Docker m68k
build -> WinUAE route. Do not run AmigaOS binaries on the WSL host.

## E. M1.21 live AmigaDOS logical discovery (next WinUAE run)

This is a **read-only preview** of names registered in the AmigaDOS
device list (LockDosList(LDF_DEVICES|LDF_READ) /
NextDosEntry(LDF_DEVICES)). A DOS device list entry is **not**
proof that a medium is currently inserted or that a filesystem is
mounted. The physical and partition chooser lists remain synthetic.

| ID | Case | Expected result |
|---|---|---|
| DISC-10 | Open Logical on reference Workbench 640x256 | Real DOS device names, not synthetic DH0/DH1 |
| DISC-11 | Machine has DF0: and/or DH0: | Present names appear only if registered; no fabricated entries |
| DISC-12 | Select registered DOS device and Use selection | Main shows its actual name with colon; Source/Destination independent |
| DISC-13 | Double-click registered device | Same result as Use selection |
| DISC-14 | Repeat opening both pickers | No crash, stale names or unexpected mutation |
| DISC-15 | Switch modes, resize, Cancel | Selection controls remain usable; Cancel does not commit |
| DISC-16 | No registered entries or discovery unavailable | Explicit preview message; no selectable fake item |
| DISC-17 | More than 24 entries | No overflow, explicit first-24 limitation |
| DISC-18 | Source and target refer to same DOS name | Refuse Copy even if entry ordering changes |
| DISC-19 | Physical device / Partition after upgrade | Clearly synthetic test labels remain; no physical I/O |
| DISC-20 | Startup Shell and exit | No Software Failure, exit cleanly |

**Do not infer** media presence, unit index, device driver or RDB/MBR
from a DOS registration name. No OpenDevice(), DoIO(), TD_READ,
CMD_READ or write operation is introduced in M1.21.

State: **implementation committed; untested in Docker/WinUAE**.
RTG tests and RAM profiling are still postponed.

## F. M1.22 filtering and disk-identification panel (reference 640x256)

| ID | Action | Expected |
|---|---|---|
| DISC-21 | Open Logical device on AmigaDOS | RAW:, CON:, RAM:, SER:, PAR:, PRT: absent regardless of case |
| DISC-22 | Select nonstandard DOS device name | Not excluded solely for unusual name (entry still not verified block) |
| DISC-23 | Physical: select Device then Unit | First inset shows hierarchy; second inset reports INQUIRY not queried (demo) and size unavailable |
| DISC-24 | Partition: select Device, Unit, Partition | Both inset panels stay visible; second panel applies to selected physical unit |
| DISC-25 | Switch modes and resize on 640x256 | No overlapping gadgets; list/panel and panel/panel spacing approx. 3 px |
| DISC-26 | Double-click Physical Unit / Partition | Closes chooser and commits selection; no stale info |
| DISC-27 | Logical/File modes | No unintended second disk-info panel |
| DISC-28 | No hardware discovery yet | No SCSI commands sent; demo fields do not claim real identity |
| DISC-29 | Explicit physical discovery later | Query only verified driver/unit, INQUIRY vendor/product/revision then GETGEOMETRY or READ CAPACITY; unsupported queries report unavailable |

The six-name exclusion is an explicit known-nonblock filter,
**not a complete, authoritative test of block-device capability**.
AmigaDOS DLT_DEVICE includes ordinary file/stream handlers.
The second inset is deliberately provisional: the Physical/Partition
inventory still uses synthetic driver/unit candidates and must not
issue HD_SCSICMD or TD_GETGEOMETRY to an invented endpoint.

## G. M1.22 vertical margins for non-square-pixel screens

| ID | Screen | Expected |
|---|---|---|
| GUI-33 | 640x256 classic Workbench | 5 px tighter top content, bottom action buttons 8 px nearer window edge; no overlap |
| GUI-34 | 640x256 Physical/Partition | Both inset panels remain 3 px apart and below list; extra space enlarges LISTVIEW |
| GUI-35 | 640x256 File Source/Destination | Browse/path/details/format captions follow new top offsets; Use/Cancel stay reachable |
| GUI-36 | Proportional 4:3 display, later | Original baseline vertical offsets retained; RTG test remains deferred |
| GUI-37 | 640x256 resizing and mode changes | No stale pixels, clipping, inaccessible controls or lost selections |

The compact-mode criterion is screen width > twice screen height,
rather than a hardware/RTG detection or fixed display-name check.
It targets non-square-pixel classic Workbench geometries while leaving
4:3-like screen ratios unchanged. A screen with conventional square
pixels and very wide proportions may also trigger compact mode.

## H. M1.23 verified physical driver/unit diagnostic

This milestone introduces a **separate Shell executable**,
\`dolly-physical-probe\`. The normal Dolly GUI remains unchanged.
Before integrating probe results into Physical device and Partition
pickers, obtain a reliable physical-driver/unit map on the reference
WinUAE setup.

| ID | Command or case | Expected |
|---|---|---|
| PHYS-01 | \`dolly-physical-probe\` | Prints bounded real DOS startup candidates, with **zero** OpenDevice/DoIO calls |
| PHYS-02 | Repeated no-argument enumeration | Stable candidates, no Guru / Software Failure |
| PHYS-03 | \`dolly-physical-probe --probe 0\` | One selected candidate only; OpenDevice handled or cleanly reported |
| PHYS-04 | Supported unit | TD_GETGEOMETRY reports sector count/size and capacity in MiB |
| PHYS-05 | Driver supports SCSI-direct | INQUIRY returns model/manufacturer/revision; missing support reports unavailable |
| PHYS-06 | Driver without media | Reports unavailable without invented geometry or product |
| PHYS-07 | Non-disk handler startup value | No unvalidated BPTR dereference; handler excluded from probe candidates |
| PHYS-08 | Multiple DOS partitions on one driver/unit | Only one physical candidate entry |
| PHYS-09 | Invalid index or omitted index with --probe | No device opened |
| PHYS-10 | Reopen existing Dolly GUI | Previously accepted 640x256 Source/Destination functionality unchanged |

Safety boundaries:
- Enumeration only takes the AmigaDOS device-list **read lock**;
  it does not call OpenDevice until the explicit --probe INDEX option.
- A candidate is accepted only if startup BPTR, FileSysStartupMsg,
  BSTR driver name and DosEnvec show plausible structure and the
  driver name ends with .device. These checks are heuristic, **not a
  universal proof** that a third-party handler follows this structure.
- Probe sends only TD_GETGEOMETRY and HD_SCSICMD with opcode 0x12
  (SCSI INQUIRY). No CMD_READ/CMD_WRITE, no TD_FORMAT, no partitions
  parsed yet. The diagnostic sends commands synchronously and does
  not yet have a timeout; test **only in WinUAE** until reviewed.
- Chip RAM buffers are used for the SCSI data/CDB. Driver-specific
  noncompliance can still produce unexpected behavior. Never treat
  read-only intent as a guarantee of driver stability.

**Gate before UI integration:** successful 640x256 WinUAE candidate
listing and explicit safe probing, including at least one device and
one unsupported/no-medium outcome. RTG and memory profiling remain
deferred by the user.

## I. M1.24 inline labels and independently recessed disk size

Current UI refinement (source and destination) on the 640x256
reference screen. This change is independent of the M1.23 console
probe and does not issue any new device commands.

| ID | Mode / action | Expected |
|---|---|---|
| GUI-38 | Physical device, select unit | Upper inline label "Selected device", followed by recessed chosen endpoint |
| GUI-39 | Partition, select a partition | Upper inline label "Selected partition", followed by recessed chosen hierarchy |
| GUI-40 | Logical device | Upper inline label "Selected device", no redundant disk-identification row |
| GUI-41 | File source and destination | Upper inline label "Selected file", pathname field same row, ASL and Use selection unchanged |
| GUI-42 | Physical/Partition bottom row | Left label "Device name" and inset value, right label "Size" and separate inset value |
| GUI-43 | No real physical probe in GUI | Identity says "Not queried (demo)" and Size "Unavailable", with no invented capacity |
| GUI-44 | Resize picker and change modes | Size box stays right anchored, name box expands, labels do not overlap fields |
| GUI-45 | File source details / destination format | "File info" and "Image format" labels remain beside their respective fields |
| GUI-46 | Source and Destination selection + double click | Previously accepted commit, rollback and double click behavior preserved |

**Pending:** Docker m68k compile and WinUAE confirmation. RTG remains
deferred.

## J. M1.24 bridge: independent Device name and Size diagnostics

The approved M1.24 GUI already has two separate recessed fields:
Device name and Size. The independently launched diagnostic now prints
exactly those two keys; it still does **not** populate the GUI directly.

| ID | Scenario | Expected |
|---|---|---|
| PHYS-11 | No-argument physical candidate listing | No physical device opened; no geometry or SCSI queries |
| PHYS-12 | Explicit probe of supported SCSI-direct device | Device name contains sanitized INQUIRY vendor/product |
| PHYS-13 | Geometry supported | Size is independently reported in MiB |
| PHYS-14 | SCSI-direct unsupported, geometry available | Device name: Unavailable, Size: measured value |
| PHYS-15 | Geometry unsupported, SCSI INQUIRY available | Device name: product, Size: Unavailable |
| PHYS-16 | OpenDevice failure | Both labeled fields Unavailable; device not CloseDevice'd |
| PHYS-17 | GUI Source/Destination Physical/Partition | Keeps the two inset fields unchanged, still displays demo placeholders |

**Integration gate:** do not wire physical probe results to the
selection dialog until real driver/unit candidates and read-only query
results have been verified in WinUAE. Do not map hardware to the
synthetic Device/Unit indices.

## K. M1.24 WinUAE regression: physical candidates always zero

Observed: initial 68000/OS2.04 with no controller media produced
Candidates: 0, skipped/unknown: 8. A1200/OS3.1 with IDE disk attached
to scsi.device still produced Candidates: 0, skipped/unknown: 14.

Do **not** infer that the Amiga does not have an IDE drive from this.
The existing strict FileSysStartupMsg/DosEnvec heuristic may reject
legitimate handlers or structures. An extra no-physical-I/O diagnostic
now records each DOS entry's name and first failure reason, then
prints diagnostics *after* releasing the LockDosList read lock.
There is no automatic OpenDevice/DoIO.

| ID | Run | Expected |
|---|---|---|
| PHYS-18 | dolly-physical-probe, no args | If no candidates, prints device-by-device rejection reasons |
| PHYS-19 | dolly-physical-probe --inspect | Always prints per-entry reasons (no hardware I/O) |
| PHYS-20 | Boot A1200/OS3.1 with scsi.device IDE | Determine whether actual disk's DOS node is rejected by pointer, environment validation or .device name |
| PHYS-21 | Diagnostic errors | No unvalidated BADDR dereference, no calls to OpenDevice or DoIO for enumeration |

Do not enable an automatic GUI probe until the actual IDE DOS startup
record can be identified and validated. The M1.24 Device name/Size
recessed fields remain distinct.

## L. Physical candidates: A1200 / OS3.1 diagnostic findings

Observed \`--inspect\` after attaching an IDE HDF on \`scsi.device\`:
14 registered DOS device entries; 0 candidates. In particular,
OS:, WB_1.3:, WB_2.x: and GRY: reached DosEnvec, with
\`de_TableSize=16\` and \`de_SizeBlock=128\`. Those values are
ordinary (128 longwords = 512-byte blocks). Other devices,
such as PIPE:, RAM:, CON:, SER:, PAR:, PRT:, are not assumed
to be physical disks. DF0: and CC0: have invalid driver-name
pointers or bytes under current heuristic; do not force them.

The previous extractor incorrectly treated \`de_Surfaces\` and
\`de_BlocksPerTrack\` as strict requirements for *identifying*
a driver/unit. These are filesystem geometry hints; a virtual
disk's hints need not correspond to measured hardware geometry.
The extractor now accepts structurally plausible filesystem
startup entries without requiring heads or sectors-per-track
to be nonzero or below arbitrary small limits, but retains
bounds on table size and block size, and still requires a
valid BSTR driver name with a .device suffix. In \`--inspect\`
it reports table, blockwords, heads, spt, driver and unit.

Tests:
- PHYS-22: OS3.1 A1200 IDE / --inspect: reveal whether OS:/WB/GRY
  have recognized .device driver/unit tuples
- PHYS-23: without --probe INDEX: never OpenDevice/DoIO, including
  when a candidate is discovered
- PHYS-24: unsupported handler/startup: excluded safely rather
  than interpreted as a physical disk
- PHYS-25: Device name and Size remain distinct; GUI still shows
  unavailable until a tested real unit is integrated

This patch is **not yet compiled or WinUAE tested**.

## M. A1200/OS3.1 IDE: every disk DOS entry fails driver BSTR validation

New user WinUAE result: DB0:/CC0:/OS:/WB_1.3:/WB_2.x:/GRY:/DL0:
reach DOS startup geometry, including ordinary \`table=16\` and
\`blockwords=128\`, but fail \`fssm_Device\` BSTR validation.
Even the directly mounted IDE disk on \`scsi.device\` does not
appear as a physical candidate. Do not further relax geometry
checks or guess/force scsi.device unit numbers.

The next no-I/O diagnostic now captures, **only for failed driver
BSTR validation**, the raw BCPL pointer, its translated address,
Exec TypeOfMem result, reported BSTR length (only if the first byte
is in valid memory) and up to 8 validated bytes in hexadecimal.
This enables differentiation among invalid pointer, unusual memory
mapping, empty/oversized BSTR and actual unexpected contents.

Test M-01: build and run \`dolly-physical-probe --inspect\` in the
same A1200/OS3.1/WinUAE configuration. Record one or two complete
entries of OS:/WB_1.3:/DL0: with the new bracketed diagnostics.
Never run \`--probe INDEX\` until a correctly identified candidate
appears; current normal enumeration performs no physical I/O.

## N. M1.24 A1200 driver BSTR: trailing zero identified

WinUAE OS 3.1 screenshot showed validated addresses and recognizable
ASCII hex prefixes for driver names. For example: OS: length=12,
bytes \`736373692e646576\` = "scsi.dev...", whereas "scsi.device"
is 11 characters. DB0: length=17 versus "trackdisk.device" at 16;
CC0: 16 versus "carddisk.device" at 15; DL0: 13 versus
"uaehf.device" at 12.

Likely cause: startup BSTR includes an optional terminal NUL counted
in its length. The original parser rejected control bytes entirely.
The amended parser first validates memory for the complete length,
accepts exactly one terminal NUL, and continues rejecting any
embedded NUL or other control byte. The DOS device-name path uses
the same bounded function.

| ID | Next WinUAE check | Expected |
|---|---|---|
| PHYS-26 | A1200/OS3.1 IDE, no arguments | Detect real \`scsi.device\` and other valid \`.device\` pairs |
| PHYS-27 | \`--inspect\` | Shows accepted driver/unit candidates; no I/O |
| PHYS-28 | Other handler names, unknown formats | Still rejected when their startup data is not a well-formed device record |
| PHYS-29 | Physical probe | Not run until candidate driver/unit verified against actual emulator configuration |

Do not automatically send read-only hardware commands merely because
candidate enumeration now succeeds. Keep the approved GUI's
Device name and Size in **separate fields**.

## O. Device support decision: trackdisk only excluded

- `trackdisk.device`: excluded from the normal Physical/Partition
  picker and generic imaging; floppy support is reserved for a future
  filesystem repair module
- `carddisk.device`: **keep** in discovery and normal selection;
  study PCMCIA SRAM-specific behavior with real hardware later
- `scsi.device`, `uaehf.device`: current controlled WinUAE targets

| ID | Scenario | Expected |
|---|---|---|
| PHYS-30 | Enumerate with DF0: | trackdisk.device reported as excluded, absent from probeable candidates |
| PHYS-31 | Enumerate with CC0: | carddisk.device remains a candidate (not silently filtered) |
| PHYS-32 | Reference GUI | No trackdisk entry; carddisk is retained in synthetic picker until real discovery replaces it |
| PHYS-33 | Later SRAM card hardware test | Record carddisk-specific geometry / errors without assuming SCSI behavior |
| PHYS-34 | Future filesystem repair | Floppy-specific handling independent from generic disk workflow |

Following the filter, do not reuse an old --probe INDEX; run the
enumeration again and confirm the driver/unit identity first.

## P. M1.25 named, confirmed driver/unit probe

The previously accepted no-I/O discovery returned four unique
devices before excluding trackdisk.device:
trackdisk.device unit 0, carddisk.device unit 0,
scsi.device unit 0, uaehf.device unit 0.
After the agreed floppy exclusion the list is reordered.
Do not reuse an old numerical INDEX for a physical probe.

M1.25 adds a stable alternative:
\`dolly-physical-probe --probe-driver scsi.device 0\`

The diagnostic first enumerates and validates AmigaDOS startup
driver/unit records without device I/O. It only calls OpenDevice
when the exact requested driver name and numeric unit match one of
those current validated candidates. It refuses missing, stale or
unrecognized pairs; trackdisk.device cannot be probed through this
candidate list. carddisk.device is still enumerated but its
special characteristics are postponed for testing with a real PCMCIA
SRAM card.

| ID | Test | Expected |
|---|---|---|
| PHYS-35 | List with no arguments | Enumerates without opening devices; trackdisk excluded, carddisk retained |
| PHYS-36 | \`--probe-driver scsi.device 0\` | Opens only confirmed scsi.device unit 0; separately reports Device name and Size |
| PHYS-37 | \`--probe-driver scsi.device 9999\` | No match; does not call OpenDevice, returns error |
| PHYS-38 | \`--probe-driver trackdisk.device 0\` | Excluded; cannot call OpenDevice |
| PHYS-39 | \`--probe-driver scsi.device bad\` | Invalid argument, no I/O |
| PHYS-40 | scsi.device SCSI INQUIRY unsupported | Device name Unavailable, independently report capacity if geometry works |
| PHYS-41 | scsi.device TD_GETGEOMETRY unsupported | Size Unavailable, independently report INQUIRY if supported |

This opt-in diagnostic still uses synchronous DoIO without a
timeout; run only in controlled WinUAE, on a test image. Automatic
GUI probing remains disabled pending test verification. The M1.24
GUI uses two distinct recessed fields: Device name and Size.

## Q. M1.26 WinUAE: SCSI INQUIRY succeeds but Size unavailable

Confirmed on A1200/OS3.1/WinUAE with \`scsi.device unit 0\`:
- Discovery shows carddisk.device 0, scsi.device 0 and uaehf.device 0;
  trackdisk.device is deliberately excluded.
- SCSI INQUIRY returns real virtual identity **UAE-IDE rafael.hdf**.
- Original TD_GETGEOMETRY path reported **Size: Unavailable**.
  This alone is not evidence of a zero-sized drive.

Next build:
- Allocate a full \`struct IOExtTD\` for the block-device request
  (rather than a smaller \`IOStdReq\`).
- Print TD_GETGEOMETRY return status, io_Error and actual geometry.
- If geometry fails or reports zero size, send optional read-only
  SCSI READ CAPACITY(10), opcode 0x25, with a 10-byte CDB and 8-byte
  result in Chip RAM. Interpret big-endian last-LBA-plus-one and
  block size as capacity, with explicit 0xFFFFFFFF limit handling.
- Preserve independent **Device name** (INQUIRY) and **Size**
  (TD_GETGEOMETRY or READ CAPACITY) outputs.

| ID | Test | Expected |
|---|---|---|
| PHYS-42 | \`--probe-driver scsi.device 0\` | INQUIRY still identifies virtual UAE-IDE; geometry status printed |
| PHYS-43 | TD_GETGEOMETRY unavailable | Optional READ CAPACITY(10) attempted and result printed |
| PHYS-44 | READ CAPACITY succeeds | Size computed from (last LBA + 1) * bytes/block |
| PHYS-45 | READ CAPACITY unavailable | Device name retained, Size: Unavailable, no false value |
| PHYS-46 | \`--probe-driver trackdisk.device 0\` | Not in candidates; no OpenDevice |
| PHYS-47 | Plain listing | No disk I/O, no geometry or INQUIRY commands |

No automatic GUI probe yet. M1.24 still presents Device name and
Size in **two separate recessed boxes**. Do not connect commands to
physical UI selections until query results are verified under WinUAE.
READ CAPACITY(10) does not support >2 TiB: those drives remain
unavailable pending READ CAPACITY(16) support. Commands use
synchronous DoIO without timeout, so run only with a test HDF.

## R. M1.26 WinUAE verified — A1200 IDE HDF

Observed in WinUAE (Amiga 1200, AmigaOS 3.1, internal IDE using
`scsi.device`, unit 0), after `--probe-driver scsi.device 0`:

- Candidates: 3: `carddisk.device` unit 0, `scsi.device` unit 0,
  `uaehf.device` unit 0. `trackdisk.device` excluded as agreed.
- `TD_GETGEOMETRY`: `rc=-1`, `error=-1`,
  `sectors=0`, `sector_bytes=0` — unusable on this controller.
- `INQUIRY`: `rc=0`, `scsi=0`, `actual=36`,
  `Device name: UAE-IDE rafael.hdf`.
- `READ CAPACITY(10)`: `rc=0`, `scsi=0`, `actual=8`;
  `Size: 1881 MiB`.

Therefore PHYS-42, PHYS-43 and PHYS-44 have been **verified
in WinUAE** for this configuration. This proves read-only capacity
fallback works; it does not prove TD_GETGEOMETRY behavior on other
controllers or a safe automatic GUI probe. Continue with a separate,
bounded hardware-discovery GUI integration, retaining the M1.24
`Device name` and `Size` recessed fields as independent values.

## S. M1.27 — user-driven physical Query in the native GUI

| ID | Test on 640x256 | Expected |
|---|---|---|
| GUI-47 | Open Source/Physical | Real DOS-startup driver list (no trackdisk), NO physical I/O |
| GUI-48 | Select scsi.device | Real registered units shown, Query remains disabled until unit selected |
| GUI-49 | Select Unit 0 but do not press Query | Device name says Press Query; Size says Not queried, no OpenDevice/DoIO |
| GUI-50 | Press Query on scsi.device unit 0 | Device name = UAE-IDE rafael.hdf, Size = 1881 MiB on observed WinUAE HDF |
| GUI-51 | Query again | Explicit second query, no automatic background retries |
| GUI-52 | Switch driver/unit after Query | Previous results cleared; displays prompt until next Query |
| GUI-53 | Resize after Query | Values persist, separate right-hand Size box and Query alignment stay correct |
| GUI-54 | Double-click Unit without Query | Commits physical selection; no implicit device I/O |
| GUI-55 | Physical Use selection | Main window displays real driver name/unit, not Demo device index |
| GUI-56 | Source/Destination independence | Querying one chooser does not change the other's selection |
| GUI-57 | Partition mode | Real device/unit list but empty/disabled Partition list; no fabricated RDB/MBR |
| GUI-58 | Logical/File modes | No Query button, existing behavior unchanged |
| GUI-59 | carddisk.device | Visible, but special behavior deferred to physical SRAM test |
| GUI-60 | Missing media or unsupported query | Unavailable per field; no crash or invented size |
| GUI-61 | Test release | Native m68k -Werror and portable endpoint test pass |

Query is the **only** GUI action that sends physical device commands.
All other actions (opening, listing, selecting, double-clicking,
resizing, switching modes, cancelling) must remain free of OpenDevice
and DoIO. Synchronous commands currently have no timeout, so test
this build on WinUAE disposable images only. RTG and memory testing
are still deferred.


## T. M1.28 — explicit Query, partition scanning and details

| ID | Scenario | Expected |
|---|---|---|
| GUI-62 | Physical, select Unit 0 | Use selection disabled; no device I/O |
| GUI-63 | Physical, Query succeeds | Device name and Size independently populated; Use enabled |
| GUI-64 | Physical, Query fails | Use stays disabled; no false success |
| GUI-65 | Change driver or unit after Query | Old identity/capacity cleared, Use disabled |
| GUI-66 | Double-click Unit / Volume / Partition | No automatic Use/Close in ANY mode |
| GUI-67 | Logical/File Use selection | Explicit button unchanged; no double-click |
| GUI-68 | Partition, before Query | Third list empty, Use disabled, extra Partition info inset visible |
| GUI-69 | Partition, Query finds valid whole-disk RDB | Third list shows actual PART names (e.g. OS:, WB_1.3:), no duplicates invented |
| GUI-70 | Highlight RDB partition | Extra inset shows volume, MiB, DosType, LowCyl, HighCyl, block size, sectors-per-block |
| GUI-71 | Valid simple MBR, Query | Four primary entries at most, each labelled MBR n; metadata uses type code and LBA, no fabricated volume/cylinders |
| GUI-72 | No recognized map or invalid RDB checksum | Third list empty, Use disabled, explicit status |
| GUI-73 | GPT / extended MBR / MBR 0x76 nested RDB | Unsupported, no selectable unverified partition |
| GUI-74 | RDB cycle or partition extent outside medium | Reject map without silently returning partial partitions |
| GUI-75 | Change selection after partition Query | Partition nodes and detail cleared before list rebuild |
| GUI-76 | Resize 640x256 Partition window | Third inset above actions; no list overlap or field clipping beyond approved widths |
| GUI-77 | Device-vs-partition copy of the same unit | Reject same physical address even when displayed endpoint strings differ |
| GUI-78 | Different units 1 and 10 | Not incorrectly identified as same unit |
| GUI-79 | Source vs Destination | Query and partition selection are independent, Cancel does not commit |
| GUI-80 | Native cross-compile / endpoint portable test | \`-Werror\` build and \`test-endpoint\` pass |

**Strict safety gate:** these are tests to run, not claims of PASS.
Do not run this unverified build against real media. Scanner reads
only on explicit Query; no sector writes exist, and the source
driver may still block on synchronous DoIO. Use the controlled
WinUAE A1200/OS3.1 test HDF. RTG and memory usage profiling remain
postponed.

## U. M1.28 confirmed WinUAE screenshot (A1200 / OS 3.1 / 640x256)

The user confirmed the M1.28 GUI starts and works, with a screenshot
of **Source → Partition** after Query of `scsi.device` Unit 0.
Observed, not simulated:

- Physical identity: `UAE-IDE rafael.hdf`
- Media capacity: `1881 MiB`
- Third LISTVIEW populated with RDB names:
  `WB_1.3`, `WB_2.x`, `OS`, `GRY`
- `Selected partition`: `scsi.device / Unit 0 / OS`
- `Partition info`: `OS 600MiB DOS1(FFS) Cyl860-1990 Blk512 SPB1`
- Third recessed information row and bottom buttons visually fit
  the classic 640x256 selector without visible overlap.

The screenshot supports successful native compilation and the
tested RDB/INQUIRY/READ CAPACITY display path. It does **not** by
itself verify all M1.28 tests, particularly cancellation semantics,
Query gating before selection, failure paths, MBR interpretation,
or actual copying/imaging. No write operations have been added.
Keep those cases pending before treating M1.28 as fully validated.

## V. M1.29 — list selection persistence and partition ordinals

| ID | Test | Expected |
|---|---|---|
| GUI-81 | AmigaOS 3.1: click Device row | Selected row stays highlighted after release |
| GUI-82 | AmigaOS 3.1: click Unit / Partition / Logical | Each selected row remains highlighted until another selection |
| GUI-83 | Rebuild or resize with a selected row | Persistent highlight restored through GTLV_Selected |
| GUI-84 | RDB Query for rafael.hdf | Third LISTVIEW displays `1: WB_1.3`, `2: WB_2.x`, `3: OS`, `4: GRY` |
| GUI-85 | Select `3: OS` | Upper preview and committed endpoint use `OS`, not `3: OS` |
| GUI-86 | RDB order | Ordinals represent encountered PART-chain sequence, no alphabetical sorting |
| GUI-87 | AmigaOS 2.04 / GadTools V37 | Selected entry appears in a read-only field under list; V39 highlight cannot be assumed |
| GUI-88 | Query gating and no double-click | Existing explicit Query and Use requirements unchanged |

These tests remain pending WinUAE verification. Numbering affects
display only and does not alter the on-disk RDB, parser or source/target
identity. Extra V37 selection display may reduce visible list rows.

## W. M1.30 — reported Destination Unit click Guru #8000000B

User-confirmed M1.29 persistent GadTools LISTVIEW highlighting works,
but the following regression was reported on WinUAE A1200 / OS 3.1:

1. Choose and commit Source device.
2. Set Copy disk-to-disk operation.
3. Open Destination physical device chooser.
4. Click Unit 0.
5. System crashes with Guru \`#8000000B\` (CPU Line-F exception).

The exact corruption site is **not yet proven**. One suspicious pattern
in M1.29 was destroying and recreating GadTools LISTVIEW gadgets and
mutating their linked lists directly inside a GADGETUP message handler.
The failure happened on a Unit change, which uses that path. With
GTLV_ShowSelected enabled on V39+, GadTools keeps selection state and
may reference its existing list until all pending messages are replied.

M1.30 first drains all GT_GetIMsg messages, using GT_ReplyIMsg on
each, *without* modifying any LISTVIEW nodes or destroying gadgets.
An update flag remembers whether Mode, Device, Unit, or a Partition
Query needs a rebuild. At the end of the pump, the old gadget list
is detached, then node lists and query state are updated, then the
new GadTools context is constructed. Further queued messages
while an update is pending are replied and ignored as stale.
Physical Query remains explicit and never fires due to selection.

| ID | Regression sequence | Expected |
|---|---|---|
| GUI-89 | Source physical → Query → Use | Source committed normally |
| GUI-90 | Change operation to disk-to-disk | Main selector keeps stable source |
| GUI-91 | Destination Physical → driver → Unit 0 | No Guru/Line-F; persistent unit highlight |
| GUI-92 | Query destination then Use | Two independent endpoints, stable name/size |
| GUI-93 | Alternate Source/Destination chooser multiple times | No crash or stale gadget pointers |
| GUI-94 | Repeated driver changes in Partition mode | Gadgets rebuilt only after message drain |
| GUI-95 | Partition Query → RDB list → selection | Persistent highlight and numbered entries preserved |
| GUI-96 | OS2.04 / V37 after tests on 3.1 | No crash; version-specific list rendering unchanged |

This patch is a **hypothesis-driven fix**, not a confirmed root cause.
Compile and controlled WinUAE reproduction are required.
Do not enable real-media writes; Query remains read-only.

## X. M1.31 — Destination Query HALT3 diagnostic isolation

Additional user observation: in the exact Source -> disk-to-disk ->
Destination -> scsi.device Unit 0 path, **Unit selection completed**
and the failure appeared only on **Query**. WinUAE displayed HALT3.
This supersedes the earlier assumption that clicking Unit 0 itself
caused the crash. The previous M1.30 deferred-message patch did not
resolve it.

M1.31 is a **diagnostic and lifetime-management patch**, not a
verified cure. It adds a key separation: after draining the
GT_GetIMsg queue, destroy the old GadTools context with FreeGadgets
**before** repopulating any LISTVIEW node chain or executing a Query
that may mutate Partition labels. Removing gadgets from the Window
alone does not release references retained by the GadTools context.

Temporary flush-to-stdout trace markers:
- DOLLY M1.31 DEST Query: gadgets teardown
- DOLLY M1.31 DEST Query: device commands begin
- DOLLY M1.31 DEST Query: device commands returned
- DOLLY M1.31 DEST Query: partition scan begin
- DOLLY M1.31 DEST Query: partition scan returned
- DOLLY M1.31 DEST Query: populate partition nodes
- DOLLY M1.31 DEST Query: partition nodes ready
- DOLLY M1.31 DEST Rebuild: begin/completed

Run from AmigaShell with Stack 16384 and keep stdout visible.
If HALT3 recurs, capture the *last printed marker*, differentiating
query/driver/RDB from GadTools rendering/rebuild. Source and
Destination traces include their role. Only the explicit Query path
uses hardware requests; nothing is written to the disk.

Test: re-run the previously crashing sequence on A1200 / OS3.1 /
controlled WinUAE test HDF. Inspect that Query returns Device name
and Size and creates numbered RDB partition rows. Also test Source
alone; repeat Source -> Destination and Destination -> Source.
Do not use a physical Amiga until the HALT3 failure is resolved.

## Y. M1.32 — Partition Query rendered corrupted text after M1.31

A1200 / OS 3.1 WinUAE user test: M1.31 eliminated the observed
HALT3 crash on the Source -> disk-to-disk -> Destination -> Partition
-> scsi.device Unit 0 -> Query sequence. The final console markers
were \`DEST Query: partition nodes ready\`, \`DEST Rebuild: begin\`,
and \`DEST Rebuild: completed\`. The Device name and Size were
correct (UAE-IDE rafael.hdf / 1881 MiB), but the newly recreated
Partition LISTVIEW displayed corrupted entries (\`0:\`, garbage)
instead of \`1: WB_1.3\`, \`2: WB_2.x\`, \`3: OS\`, \`4: GRY\`.

GadTools documentation explicitly requires detaching a List by setting
GTLV_Labels to \`~0\` before mutating its List/Node structures, and
reattaching it afterward. Merely removing a Gadget from Intuition
does not substitute for that documented contract.

M1.32 avoids removing/freeing/recreating GadTools completely on a
Partition Query:
1. Drain all IDCMP messages / reply to each as in M1.31.
2. \`GT_SetGadgetAttrs(partition_list,...,GTLV_Labels,~0,...)\`.
3. Perform the already validated read-only device/partition queries.
4. Rebuild private RDB/MBR nodes and stable numbered display strings.
5. Print each raw RDB label and displayed label to AmigaShell for
   verification (only in this diagnostic version).
6. \`GT_SetGadgetAttrs(...,GTLV_Labels,&partitions,
   GTLV_Selected,~0,...)\`, refresh independent device/size,
   partition info, preview, and Query-gated Use button.

Query does NOT rebuild the Gadget context. Mode/Driver/Unit changes
still use the M1.31 delayed rebuild path pending broader refactor.

| ID | WinUAE verification | Expected |
|---|---|---|
| GUI-97 | Repeat Source then Destination Partition Query | No HALT3 or Guru |
| GUI-98 | Shell debug \`DOLLY M1.32 DEST PART ...\` lines | Correct raw volume names and numbered display strings |
| GUI-99 | Third LISTVIEW immediately after Query | \`1: WB_1.3\`, \`2: WB_2.x\`, \`3: OS\`, \`4: GRY\` |
| GUI-100 | Select a third-list partition | Persistent highlight; Selected partition and Partition info show original name |
| GUI-101 | Repeated Query without reopening picker | Stable list, no ghost entries or dangling labels |
| GUI-102 | Query error / no partition table | Empty list, Use selection disabled |
| GUI-103 | Destination Physical Query | Existing name/size behavior unchanged |

If the Shell shows correct \`name=\`/\`label=\` but LISTVIEW is still
corrupt, next isolate the GadTools attribute update / gadget context.
If Shell labels are wrong, investigate parser/map overwrites before
further GUI work. Cross-compilation and WinUAE testing are pending;
do not use against real hardware yet.

## Z. M1.33 — Query returns from RDB scan then hangs before first row trace

User's last observed M1.32 message: \`DEST Query: partition scan returned\`.
No subsequent \`DOLLY M1.32 DEST PART ...\` lines appeared.
This narrows the fault to reading \`partition_map.count\` and preparing
the third LISTVIEW's node/label storage, although hidden memory
corruption during the completed scan remains possible. The old fix
which rebuilt all GadTools after Query caused display garbage; the
in-place-label update of M1.32 could still be touching LISTVIEW data
by reusing the same \`struct List\` and \`struct Node\` memory.

M1.33 is an **unverified isolation patch**, not a confirmed fix:
- Keep two complete, persistent banks of partition List, Nodes, and
  display strings within each Source/Destination picker.
- Build only the **inactive** bank; never mutate the active
  LISTVIEW-owned nodes or their text, even across repeated Queries.
- Limit to DOLLY_PART_MAX=16; fail closed on an inconsistent count.
- Build numbered display strings explicitly without \`sprintf\`
  or shared source/destination buffers.
- After \`GTLV_Labels=~0\`, attach the fully prepared bank by its
  independent List pointer; only then allow selection again.
- Emit flushed Shell trace of the count/bank, and before/after
  each individual AddTail call. If the emulator crashes, the exact
  last line will show whether failure happens at a specific node.
- Original names and descriptive fields in partition_map remain
  unchanged. No read/write behavior changes.

| ID | Regression sequence | Expected |
|---|---|---|
| GUI-104 | Destination Partition Query on A1200 OS3.1 | No HALT3; RDB labels correctly numbered |
| GUI-105 | Before/after per-row trace | Four rows linked, names WB_1.3 / WB_2.x / OS / GRY |
| GUI-106 | Repeat Query | Alternate banks safely; no stale or corrupted strings |
| GUI-107 | Source then Destination; reverse | Independent lists and queries; no cross-window corruption |
| GUI-108 | Query no map/unsupported | Empty third LISTVIEW, Use disabled |
| GUI-109 | Use selection after choosing RDB partition | Stable original partition identity, not list number |

Only test with a disposable WinUAE image. Do not use real media until
the crash is resolved. RTG and memory profiling remain deferred.

## AA. M1.33 verified in WinUAE — Destination Partition Query

User confirmed screenshot on the previously crashing scenario:
Source selected, disk-to-disk operation, Destination -> Partition ->
scsi.device Unit 0 -> Query.

**Observed PASS for this one scenario:**
- No HALT3 or Guru.
- Third LISTVIEW displays `1: WB_1.3`, `2: WB_2.x`,
  `3: OS`, `4: GRY` correctly.
- Device name: `UAE-IDE rafael.hdf`; Size: `1881 MiB`.
- Partition info prompts "Select a partition..." until one is chosen.
- Use selection appears disabled before partition choice.
- Console reports "Query: reattach partition labels" and
  "Query: in-place update completed".

This confirms the M1.33 double-buffered list mechanism fixes the
previously observed crash/corrupted-display scenario **in this
WinUAE configuration**. Not yet tested: repeated Query, selecting
another driver/unit after Query, alternating chooser roles repeatedly,
or invalid/empty RDB. These remain pending before removing
diagnostic traces or declaring the whole picker robust.

## AB. M1.33 repeat interaction verification; M1.34 diagnostics cleanup

The user confirmed that multiple cycles of selecting items, closing
the chooser and reopening it work correctly after the M1.33
double-buffered partition LISTVIEW fix. This extends the previously
verified single Destination Query path with repeated interaction
and window lifecycle behavior. The exact repetitions and failure
cases were not separately itemized, so keep untested media formats,
unsupported device I/O and AmigaOS 2.04 as pending.

M1.34 removes only temporary M1.31-M1.33 AmigaShell debug trace
prints, including per-node and per-Query messages. The live
double-buffered List/Node/text banks, explicit Query gating,
GTLV_Labels detach/reattach, persistent selection, numbered RDB
names, and separate Device name/Size boxes remain unchanged.

| ID | Test | Status |
|---|---|---|
| GUI-104 | Destination Query displays correct numbered RDB partitions | PASS on user WinUAE A1200/OS3.1 |
| GUI-106 | Repeated selecting/opening/closing chooser | PASS per user report |
| GUI-110 | Rebuild without M1.31–M1.33 shell trace | Pending M1.34 build |
| GUI-111 | Repeat Destination Query after trace removal | Pending WinUAE |
| GUI-112 | Reopen/close Source and Destination repeatedly in M1.34 | Pending WinUAE |
| GUI-113 | Invalid RDB, MBR, no media, OS2.04 | Not yet verified |

This is a trace-only cleanup and is not yet cross-compiled or
tested in WinUAE. Continue controlled test-HDF use; RTG and RAM
profiling remain deferred.


## AC. M1.35 contextual Cycle filtering and Disk Vet

| ID | Test | Expected |
|---|---|---|
| GUI-114 | Main at 640x256 | Operation Cycle on top, followed by Source and Destination; no overlap |
| GUI-115 | Change Operation with Source/Target chosen | Previous values retained but incompatible Start disabled with explanatory status |
| GUI-116 | Restore Image -> Source chooser | Cycle shows File only; no Physical/Logical/Partition choice |
| GUI-117 | Restore Image -> Destination after File Source | Cycle shows Physical and Partition only; correctly maps positions 0/1 to mode enum 0/2 |
| GUI-118 | Create Image -> Destination | Cycle contains File only (mode enum 3), no index confusion |
| GUI-119 | Copy -> Source Physical -> Destination | Physical only in Normal; Partition hidden |
| GUI-120 | Copy -> Source Partition -> Destination | Partition only in Normal; physical hidden |
| GUI-121 | Copy -> Source Logical -> Destination | Logical only in Normal |
| GUI-122 | Copy -> Source File -> Destination | File only; identical paths rejected, distinct files allowed by demo validator |
| GUI-123 | Disk Vet | Visible sixth Operation; Source chooser offered, Destination disabled; Start never enabled |
| GUI-124 | Scan/Inventory | Destination disabled, proper Source page subsets |
| GUI-125 | Change Source while choosing Destination | Operation control cannot change while picker is open |
| GUI-126 | Picker Mode selection | Correct interpretation of filtered GTCY_Active/IDCMP Code |
| GUI-127 | Source/Destination Query and RDB | Explicit Query gate and double-buffered numbered PART list unaffected |
| GUI-128 | Portable endpoint test with -Werror | Mode mask and validation assertions pass |
| GUI-129 | OS2.04 / MC68000 | Dynamic NULL-terminated GTCY_Labels works on GadTools V37 |
| GUI-130 | Conversion/Doctor | File-to-File Copy is demo only; Disk Vet never invokes a repair backend |

All M1.35 rows remain **pending** until the feature branch is built
and tested. Berserk is only a future policy parameter, with no
active UI toggle or authorization to write. No RTG/memory profiling
in this stage.

## AD. M1.36 — main-window visual polish

| ID | 640×256 WinUAE regression | Expected |
|---|---|---|
| GUI-131 | Open main window | Operation, Info, Source, Destination in order; no decorative heading |
| GUI-132 | Sheep Counter / Sheep Inventory / Quit | Equal widths and even gaps, no overlap |
| GUI-133 | Sheep Inventory | Always disabled; never launches a window or operation |
| GUI-134 | Main titles, buttons, Operation Cycle | No visible "demo" or "preview" suffixes; title "Dolly" |
| GUI-135 | Main Progress at idle | `0 / 192 (0%)`, recessed field and Progress label |
| GUI-136 | Run demo engine (no actual I/O) | Percentage reaches 100% without overflowing its field |
| GUI-137 | Lower status area | Compact gap beneath Progress; recessed status remains legible |
| GUI-138 | Resize and relayout | No overlap/clipping at 640×256 and on proportional screens |
| GUI-139 | Open Source / Destination and Query RDB | Previously verified M1.33/M1.35 picker behavior unchanged |

M1.36 is a layout/label-only update. Functional engine still only
simulates progress and is not authorized to copy or write real media.
Cross-compile and WinUAE testing are pending.

## AE. M1.37 — compact main and Create Image output format

| ID | WinUAE scenario | Expected |
|---|---|---|
| GUI-140 | Open Dolly main on 640×256 | Shorter initial window (195px outer), status fully visible |
| GUI-141 | Middle button | Blank caption, still disabled; Inventory remains in Operation Cycle |
| GUI-142 | Resize main (minimum and larger) | No overlap or chopped status/progress at lower bound |
| GUI-143 | Create Image -> Destination File -> HDF | Recessed target path ends in ` (HDF)` |
| GUI-144 | Change Destination output format -> RAW/ADF/IFF DOLY | Corresponding suffix updated after committing selector |
| GUI-145 | Destination name very long | Truncation preserves complete trailing format suffix |
| GUI-146 | Copy File->File and Restore Image | No appended output format suffix |
| GUI-147 | Reopen Destination picker | Output image_format persisted; file path not modified |
| GUI-148 | Source and Destination Partition Query | M1.33 double-buffered LISTVIEW behavior unchanged |

M1.37 source changes are not yet cross-compiled or exercised in WinUAE.

## AF. M1.38 — Stop / Quit alongside compact Progress

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-149 | Main upper action row | Sheep Counter, blank disabled, blank disabled; equal widths |
| GUI-150 | Main Progress row | Only percentage in recessed field (0%, 50%, 100%); no block count or parentheses |
| GUI-151 | Stop / Quit | Same line as Progress; Stop immediately left of Quit; identical widths |
| GUI-152 | Stop while idle and while operation running | Always disabled; no cancellation handler or fake behavior |
| GUI-153 | Quit on Progress row | Exits through the original Quit path |
| GUI-154 | Minimum-width resize | Buttons shrink from 90px to 70px together, no overlap with 66px percentage well |
| GUI-155 | Sheep Counter | Full block count plus separate percent indicator still shown |
| GUI-156 | Create Image destination | Filename (HDF/RAW/ADF/IFF DOLY) suffix retained |
| GUI-157 | Main window size | Initial 195px outer, recessed status fully visible, no new empty row |
| GUI-158 | Existing Source / Destination Query | No regression from main-window-only change |

Not yet built or tested in AmigaOS/WinUAE. No operation is currently
classified as safely stoppable; capability and actual cancellation
semantics must be specified and tested in a later task.

## AG. M1.39 — shorter footer and build identity

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-159 | Main window at default 600px width | Outer height 190px; reduced empty space below recessed status |
| GUI-160 | Progress row | Centered `Dolly build M1.39` between percentage well and Stop |
| GUI-161 | Resize to minimum usable width | Build label disappears when it cannot fully fit; no overlap with Stop or percentage |
| GUI-162 | Stop and Quit | Both remain functional/disabled as in M1.38; same alignment |
| GUI-163 | Main window on OS2.04 / alternate font | Status bezel remains visible, no clipped gadgets; check actual border metrics |

M1.39 is not yet cross-compiled or WinUAE-tested.

## AH. M1.40 — measured minimum and compact version

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-164 | Open main after M1.39 failure | No `WINDOW FAIL: measured minimum does not fit` at default 640×256/Topaz configuration |
| GUI-165 | Progress row at default and measured minimum width | Full `Dolly M1.40` label visible between percent well and Stop |
| GUI-166 | Short main window | Measured min-height matches bottommost status/control geometry and real borders |
| GUI-167 | Resize and reopen main | No overlap or clipped status box; Quit and disabled Stop unchanged |

M1.40 has not been cross-compiled or checked in WinUAE.

## AI. M1.41 — horizontal-only resize and system SHINEPEN

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-168 | Resize Dolly vertically | Height remains unchanged |
| GUI-169 | Resize Dolly horizontally | Width changes, controls remain correctly aligned |
| GUI-170 | Build label | `Dolly M1.41` uses active screen SHINEPEN |
| GUI-171 | Open main | No measured-minimum failure; unchanged compact footer |

Not yet cross-compiled or tested in WinUAE.

## AJ. M1.42 — registered versus verified physical media

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-172 | Open Physical Source on AmigaOS 2.04 | Physical drivers and units come only from DOS startup registrations; no fabricated unit numbers |
| GUI-173 | Select a registered unit before Query | Preview reads `Registered; press Query`; capacity `Not verified`; Use selection disabled |
| GUI-174 | Query a device with verified TD_GETGEOMETRY or READ CAPACITY(10) | Reported capacity displayed; physical Use selection enabled |
| GUI-175 | Query a driver with no media, failing geometry or INQUIRY only | Unknown capacity; physical Use selection remains disabled |
| GUI-176 | Query a unit in Partition mode | Partition scan requires verified capacity; existing double-bank list remains safe |
| GUI-177 | Main window | M1.41 layout unchanged except version text `Dolly M1.42` |

M1.42 code not cross-compiled or WinUAE-tested; no real media writes enabled.

## AK. M1.43 — validated device geometry and query provenance

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-178 | Physical Source, valid TD_GETGEOMETRY | Capacity accepted only with power-of-two sector size 128–65536 bytes |
| GUI-179 | Unsupported/invalid TD geometry, supported SCSI READ CAPACITY(10) | SCSI probe supplies verified capacity and size |
| GUI-180 | Invalid sector size in both queries | Unknown capacity and Use selection disabled |
| GUI-181 | Geometry available but no SCSI INQUIRY name | Device info shows sector size and whether TD geometry or SCSI capacity supplied it |
| GUI-182 | Main Dolly window | Same approved M1.41 geometry, `Dolly M1.43` SHINEPEN label |

Source changes not yet cross-compiled or tested in WinUAE. Device selection is not permission to perform writes.

## AL. M1.44 — Physical device information (read-only)

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-183 | Source picker / Physical, before Query | Three rows under LISTVIEW; Geometry says `Press Query` |
| GUI-184 | Query device with TD_GETGEOMETRY | Device name and size retained; Geometry says `N B/sector \| TD_GETGEOMETRY` |
| GUI-185 | Query device with READ CAPACITY(10) fallback | Geometry says `N B/sector \| SCSI READ CAPACITY(10)` |
| GUI-186 | Query fails or no capacity | Geometry says `Unavailable`; Use selection disabled |
| GUI-187 | Device/unit changes and picker reopen | Previous geometry not reused for a different unit; Query required again |
| GUI-188 | Partition mode and 640x256 screen | Partition details remain on third row and no overlap with LISTVIEW or buttons |
| GUI-189 | Main window | M1.41 layout remains frozen; SHINEPEN version reads `Dolly M1.44` |

Native cross-build and WinUAE tests remain pending; this is not media-write certification.

## AM. M1.45 — first native Sheep Inventory view

| ID | WinUAE check | Expected |
|---|---|---|
| GUI-190 | Select Inventory operation, Source Physical, press Start | Opens separate Dolly - Sheep Inventory; no synthetic Sheep Counter operation |
| GUI-191 | Query supported 512 B/sector physical unit | Shows queried device name, capacity and source of geometry |
| GUI-192 | Drive with valid RDB/MBR | Read-only scan lists up to 16 partition labels and reports declared partition count |
| GUI-193 | Unsupported/failed RDB/MBR or sector size !=512 | Explicit unavailable/unsupported message, not a fabricated partition list |
| GUI-194 | Logical/File source selected | Explicit not-yet-supported Inventory notice; never opens guessed physical unit |
| GUI-195 | Close Inventory and reopen | Fresh Query; no stale list nodes or geometry |
| GUI-196 | Quit while Inventory open | Inventory gadgets/window released before closing libraries |
| GUI-197 | Main window and picker | Approved M1.41 layout unchanged; only build label M1.45 |

This code has not been cross-compiled or tested in WinUAE. Device reads occur only after pressing Inventory Start and are limited to the existing read-only query/partition scanner. Partition labels and RDB fields are *declared metadata*, not independently verified filesystem state.

### M1.45 regression — empty partition list (WinUAE screenshot)

A verified device (`scsi.device` unit 0, UAE-IDE image) returned `RDB: 0 partition(s) declared`, but the initial Inventory LISTVIEW showed repeated `0:` and corrupted text. The fix skips LISTVIEW creation when the validated partition count is zero and paints `No partitions to display` as ordinary text. A nonempty LISTVIEW is now explicitly noninteractive and retains its view-owned nodes for its entire lifetime. Validate both zero-partition and actual multipartition disk images in WinUAE; the latter remains unverified. No source or disk data is changed.

### M1.45 regression — confirmed four-partition image, incorrect zero output

User confirmed the tested UAE-IDE image has four partitions. Initial screenshot displayed four list entries as `0:` with damaged captions and `RDB: 0 partition(s) declared`. Thus the earlier empty-list hypothesis was wrong and did not explain the defect. The native inventory renderer used `%u` for its list indices/count while existing working Amiga GUI code uses `%lu` with `(unsigned long)`; the affected formatting is now consistent with the working code. Added diagnostic `INVENTORY:` lines for scanner result/count and each raw partition label in the Amiga Shell. RDB/MBR default partition labels also use `%lu`. Re-test the same four-partition disk: require count 4 and all four correct labels before closing the incident. If raw diagnostic labels are also corrupt, the partition parser needs a separate correction; never misreport the medium as empty.

## AN. M1.46 — Sheep Inventory: selected partition details

| ID | WinUAE test | Expected |
|---|---|---|
| GUI-198 | Open four-partition UAE-IDE HDF, Inventory | Four correctly named entries; initial prompt on right |
| GUI-199 | Click each partition | Selection remains active; right panel updates without a new disk read |
| GUI-200 | RDB DOSx/PFS/SFS partition | Hex and escaped ASCII DosType, declared filesystem name, Bootable, Automount, signed BootPri, MaxTransfer and Mask |
| GUI-201 | RDB short DosEnvec | Missing optional values displayed as unavailable, not zero |
| GUI-202 | RDB cylinders and block size | Low/high cylinder, filesystem block bytes, SectorPerBlock and partition size (bytes below 1 MiB, otherwise whole MiB) |
| GUI-203 | MBR image | Declared type code/name, role, active flag, LBA range, sector count and size |
| GUI-204 | Refresh/close/reopen | Selected row and text stay consistent; no stale gadgets |
| GUI-205 | Main Dolly window | M1.41 approved layout unchanged, SHINEPEN build marker Dolly M1.46 |
| GUI-206 | Resize Sheep Inventory with a selected MBR or RDB row | Driver, Unit, Geometry, Disk Name and the partition count stay at the top. The list scales, the selection stays visible, and the buttons follow the bottom edge. |

M1.46 Release cross-build passed with `-m68000 -noixemul`.
`m68k-amigaos-objdump -f` identified `dolly-two-window-demo` as Amiga
format with architecture `m68k:68000`. WinUAE checks GUI-198 through
GUI-205 are pending.
The ten native console test executables and `dolly-native-smoke --selftest`
passed in vamos with CPU 68000 and 512 KiB of emulated RAM. That run used
vamos library emulation and did not exercise GadTools or real Chip RAM.
In FS-UAE 3.1.66, Dolly opened on an emulated A600 with Kickstart 2.05
37.350, Workbench 2.05 and GadTools 37.110. Inventory opened for the
logical source DF0:, displayed the unsupported-source status, closed
and reopened, and Dolly exited with code 0. A separate read-only UAE-IDE
HDF prepared with AmiPart exposed DH0–DH3 on `scsi.device` unit 0. All
four rows displayed the expected DosType, BootPri, MaxTransfer, Mask,
block geometry and cylinder range. In a second read-only HDF, the DH3
PART checksum covered a shortened DosEnvec ending at index 12. The
four optional fields showed `unavailable`; cylinder range and size
were still visible. Closing and reopening Inventory restored all four
rows with no selection. On a separate read-only MBR image at
`scsi.device` unit 1, Inventory showed one partition of type `0x83`
and size 4 MiB, marked the RDB-only fields as absent from MBR, and
disabled **Backup RDB**. The AmigaOS 2.04 run remains pending.
The view uses a saved, bounded read-only scan result; selection never sends
additional device I/O and all displayed RDB fields are *declared* metadata,
not an assertion about active filesystem/controller configuration.

### Sheep Inventory RDB backup

On the same FS-UAE A600 with GadTools 37.110, **Backup RDB** opened the
ASL save requester and saved to the selected `DOLLYTEST:` drawer. The
2,560-byte output matched the source HDF byte for byte across all five
reserved sectors. Cancelling the requester returned to Inventory.
Selecting the existing destination again was refused and preserved the
first backup.
A host run of the native scanner with mock Exec/DOS and UBSan also
covered a short write, an RDB reserved area overlapping a partition,
and an MBR-only disk. The backup call failed in all three cases.

After adding MBR export, selecting the read-only MBR disk in FS-UAE
showed **Backup MBR** and disabled **Restore MBR**. The file saved via
ASL was exactly 512 bytes and matched LBA 0 byte for byte. Switching
to the RDB disk showed **Backup RDB** and disabled **Restore RDB**.
The resulting RDB backup still matched all five reserved sectors.

The expanded Inventory view was checked in FS-UAE on AmigaOS 2.05.
The MBR fixture showed active primary `0x07`, extended `0x0F`, unknown
`0x99` and FAT32 LBA `0x0C` entries with their LBA ranges. The extended
container was listed without traversing its contents and disabled backup.
The RDB fixture showed `DOS\7` as FastFileSystem (long filenames),
`DOS\1` as FastFileSystem and `PFS\3` as ProfessionalFileSystem 3.
The shortened DosEnvec still showed the DosType as unavailable.

On the same A600, shrinking the MBR and RDB Inventory windows kept the
selected row, header and details visible. The list grew again when the
RDB window was enlarged. The footer buttons tracked the window edge,
and the partition count appeared only in the list heading.

The reordered header was verified in FS-UAE at the actual Inventory
limits of 504×230 and 640×256 pixels, using the same selected MBR row.
SCSI MODE SENSE reported 130 cylinders, 4 heads and 63 sectors per
track. Capacity remained 32,760 sectors of 512 bytes in both views.

The experimental right-hand LISTVIEW was tested with GadTools 37.110
in FS-UAE. Right Amiga+C copied the selected MBR detail to the actual
guest clipboard. Reading unit 0 returned a valid FTXT/CHRS clip with
`Last LBA: 24287`. At minimum window width, a clipped role row still
returned the full text `Role: Extended | Active: No`.

With click-to-copy enabled, clicking the geometry row, the extended
partition and its Last LBA detail separately put each full row on
clipboard unit 0 without a keyboard shortcut. The partition and
details lists aligned above the shared recessed status box.

### NSD writes across 4 GiB

A MC68000 test program called Dolly's destination adapter under FS-UAE
on an emulated A600 with Kickstart 37.350 and a disposable sparse 5 GiB
hardfile on `uaehf.device` unit 0. The driver advertised the required
commands. A verified 1 KiB write at 0xfffffe00 and a verified 512-byte
write at 0x100000200 both succeeded. On the host, the three written
sectors matched their patterns and the first two sectors retained their
canary. This exercises the actual driver and Dolly's block write adapter.
The complete Copy and Restore workflows across the boundary still need
end-to-end coverage.

The classic image size check was also exercised through the FS-UAE
directory filesystem on the same A600 configuration. It reported a
1,024-byte RAW file correctly and rejected both a sparse 5 GiB file
and a sparse 4 GiB + 1,024-byte file. The latter checks that a large
file does not pass as a small image after a 32-bit wrap.

### Create Image warning and split output

MC68000 build and all 36 host tests passed. An A600 / Kickstart 37.350
FS-UAE probe used the native staging and write code, with a synthetic
source, mocked volume information and warning decisions, and reduced
split thresholds. It published a three-part RAW image and a three-part
compressed DOLY image. The host verified RAW bytes, DOLY FORM lengths and
starting LBAs, and checked that cancellation left no output. The probe
also verified that a small RAW image still publishes as a single file
without a size or free-space warning. The full-size GUI pathway remains
to be tested.

### Restore split IFF DOLY

The host manifest test covers a logical image larger than 4 GiB, the final
partial part, incorrect order and geometry, and malformed manifests.
A POSIX shim for AmigaDOS exercised the native source adapter with a
three-part compressed IFF DOLY fixture. It verified cross-part reads,
missing-part rejection, handle cleanup, and a damaged chunk stopping
validation before the first destination write callback. All 36 host
tests passed. The MC68000 build and end-to-end GUI Restore in UAE remain
unverified because the build environment ran out of disk space.

### Numbered multipart files

The portable test covers MSET v2 writes and reads for each part, including
the short final part, and rejection of damaged numbering metadata.
The AmigaDOS shim exercised opening part 3/3, redirecting to the matching
first part, resolving a missing part on another volume and reading bytes
across part boundaries. Native MC68000 compilation and the ASL/requester
flow in UAE remain unverified due to the full build environment.

### Native build and multipart read follow-up

After reclaiming workspace space, the full `dolly-two-window-demo`
MC68000 build succeeded. The rebuilt application opened its main window
in FS-UAE 3.1.66 on an A600 with Kickstart 37.350. A separate native
probe ran the current Restore source adapter against the real AmigaDOS
folder filesystem, with test substitutes for the dialog and ASL choices.
It opened part 3/3, switched to the first part, identified missing part
2/3, opened it from an alternate drawer and verified bytes across a
part boundary. The probe exited with status 0. Host CTest passed 36/36.
The interactive ASL presentation, the write-retry path in UAE and a
complete destructive Restore to disposable media remain to be tested.

### Multipart Restore to a disposable HDF

A native MC68000 probe in FS-UAE 3.1.66 (A600, Kickstart 37.350)
used the current multipart source adapter, Restore transfer engine,
and verified device writer. ASL and confirmation choices were stubbed.
A corrupt chunk checksum in part 2/3 blocked the source with zero
write callbacks. A clean three-part set with part 2/3 in an alternate
drawer was validated and restored through `uaehf.device` unit 0 in
three 1,536-byte batches with readback verification. The host compared
the entire destination HDF byte for byte, including both guard regions.
The control HDF was unchanged. This exercises the native transfer
path independently of the main window's interactive preflight.

### Native retry of a split output part

An MC68000 probe under FS-UAE on A600 / Kickstart 37.350 ran the
current image staging and retry code with real AmigaDOS files and
two mounted directory volumes. Test-only limits forced a three-part
image from nine synthetic sectors. The probe injected a short write
during part 2/3, selected the second volume through a scripted chooser,
and observed a rewind to the first sector of the failed part. Part 1/3
was published on the original volume, and parts 2/3 and 3/3 on the
replacement volume. The host verified all payload bytes, part numbering,
set ID, manifest and chunk checksums, and absence of incomplete files.
The chooser and warning actions were scripted. Host CTest passed 36/36,
including regression coverage for operation-progress rewind.

### Desktop Fast compression above 4 GiB

On Linux, `dolly-image split --fast` converted a sparse 4 GiB + 1 KiB
RAW fixture into 17 IFF DOLY files occupying approximately 72 MiB.
`dolly-image verify` decoded and verified all 4,294,968,320 bytes.
An independent PackBits decoder verified distinct sector patterns at
offset zero, immediately before 4 GiB, and in the two sectors above it.
A separate three-part Fast set passed the native Restore source probe
in FS-UAE on A600 / Kickstart 37.350. Host CTest passed 37/37.

The Linux large-image fixture also completed the reverse operation
with `dolly-image join --sparse`. The rebuilt RAW had the full logical
size of 4,294,968,320 bytes and used 36 KiB of disk blocks on the test
filesystem. A byte-for-byte `cmp` found no differences across the
entire image. The CLI regression covers leading, internal and trailing
zero runs as well as a wholly zero image. Host CTest passed 37/37.

### Restore source preview

The MC68000 build passed with the image-header preview. A native probe
under FS-UAE (A600, Kickstart 37.350) reported part 16/17 and the full
decoded size of 4,294,968,320 bytes for a real numbered file. It rejected
the same file with a damaged manifest CRC and retained the HDF size hint
for a flat file. The probe verified the text produced by the preview
function. Interactive ASL selection was not part of that probe.

### Incremental IFF DOLY Restore validation

The MC68000 build passed. A native FS-UAE probe on A600 / Kickstart
37.350 opened a three-part set with valid metadata and a corrupt data
chunk in part 2/3. The Restore validation advanced by 1,536 bytes,
identified part 2/3 and stopped with zero write callbacks. The host
compared the writable target HDF against its original bytes and found
no changes. A clean three-part set, with part 2/3 selected from an
alternate drawer, passed validation and restored all 4,608 bytes in
three verified batches.

The portable regression asserts that opening a lazy reader reads one
prefix and that sequential verification of each test chunk takes at
most three backend reads, including a possible IFF padding read. It also
checks seeking back across the sparse index boundary. Pause, Stop and
the two graphical progress displays were not driven by the native probe.

### Read-only FFS Clone and resize analysis

The host regression uses a synthetic partition starting above 4 GiB
with a bitmap-extension block. It checks the exact used and out-of-target
counts and rejects invalid bitmap state, duplicate metadata, a repeating
extension, metadata marked free and a page with a bad checksum.

An independent formatted DOS/3 fixture created with AmiPart started at
byte 4,644,864,000 in a sparse 5 GiB HDF. Dolly and AmiPart both reported
37 of 131,040 blocks occupied and block 65,554 as the highest occupied
block. For a target of 49,152 blocks, Dolly reported 35 occupied blocks
outside the target and an in-place floor of 65,555 blocks. This floor
retains current metadata positions. Host CTest passed 38/38.
The MC68000 build passed with the analysis entry point. The interactive
requester and source inhibit flow were not exercised in UAE for this feature.

The growth planner is covered for a free root slot, a slot occupied by
data, a slot occupied by metadata and a root in the added range. Tests
also cover bitmap extension growth. In a comparison on a disposable
copy of the DOS/3 fixture, AmiPart grew the partition from 131,040 to
262,080 filesystem blocks. Its new root pointer (131,040) and bitmap
chain (33 to 65 pages, one extension throughout) matched Dolly's plan.
AmiPart left the bitmap marked for revalidation, and this comparison
only inspected its resulting metadata placement.

The read-only bitmap placement plan assigns the 32 new pages of the
same DOS/3 fixture to blocks 131,041 through 131,072, skipping the planned
root at 131,040. The host regression also checks page and extension
ordering and rejection of a tail with only one free block when a new
bitmap page and extension are both required.

The FFS bitmap generator passed tests for a moved root in the source
or in the added area, preservation of the old root and other allocations,
new metadata reservations, final-page masks, in-place buffer reuse,
and rejection of a damaged input checksum. The read-only preview also
succeeded on the formatted DOS/3 fixture above 4 GiB.

### Smart Copy write path

Host regressions validate the exact bitmap ranges for DOS/0 and DOS/3
and exercise the shared copy engine with a hole between write batches.
The engine rejects overlapping plan entries before issuing a write.

A host-side write and readback on a disposable HDF used the formatted
DOS/3 fixture above 4 GiB. An independent bitmap walk verified all
37 copied blocks byte for byte. All 131,003 free blocks retained their
initial pattern, and the destination RDB bytes stayed identical.
Native driver write, mount handling and the Warning Review flow still
require an interactive WinUAE test.

### Post-copy name conflicts

Host regression checks PART and OFS/FFS root names, checksum validation,
name mismatch and unformatted metadata. A disposable DOS/3 HDF with a
partition above 4 GiB exercised both updates. `DH0 → DH0_0` and
`DollyFFS → Copy_of_DollyFFS` reread with valid checksums, and bytes
outside the name and checksum fields remained unchanged. WinUAE
verification still needs to exercise both independent choices and the
subsequent remount with both disks attached.


The user confirmed Smart Copy in WinUAE on the previously tested
system HDFs. After disconnecting the source HDF, AmigaOS booted
successfully from the copied target. The subsequent fix for empty DOS
nodes awaiting shutdown still needs a WinUAE regression run.

### Cancel during transfer preparation

In FS-UAE on an A600 configuration, the preceding published build
opened Warning Review for a sector copy from a formatted UAE-IDE unit
to a blank unit. After approval of destination I/O settings and one
source inhibit, Cancel returned to the main window with
`Transfer cancelled.` and Start enabled. Both HDF checksums matched
the pre-test state. The reported WinUAE freeze did not reproduce in
this narrower test.

The updated path restores focus, displays the cancellation phase
before cleanup, and suppresses DOS media requesters during uninhibit.
WinUAE still needs a regression run that cancels the same warning
as the original report, including the late Driver access warning if it
appears after the destination handle opens.

### Volume label write after Smart Copy

The host Exec mock exercises the post-copy read/modify/write path for a
volume root block at an offset within a destination partition. It checks
the destination LBA, CMD_WRITE, CMD_UPDATE, CMD_READ and the new label
with its checksum. The native MC68000 target builds with reduced
requester stack use and distinct status messages for metadata
validation, the verified write, and source reactivation. Reproducing
the reported WinUAE HALT3 at the original confirmation is still open.


### Post-copy completion and retained destination handler

A WinUAE report showed Dolly stuck with Copying sectors at 99% after
volume label confirmation while the rest of the system remained
responsive. The test build now redraws 100% before the metadata review,
shows each finalization phase, and avoids retrying synchronous ACTION_DIE
on an inhibited destination handler after writing. Dormant DOS nodes
can still be removed. If a live handler remains, the status reports that
a reboot is needed to remount it. The original WinUAE case still needs
verification.

WinUAE confirmed successful Smart Copy finalization and an accessible
`Copy_of_Test` destination volume on `scsi.device` unit 1 after reboot
with RDB mode enabled. The readback with RDB mode disabled contained
an additional 256 KiB virtual RDB, followed by the original HDF
byte for byte.
