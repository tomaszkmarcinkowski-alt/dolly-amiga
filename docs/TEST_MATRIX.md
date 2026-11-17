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
