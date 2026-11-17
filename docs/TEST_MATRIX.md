# Dolly — staged testing matrix

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
