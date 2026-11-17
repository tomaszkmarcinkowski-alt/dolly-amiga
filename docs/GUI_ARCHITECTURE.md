# Dolly GUI architecture — MVP GadTools only; future optional backends

**Status 2026-11-17:** architecture and portable layout/notification components are in the repository. There is **no native GadTools window, ReAction/MUI module loader, verified 68000 binary or WinUAE run** yet.

## GUI backends

| Backend | Delivery | Requirement |
|---|---|---|
| GadTools | **Only MVP GUI**, built in | AmigaOS 2.04+, MC68000 |
| ReAction | **Post-MVP optional external module** | Developed and distributed later, only when its classes/libraries are installed |
| MUI | **Post-MVP, later than the core GUI**, independent optional module | Only if installed, never mandatory |

**MVP freeze (2026-11-17):** No ReAction/MUI implementation, runtime loading, compatibility testing or packaging is a condition for the MVP release. Their design contract may be retained to avoid future architectural coupling, but their delivery is an independent **post-MVP** milestone. All screens, dialogs, warnings and Sheep Inventory in MVP use GadTools. The standard executable has no link-time dependency on ReAction or MUI. Resource files and future plug-ins use the installation root `DOLLY:`; see [installation layout](INSTALLATION.md).

The versioned toolkit-neutral contract lives in `include/dolly_gui_backend.h`. It defines a core-owned host interface and a backend table for events/main window/notice/window relayout. **It is a design contract, not an implemented dynamic loader**. We must verify actual AmigaOS shared-library/LoadSeg ABI, compiler calling conventions, symbol lifetime and packaging before describing modules as loadable plugins. Backend failure must show a brief message and revert to built-in GadTools, never fail to launch Dolly.

## Sheep Counter window placement (M1.5)

**Default:** open on the existing public/system screen (Workbench) using
Intuition/GadTools styling and the screen's own pens/font. At launch,
the window is maximized to the usable screen area **below** the
system screen title bar, without covering that bar:

- Left = 0, Top = `Screen.BarHeight`
- Outer Width = `Screen.Width`
- Outer Height = `Screen.Height - Screen.BarHeight`

These are **outer window dimensions**, including Dolly's own title bar,
borders and close gadget. The top system title bar remains visible.
The screen size is queried at runtime; do not hard-code a window size
of 640x256 or force a new screen.

Future optional preference: open Sheep Counter on a separate dedicated
screen, with a return to default public-screen mode. Do not implement
the separate-screen option as a requirement for the current milestone.
Use one model for block progress and a simple ASCII `*` marker until
the final red/white Dolly art and animations are developed.

The M1.5 prototype `dolly-sheep-window` uses a public Intuition window
and direct RastPort drawing, not yet full GadTools gadgets. It has
synthetic progress and **no physical disk reads or writes**. Fixed
20x6 grid rendering is an experimental 640x256 baseline; adaptable
layout/font measurements and refresh handling remain open. It should
be compiled and run on MC68000 / 512 KiB Chip / Kickstart 2.04 before
claiming this UI milestone complete.

## Screen resolution and font policy

Mandatory **640x256, topaz.font/8** baseline for every built-in GadTools window and warning, considering outer dimensions (title, gadgets, borders, buttons). Prefer practical 640x200 compatibility. Existing `dolly_gui_layout` is a *portable geometry preflight*, not a real pixel-precise GadTools renderer.

Font selection priority:

1. Explicitly enabled **Dolly custom font**, if available and its measured content fits.
2. Actual current **system Screen Font**, when the user has not selected a custom font or custom font does not fit.
3. **topaz.font/8 fallback** if the screen or custom font would clip a view; use a compact, paginated layout if needed.

The actual font of a public screen's title/menu is owned by Intuition and **cannot be changed by Dolly**. Use `Screen.Font` / `GetScreenDrawInfo(...)->dri_Font` as appropriate and real glyph measurements (e.g. `TextLength`, `TextExtent`) rather than assuming that a proportional font has fixed-width cells. The portable layout receives conservative measured cell/line metrics plus exact measured title and border dimensions. The renderer must **clip all text**, truncate/scroll/paginate lengthy diagnostics and reserve a permanently reachable button row. If even compact Topaz/8 cannot fit, do **not** open an inaccessible window; provide a safe alternative (CLI or short explanation on a usable screen).

## Live screen/font change notifications — verified API distinctions

### System preferences file changes

AmigaDOS `StartNotify()` / `EndNotify()` is available from **V36 (AmigaOS 2.x)**. Monitor:
- `ENV:sys/screenmode.prefs`
- `ENV:sys/screenfont.prefs`

This alerts Dolly when preferences files change. **It does not guarantee** that Intuition has already reopened Workbench or that the same `Screen*` is valid. `IDCMP_NEWPREFS` alone is insufficient for newer font/screen settings.

Experimental adapter `amiga/dolly_amiga_pref_notify.c` uses a private Exec message port, `NRF_SEND_MESSAGE | NRF_WAIT_REPLY`, coalescing normalized events through `dolly_screen_watch_event()`. It replies to messages and calls EndNotify before releasing requests. StartNotify may fail when a particular prefs file does not exist; the failure leaves normal operation intact.

**The adapter has not been compiled against AmigaOS NDK or exercised in WinUAE.**

### Screen public lifecycle

AmigaOS 3.1.4 does **not** have a confirmed built-in classic screen-open/close notification replacement for `screennotify.library`. Don't equate file change notifications with imminent screen closing.

`screennotify.library` is a **separately installed optional third-party library** used by existing OS 2.x–3.x applications to track public-screen lifecycle events. Dolly should attempt to open it dynamically if installed; no hard dependency. Exact message protocol/function table and safe deregistration need confirmation from the library's SDK/source; **do not invent numeric function offsets**. Integrating it is still pending.

When a public screen is about to close, close or detach Dolly windows promptly and **invalidate any old screen pointer**. After the screen is available, reacquire via `LockPubScreen()`, obtain real border/font metrics and rebuild views. Repeated notifications should be coalesced, and window state/operation progress retained, without recreating windows during partial screen reset.

### Fallback when no ScreenNotify is available

Revalidate current screen geometry on open, activation/resize, changes in Dolly font settings and ordinary preference notifications. For extra safety, a **bounded deferred retry** can attempt to reacquire the public screen on the UI task when a preference change is pending; avoid busy-polling and requester loops. If the screen is unavailable, keep the UI suspended and report status when a usable screen returns. This does *not* guarantee that every external screen close is detected in advance.

## Planned native event loop

1. On startup, subscribe using the available DOS notify adapter and optional ScreenNotify, then record the current screen/font geometry.
2. Wait for Exec/Intuition/notification ports; drain all messages and reply to the appropriate owner.
3. Feed `DollyScreenWatch`; reduce duplicate events.
4. Before touching any screen, lock or otherwise validate it; no stale pointer use.
5. Plan `custom -> Screen Font -> topaz/8` per view with real text/chrome measurements.
6. Rebuild GadTools gadgets if necessary, preserve current task and window state.
7. If no valid view fits, fail closed rather than create an off-screen requester.
8. On shutdown, EndNotify active requests, drain/reply messages, deregister ScreenNotify, then close ports/libraries.

## Verification required

- Unit tests: `tests/test_gui_layout.c` checks all seven view types, 640x256 / 640x200, font preference order, oversized fonts and minimum bounds.
- Unit tests: `tests/test_screen_watch.c` checks coalesced events, screen close/reopen, no rendering while invalid, and missing optional notification library.
- **Not yet verified:** CTest of complete current `main`, actual gadget bounds, real window border/title size, proportional font strings, NDK build, and OS 2.04/3.1/3.1.4/3.2 WinUAE tests.
- Critical acceptance: capture screenshots of every GadTools view and warning in 640x256/topaz8 without clipped text, unreachable buttons or windows exceeding screen bounds.

### References

- [AmigaOS Preferences](https://wiki.amigaos.net/wiki/Preferences)
- [AmigaDOS file notification](https://wiki.amigaos.net/wiki/Notification)
- [Intuition window IDCMP_NEW PREFS constraints](https://wiki.amigaos.net/wiki/Intuition_Input_and_Output_Methods)
- [Intuition Screens font rules](https://wiki.amigaos.net/wiki/Intuition_Screens)
- [ScreenNotify 1.0 (Aminet)](https://aminet.net/package/util/libs/ScreenNotify10)
- [Issues #14](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/14) and [#15](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/15)

## M1.6 — GadTools-backed Sheep Counter, incremental grid (experimental)

The default `dolly-sheep-window` prototype now uses OS 2.x GadTools:
two read-only `NUMBER_KIND` values (blocks processed, errors) and a
`BUTTON_KIND` Cancel control. It retains the maximized public-screen
window **below** `Screen.BarHeight` and obtains system pens/font from
the current public screen. GadTools values are changed with
`GT_SetGadgetAttrs` — never by re-running `GT_RefreshWindow` every tick.

The 20x6 synthetic surface-map grid and simple progress bar remain
direct RastPort drawing: GadTools has no native cell-grid or
read-only progress indicator. Only the old and new active cells
and newly filled progress-bar span are redrawn for each block.
The current cell is marked with ASCII `*` rather than an image.

This prototype is not yet compiled or tested on MC68000, 512 KiB
Chip RAM, Kickstart 2.04. It uses synthetic data and never opens a
physical device or writes to any disk. Window resizing/re-exposure,
font-sensitive layout, real scan integration, and functional Pause
remain future work. The Cancel button stops only synthetic progress.

The first native test should inspect correct GadTools creation,
legibility and reduced flicker on actual WinUAE hardware emulation,
and confirm `WINDOW PASS` and exit code zero.

## M1.7 — resizable public-screen Sheep Counter (unverified)

The previous screenshot showed the "Blocks" gadget's auto-placed label
clipping past the left edge. M1.7 removes the externally positioned
GadTools label and draws "Blocks:" and "Errors:" explicitly **above**
the native number gadgets, within the client area.

The prototype window starts maximized **below the system screen title
bar**, but is now resizable with the standard Intuition size gadget.
It sets a minimum outer size of 420x205. Layout is recalculated from
current window border sizes, available width and height:

- 20 columns x 6 rows; individual grid cells grow/shrink in both axes
- progress bar width follows the grid width
- count gadgets grow/shrink and remain inside the client area
- Cancel button follows the right edge, control row follows bottom edge
- current cell retains the ASCII `*` marker

A resize triggers IDCMP_NEWSIZE, detaches and rebuilds **the complete**
GadTools gadget list (not selective refresh), then redraws the
grid at its new cell dimensions. Normal scan progress still touches
only the previous/new cell, progress-bar delta and number gadget.
Exposed-window refresh uses the GadTools-aware refresh calls.
This is intentionally a source-level test until the Docker m68k build
and WinUAE test confirm it on MC68000 / 512 KiB Chip / Kickstart 2.04.

Known limitations: fixed 20x6 logical grid, no true device I/O or
non-synthetic progress; font/geometry edge cases and repeated resize
memory behavior need testing. When an actual operation is in progress,
state must be preserved across all layout changes.

## Sheep Counter Hide semantics (M1.7 decision)

Sheep Counter is an **auxiliary progress view**, not the owner of the
running imaging/recovery operation. Its action button is labelled **Hide**,
not Cancel or Exit. The window close gadget has the same meaning.

- **Hide/close** dismisses only the Sheep Counter view and returns focus
  to the main Dolly window. The core-owned operation continues unchanged,
  including progress, errors and retries.
- The main window will offer a way to reopen Sheep Counter and show the
  current operation state. Closing the view is not an operation cancellation.
- Pausing, stopping, or canceling a running operation requires an explicit
  control in the operation-management UI with its own confirmation/safety
  rules, not the Sheep Counter Hide action.
- Current `dolly-sheep-window` is a **standalone synthetic executable**;
  hiding its sole window necessarily ends that demo process because there
  is no main window or independent core task yet. This is **not** a tested
  implementation of background continuation or view reopening.

The user confirmed the earlier M1.7 WinUAE build and that the resizable
GadTools appearance is an acceptable minimum on MC68000, 512 KiB Chip,
Kickstart 2.04. The subsequent Hide-label change is not yet retested.

### M1.7 minimum-width resize regression (2026-11-17)

User-reported WinUAE failure at the horizontal minimum:

```text
WINDOW before chip=302520
WINDOW open chip=270288
WINDOW FAIL: resize reflow
```

The window did not survive the attempted minimum-width reflow. Source
inspection identified an inconsistent minimum: Intuition allowed an
outer window width of 420 pixels while `calculate()` required an
**inner** width of at least 400 pixels, excluding the OS borders.
The check was relaxed to 360 pixels inner width, consistent with a
420-pixel outer minimum on the tested classic border geometry.

Furthermore, geometry is now validated before detaching existing
GadTools, and the proposed layout is not committed unless validation
succeeds. A rejected transient geometry with a still-valid previous
gadget list now reports a warning rather than closing the viewer.
This fix must be cross-compiled and independently retested on MC68000,
512 KiB Chip, Kickstart 2.04. No test success is claimed yet.

## M1.8 — system bar clearance and recessed progress grid (unverified)

Tester confirms M1.7 resize now works at minimum width. The next
visual refinement changes the system-screen default window geometry to
**Top = Screen.BarHeight + 1**, **Height = Screen.Height - Top**,
keeping the window bottom at the screen edge while avoiding the
screen title bar's last pixel. This is calculated from the actual
screen, not a hard-coded 640x256 rectangle.

The fixed 20x6 / 120 synthetic map becomes **24x8 / 192 cells**,
while both cell dimensions remain adaptive to the window client area.
The map sits in a classic system-coloured **recessed bevel box**
using GadTools `DrawBevelBoxA(..., GTBB_Recessed, TRUE)`; it is *not*
a `LISTVIEW_KIND` gadget. GadTools listview is for rows of items and
would not natively render/refresh 192 rectangular progress cells.
`DrawBevelBoxA` is available since V36 and does not require
AmigaOS 3.x. All cells continue to be drawn directly to the window,
without an extra bitmap or own screen.

Normal progress still only updates the old/new active cell, the
changed progress-bar segment, and the GadTools number gadget.
Only resize/exposure rebuilds the full map and recessed frame.
The symbol `*` remains a placeholder for Dolly.

No compilation or WinUAE run of M1.8 has been confirmed yet.
Test especially **both minimum dimensions**, grid-frame clipping,
system bar bottom pixel, and the memory report on MC68000,
512 KiB Chip RAM, Kickstart 2.04.

## M1.9 — fixed Topaz/8 transparent current-block marker (unverified)

The M1.8 recessed 24x8 progress grid remains unchanged in geometry and
uses system DrawInfo pens. For its ASCII `*` marker **only**, M1.9 explicitly
opens `topaz.font` size 8 using `OpenFont()`, switches the window RastPort
font transiently via `SetFont()`, sets `JAM1` transparent text mode,
draws the glyph, and restores the previous font and draw mode.
The font is closed when the demo ends. System labels and GadTools
widgets still use the screen's current font.

This prevents larger user-selected fonts from changing the glyph
shape/size or adding an opaque text background. On a minimum-size grid,
the 8px glyph may still overlap its thin cell outline; clipping is a
separate possible enhancement and is not claimed here.

Requires Docker m68k build and WinUAE verification (especially
minimum-width/height resize and memory cleanup) on MC68000, 512 KiB
Chip RAM, Kickstart 2.04. The prototype remains synthetic/no disk I/O.

## DOL-004 — portable OperationSession groundwork (experimental)

The new C99 `dolly_operation_session` module is independent of GUI,
Intuition, POSIX, and disk/device I/O. It holds:
- 64-bit total and completed block counters;
- error and retry counters;
- explicit idle/running/paused/completed/failed/cancelled state;
- a separate `sheep_counter_visible` flag.

`dolly_operation_hide_sheep_counter()` changes **only visibility**.
It never pauses, cancels, or resets an active operation.
When the view reopens it can read the same session's progress.
Progress updates are monotonically bounded and disallowed while
paused or after completion. Cancellation is exposed as a **separate**
core control; no UI Hide event is allowed to call it.

The module and its synthetic unit tests are included in ordinary and
native-only CMake builds, with no physical device operations.
A successful CTest run is still required before claiming native or host
validation. The current standalone Sheep Counter demo is **not yet
connected** to OperationSession. There is no background worker yet;
continuing real operations after hiding the view remains future work.

Next integration: construct the application's operation owner and shared
session state, then feed the main window and Sheep Counter from that
single source. Avoid creating a second operation when a view reopens.

### Two-window execution model — explicit user decision (2026-11-17)

**The main Dolly window remains open at all times when Sheep Counter
is displayed.** Sheep Counter is opened as a **second Intuition
window on the existing public screen** (not a replacement for the main
view). It can be brought forward, hidden, then reopened without
resetting or aborting the core-owned operation.

Experimental native `dolly-two-window-demo` now exercises this
architecture using a single `DollyOperationSession` in one process:
the main window contains `Sheep Counter` (show) and `Quit demo`
buttons; the auxiliary Sheep Counter has `Hide` and a close gadget.
A shared synthetic 192-block operation progresses even while Sheep
Counter is hidden. Reopening the view displays its latest progress.
The main window remains allocated/open while the secondary view is
shown or hidden. The user can terminate the test using `Quit demo`
in the main window, not Hide in the auxiliary view.

This early two-window diagnostic is intentionally **separate from**
the accepted resizable GadTools Sheep Counter executable. The small
auxiliary demonstration is not yet a transplant of the final scaled
layout, and its progress repaint is preliminary. No disk/device I/O
or real worker concurrency is involved; the operation progresses
cooperatively in the main event loop. Cross-build and WinUAE native
testing remain outstanding.

Validation: MC68000 / 512 KiB Chip RAM / Kickstart 2.04 / Workbench
640x256: open both views, Hide Sheep Counter, verify main counter
continues, reopen Sheep Counter, verify it resumes at a nonzero
current count, close Sheep Counter with its close gadget and finally
Quit demo via main window. Check `Echo $RC` and absence of
memory pressure/failures. Low-memory success must not be claimed
before testing both windows open simultaneously.

### M1.11 — GadTools counters in two-window demonstration (unverified)

User tested the initial two-window demo on MC68000 / 512 KiB Chip /
Kickstart 2.04: main and auxiliary window lifecycle was functional,
but screenshot showed a damaged Sheep Counter view, bad-looking
numbers (e.g. `Blocks: 29 / 0 | Errors: 42582912`) and slow full
redraws on every progress tick. The source used `sprintf`, `Text`,
and a full 24x8 grid redraw at every tick. This was inferior to the
previously validated standalone Sheep Counter.

The next code revision replaces both numeric counters with native
GadTools `NUMBER_KIND` gadgets updated using `GT_SetGadgetAttrs`,
while rendering labels only during creation/refresh. It uses incremental
cell changes for the 24x8 synthetic grid rather than redrawing 192 cells
on every progress tick. The star uses Topaz/8 with transparent `JAM1`.
The operation remains in a single shared C99 `OperationSession`
and progresses even when the auxiliary window is hidden.

This version is an integration prototype, **not yet** the polished
resizable Sheep Counter. It must be compiled and tested in WinUAE
before the counter rendering, screen appearance or memory footprint
can be called validated. Production code will use existing GadTools
controls whenever appropriate, with direct drawing reserved for custom
progress maps/visuals for which GadTools has no suitable gadget.

### M1.12 — responsive main + Sheep Counter windows (unverified)

User feedback on the first two-window demonstration: main window and
Sheep Counter work correctly as separate views, but neither had resize
gadgets; Sheep Counter's 24x8 grid had no recessed frame; both windows
left too much unused area. Requested client-space margins: **4px
horizontal + 2px vertical on non-square pixel displays** or **2px
horizontal + 2px vertical on square-pixel displays**, measured from the
actual inside edges of the system window border.

AmigaOS 2.x provides real `Window.BorderLeft/Right/Top/Bottom` and the
display's pixel resolution ratio in `DrawInfo.dri_Resolution.X/Y`. It
does not provide a universal intrinsic padding parameter for custom
GadTools client layouts, so the user-requested padding is explicitly
applied relative to the runtime border widths.

The new two-window integration demo now opens **both windows resizable**
with `WA_SizeGadget`, minimum dimensions, and `IDCMP_NEWSIZE`.
It rebuilds native GadTools gadgets when resized and repositions
them to the bottom/top of usable client space. Sheep Counter's
24x8 grid uses almost all remaining central area, with a
`DrawBevelBoxA(GTBB_Recessed)` container and system palette.
Each cell's span is calculated proportionally from its grid boundary
to distribute fractional unused pixels instead of leaving large
right/bottom gutters.

Routine progress is **incremental**: update the previous/current cells,
only the newly filled progress-bar span, and NUMBER_KIND counters via
`GT_SetGadgetAttrs`. Static labels and the full grid are redrawn on
window resize and refresh only. `Hide` still closes solely the
auxiliary window and the shared OperationSession continues. Sheep
Counter opens maximized below the screen bar (+1px clearance); the
main window remains open behind it.

The code has **not yet been compiled or executed** on the user's
Docker/WSL2 setup. Acceptance requires MC68000, 512 KiB Chip RAM,
Kickstart 2.04, 640x256 Workbench: resize both windows to minima and
back, verify all controls accessible, grid frame inside boundaries,
no excessive flicker, Hide/reopen preserves progress, and successful
cleanup. No physical device operations are performed by this demo.

### M1.13 — current / total and system-style progress bezel (unverified)

User requested that the two-window demo display block counts as
`current / total` (for example `29 / 192`) in both windows and show
a numeric completion percentage immediately **to the right** of the
Sheep Counter progress bar. Use native GadTools `TEXT_KIND` gadgets
for these composite string values (not per-frame direct Text/sprintf),
updated with `GT_SetGadgetAttrs(..., GTST_String, ...)` and backed by
stable, view-owned character arrays. Keep `NUMBER_KIND` for the
standalone Errors counter.

The progress bar has a `DrawBevelBoxA(GTBB_Recessed)` frame matching
the grid's classic GadTools recessed container. **Do not** paint a
separate black/shadow rectangle around the colour fill. Only fill the
interior with BACKGROUNDPEN and FILLPEN from the current screen's palette.
Routine progress updates fill only the newly completed interior span;
the native percentage gadget is refreshed separately. On resize/exposure,
the same bezel and fill are reconstructed at the new dimensions.

Progress percent is based on completed/total blocks and clamped to 100.
The current 192-block synthetic demo fits in ULONG. A real operation
with 64-bit block counters will require a proper 64-bit text conversion
without truncation before integration with device I/O. The separate
Sheep Counter remains optional/auxiliary: Hide never cancels or pauses
the shared OperationSession; the main window remains open.

This is source code in a draft PR and has not yet passed the user's
Docker m68k build or WinUAE runtime test.

### M1.13 GadTools TEXT_KIND compile correction

The first Docker m68k build reported `GTST_Border` undeclared. The source accidentally used STRING_KIND (`GTST_*`) tags for read-only TEXT_KIND controls. Updated all affected creation and runtime attributes to `GTTX_Text` and `GTTX_Border` according to AmigaOS V36 GadTools documentation. Percent and `current / total` remain TEXT_KIND, with view-owned buffers; Errors remains NUMBER_KIND. Native rebuild and WinUAE verification are still outstanding.
