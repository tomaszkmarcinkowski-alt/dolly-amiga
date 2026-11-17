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

### M1.14 — prevent interim old-size gadget painting during resize (unverified)

Tester clarified that main-window GadTools button geometry is **correct
after resizing**, but while dragging smaller the old-size button is
temporarily painted and overlaps the new frame. This is **not** an
incorrect minimum window width. Do not change the accepted dimensions
or font/layout calculations to compensate.

On AmigaOS 2.x, `IDCMP_SIZEVERIFY` arrives before interactive
window sizing and `IDCMP_NEWSIZE` when complete. The M1.14
two-window prototype now detaches its GadTools list in
`IDCMP_SIZEVERIFY`, replies immediately, and rebuilds + reattaches
it after `IDCMP_NEWSIZE`. A detached-state flag prevents duplicate
`RemoveGList` and an invalid geometry retains a reattach path.
Full custom rendering remains limited to layout/refresh; routine
progress is incremental.

Keep `WA_SmartRefresh`: Intuition already preserves obscured
window pixels where possible. `GT_RefreshWindow` is called only
after gadget insertion/reinsertion, not on each progress tick.
This specific change is intended to avoid old-size gadget redraw
during drag; it must be cross-compiled and tested on WinUAE
MC68000 / 512 KiB Chip / Kickstart 2.04. It has **not** been
runtime-validated.

### Minimum window geometry must be content-derived (GUI policy, 2026-11-17)

The tester confirmed the M1.14 resize behavior in WinUAE, but one
placeholder label ("Sheep Counter is an auxiliary window.") slightly
overlaps the frame at the current minimum size. The placeholder itself
does not require special treatment; the general constraint is now
**mandatory for future Dolly GUI work**:

1. Measure the actual displayed font and label widths using the
   window RastPort's `TextLength()` (or equivalent graphics text
   measurement). Do not estimate English text widths from character count
   or assume Topaz/8 for labels when the screen font is configurable.
2. Account for native gadget dimensions and text, internal separation,
   requested content padding, and the actual window borders from
   `BorderLeft/Right/Top/Bottom`. Use 4px horizontal and 2px vertical
   padding for non-square pixels, or 2px/2px for square pixels.
3. Derive the window's minimum **outer** width and height from the
   tightest permissible content layout plus the actual window chrome,
   including a size gadget if used. Update constraints when the layout,
   font, localization, screen geometry or visible controls change.
4. Before accepting an `IDCMP_NEWSIZE` reflow, ensure every visible
   control and *all mandatory text* fit inside the client rectangle.
   Treat optional explanatory text separately: wrap, abbreviate or hide
   it when too narrow, rather than clipping across the border.
5. Preserve the current content-derived minimum under subsequent
   resizes. Test the smallest width, smallest height, both minima
   simultaneously, system-font changes, and longer translated labels.
6. Never silently discard actions such as Hide/Quit to make a window
   smaller. Prefer a higher minimum or a well-defined responsive layout.
7. Retain native GadTools gadgets and update values with their native
   attributes. Do not substitute repeated manual painting of text
   for controls whose sizes and content can be managed by GadTools.

The current demonstrator still contains hard-coded `MAIN_MIN_W/H`
and `SHEEP_MIN_W/H`; it is not evidence that this general measurement
policy is implemented yet. Incorporate a measurable layout preflight
and corresponding boundary tests before treating the full GUI as
release-ready.

### M1.15 — measured minimum dimensions on both native windows (requires WinUAE verification)

The **portable C99** `dolly_gui_minimum` module accepts actual measured
pixel widths of mandatory captions and button labels, current font height,
true Intuition border thickness and the agreed aspect-dependent inner
padding. It calculates the smallest outer rectangle for the two current
layouts, including equal-width action buttons, GadTools number/text
controls, and the 24x8 Sheep Counter grid. It never guesses text
widths from character counts.

The AmigaOS 2.04 demo calls `TextLength()` on the opened window's
RastPort and then sets `WindowLimits()` from that measured minimum,
before constructing the GadTools gadget list. The window still opens
in the established location; this change affects its minimum dimensions,
not the preferred window size. Optional explanatory prose is suppressed
if its measured width does not fit the current client rectangle.

The portable unit test `test-gui-minimum` checks mandatory equal-width
button sizing, grid width and height, larger labels, square-pixel
padding and invalid font metrics. It is added to ordinary and
native-only CMake CTest configurations. A passing build or test has
not yet been observed for this branch.

This is the **first preflight integration**, not an exhaustive
localization/font stress test. Remaining work includes dynamically
recomputing limits on screen/font changes, testing longer translated
labels, avoiding clipping under extreme font metrics, and extracting
other views' constraint descriptions into a common layout engine.
The auxiliary Sheep Counter still never owns/cancels the core operation,
and this demo performs no disk I/O.

### M1.16 — GadTools main-window shell; Sheep Counter initially closed

User decision: **the main window opens first, with Sheep Counter closed
by default**. The Sheep Counter button is unavailable when the core
OperationSession is IDLE. It becomes enabled only during RUNNING/PAUSED
states, where it opens an auxiliary public-screen window without closing
the main window. Hide/close on the auxiliary window hides only that
view, without affecting the running operation. After completion the
main window disables opening a new Sheep Counter; an already visible
counter may remain visible to show 100% until the user hides it.

The first native M1.16 synthetic shell uses classic AmigaOS 2.04 GadTools:

- `CYCLE_KIND` for two **synthetic** device choices and two operation
  names; this is *not* physical device enumeration or disk access
- `TEXT_KIND` for the read-only synthetic operation description/status
- `BUTTON_KIND` Start demo, Sheep Counter and Quit demo
- `TEXT_KIND` completed/total block count, from OperationSession

Start demo is the only trigger for the synthetic session. While active,
Start demo and selection cycles are disabled with `GA_Disabled` through
the approved GadTools APIs. The Sheep Counter control becomes enabled.
When the session finishes, the controls return to idle availability.
Reopening a Sheep Counter during an operation uses current session
progress. A new demo run resets the synthetic state without opening
any real device.

The measured main-window minimum has been adjusted to fit the added
cycles, status, two action rows, and count. The main window remains
on the public Workbench screen with SmartRefresh, using the current
system font/pens and content-based dimensions. The 24x8 Sheep Counter
renderer, Topaz/8 marker, recessed frames and incremental progress
are unchanged. No physical media I/O occurs.

**Status: source-only.** M1.16 needs Docker m68k compilation and WinUAE
validation on MC68000, 512 KiB Chip RAM, Kickstart 2.04. Important
checks: only the main window at launch, Sheep Counter disabled before
Start demo, correct behavior after Start, Hide/reopen while the synthetic
session continues, both minimum window sizes, and cleanup on Quit.

### M1.17 — separate source and destination pickers (unverified)

Per user feedback on the validated M1.16 prototype, source/target selection
must **not** use CYCLE_KIND. The native main window now has distinct
`Source...` and `Destination...` GadTools BUTTON_KIND controls.
Each opens its own identical Intuition chooser window, driven by the same
`DollyEndpointPicker` implementation and independent selection state.
Destination is enabled only for operation kinds requiring a source and
a destination (synthetic Copy and Restore), not for Scan or Inventory.

Each chooser presents a hierarchical three-step selection with native
GadTools LISTVIEW_KIND controls: device first, then unit, then partition.
Unit and partition lists remain disabled until their parent is selected.
Explicit `Whole unit` and `Partition` actions commit the desired scope.
A separate `File...` action uses the standard asl.library FileRequester:
load mode for source, **V37-compatible ASL_FuncFlags/FILF_SAVE** mode for
destination (the newer ASLFR_DoSaveMode tag requires V38). The model
stores the selected full path, not a file handle. Both picker interfaces
currently contain **synthetic** devices, units and partitions. There
is no OpenDevice(), disk scan, partition write, or transfer in M1.17.
The ASL requester naturally enumerates filesystem directories when
the user explicitly requests a file; its Save variant may offer
filesystem directory creation, so avoid claiming that the requester
itself makes zero filesystem accesses.

`dolly_endpoint` is a portable C99 state model distinguishing a whole
unit, partition and file, keeping source and destination independent.
`dolly_demo_can_start` requires a source, requires a destination only
for two-sided operations, and rejects obviously identical source/target
paths or overlapping unit scopes. A portable CTest regression test
covers these boundaries. Selection views do not acquire device handles.
To conserve memory on a 512 KiB Chip-only system, opening one picker
closes the other; each role has a separate window and preserved selection.
The main Start gadget is disabled during an unfinished selection, and
controls become unavailable during a running operation.

The operation selector still intentionally uses CYCLE_KIND and now
offers Scan, Inventory, Copy and Restore **synthetic demonstrations**.
A separate TEXT_KIND line explains the selected operation. Status
moves to the bottom of the window, in a native GadTools recessed bevel.
The main window is **horizontally resizable only**: WindowLimits()
fixes its vertical outer dimension to the initial size. The Blocks
line sits at a fixed y-offset independent of window width; it no
longer moves toward the Start button when resizing. The established
4x2px (non-square) or 2x2px (square) client padding remains.
TEXT_KIND displays pre-clip their visible labels using the real
RastPort TextLength() to avoid relying on V39+ clipping behavior.

The separate Sheep Counter starts closed and becomes available only
during a RUNNING/PAUSED operation, as accepted in M1.16. Its native
resizable 24x8 GUI, recessed grid, progress bar and percent readout
remain unchanged. Hide does not cancel the shared OperationSession.

**M1.17 is source-only until verified**: needs host
`test-endpoint` and `test-gui-minimum`, native Docker m68k build,
and WinUAE tests for source unit/partition/file, destination
availability, changing operation kind, no overlap on narrow main
window, no vertical main resize, chooser cancel/reopen, and Sheep
Counter hide/reopen on MC68000, 512 KiB Chip RAM and Kickstart 2.04.

### M1.18 — endpoint type first, with four GadTools picker modes

User requested that detailed memory measurement be deferred until
the end of visual/functional GUI work. The priority here is interaction
and appearance, not profiling.

The shared Source and Destination picker now begins with a native
`CYCLE_KIND` offering four **endpoint types**. This is explicitly
different from the rejected old device-choice Cycle:

- **Physical device:** select `device`, then `unit`; choosing the
  unit means the **entire disk/unit** rather than an arbitrary partition
- **Logical device:** select a device/volume **mounted by AmigaDOS**,
  represented in this prototype by synthetic `DH0:` and `DH1:` entries
- **Partition:** select `device`, then `unit`, then a partition from
  RDB/MBR; current entries are explicitly synthetic examples, **not**
  actual RDB/MBR parsing
- **File:** use the native ASL file requester (load for source, save
  for destination); no disk image file is created by the demo

The selector uses only the controls needed in its selected mode and
rebuilds its GadTools gadget list when Cycle changes. Parent-dependent
lists stay disabled until a parent selection exists, with a final
`Use selection` confirmation. Source and destination use the same
implementation but have distinct selected endpoint state.

The main operation Cycle adds **Create image**. Its input is a selected
device/unit, logical device, partition, or file; its **destination must
be a file**. For convenience, the unselected destination picker opens
in File mode when Create image is selected; the unselected source
picker opens in File mode for Restore. The operation description is
still displayed below the main operation Cycle. Target selection
remains disabled for single-ended Scan and Inventory operations.
Nothing reads a physical unit, parses RDB/MBR or writes image data
in this prototype.

`dolly_endpoint` now models logical devices separately from
physical units, partition references and filenames. Host tests
cover mode mapping, logical-vs-logical identical endpoint prevention,
file-only Create image destinations, and the existing Copy/Restore
constraints. Alias detection between an AmigaDOS logical device and
a physical partition is **not yet possible** without actual device
discovery; the model cannot authorize physical writes.

**Unverified:** M1.18 requires a fresh host endpoint regression test,
m68k cross-build using the NDK in Docker, and WinUAE interaction tests.
Do not claim that the user has already tested this version.

### M1.18 repaint fix — changing endpoint type

Confirmed user regression: switching the mode Cycle in Source or
Destination removed the previous LISTVIEW_KIND gadgets from the
Intuition gadget list, but **left their old pixels in the window
RastPort**, causing overlapping graphical remnants. Removing a
GadTools gadget does not automatically clear its last-drawn area.

Before creating or attaching the next mode's gadgets, the picker now
clears the actual client rectangle (bounded by the live
`Window.BorderLeft/Right/Top/Bottom`) with the public screen
`BACKGROUNDPEN`. The window's native title bar/borders are untouched.
Afterwards it attaches and refreshes GadTools, re-evaluates enabled
controls, and redraws mode labels. This happens only on initial view
creation and type transitions, **not** on every progress tick. The same
function handles both source and destination picker windows.

Pending Docker m68k compilation and WinUAE runtime confirmation.

### M1.18 — File mode: preview before commit

Following WinUAE feedback, the File mode no longer puts `Browse file...`
in the bottom `Use selection` position. Instead it renders a
full-client-width GadTools `BUTTON_KIND` above a full-width recessed
`TEXT_KIND` path field in the area used by the other modes' lists.
The bottom action row **always** retains separate `Use selection`
and `Cancel` controls.

ASL now selects a **pending** file endpoint, updates the read-only text
field and enables `Use selection`; the parent Source/Destination
selection is not changed until that button is clicked. Closing or
cancelling the dialog leaves the previous committed endpoint intact.
The path displayed in the text field is measured with the active font's
`TextLength()` and clipped with an ellipsis to avoid crossing borders;
the full unmodified path remains in `pending_file` and is committed
on confirmation. An initially selected file is preloaded when revisiting
the chooser.

Same behavior applies to Source File and Destination File modes,
including Create image. No actual image I/O is performed. This GUI fix
still requires Docker m68k compile and WinUAE validation.

### M1.18 — ASL current-directory and proportional list columns (WinUAE feedback)

When selecting a file in the current directory, the ASL file requester
may return a valid filename but an empty or NULL drawer string. The
previous picker incorrectly required a **non-empty drawer** and silently
rejected this case, leaving Selected file blank and Use selection
disabled. The picker now accepts `fr_File` by itself as a valid relative
AmigaDOS pathname, or combines drawer + filename when a drawer is
provided. The pending-file preview and explicit Use selection behavior
remain unchanged. As with other relative DOS paths, any eventual real
I/O implementation must resolve or preserve the correct directory
context; this prototype does not open the file.

Physical device now uses **two equal-width LISTVIEW_KIND columns**,
Device and Unit, filling the available client width. Partition uses
**three equal-width columns**, Device, Unit and Partition. Both the
column list gadgets and their captions use the same geometry function
based on the actual Intuition window borders and a constant 8-pixel
gutter. Previously the first two lists had fixed narrow widths even
when the third column was absent.

This revision is committed but needs m68k Docker compilation and WinUAE
confirmation in both Source and Destination pickers.

### M1.18 — touching columns, image format and source-file inspection

Per user instruction, Physical device's two LISTVIEW_KIND columns and
Partition's three columns now have **zero intentional inter-column
gutter**, sharing the full client width, with proportional widths
derived from the actual Intuition window border sizes.

File-mode behavior differs by endpoint role:

- **Destination**: beneath Browse file and its recessed path preview,
  a native `CYCLE_KIND` now chooses **RAW / HDF / ADF / IFF DOLY**.
  The selected format is stored in `DollyEndpoint.image_format` and
  preserved when the selection is committed. RAW is the prototype
  default. This is a *parameter only*: no output file encoding or IFF
  DOLY format serializer exists yet.
- **Source**: beneath Browse file and the recessed filename/path preview,
  another recessed `TEXT_KIND` shows basic file metadata. The size
  is obtained from AmigaDOS `Lock` + `Examine` (`fib_Size`), using
  read-only directory metadata, with a clear 'unavailable' fallback.
  The type label is a **filename-extension hint only** (ADF, HDF,
  RAW/IMG, possible IFF), *not* content sniffing or format validation.
  On filename selection, `GT_SetGadgetAttrs` updates this field.
  File paths that are relative to the current AmigaDOS directory remain
  valid inputs, and no physical disk block access is introduced.

The two views reuse the same picker window and native GadTools
rendering; the output-format control is specific to the Destination
File mode. A new portable endpoint regression assertion checks
format metadata attached to the committed file endpoint.

**Awaiting Docker/WinUAE verification.** Memory profiling remains
deferred per the user's earlier request.

### M1.18 startup regression — alert 0x8000000B

User WinUAE report after adding DOS-backed source file metadata:
`Software Failure / Program failed (error #8000000B)` before
a usable main window. This is the MC68000 Line-F exception code,
but the alert alone does not identify the instruction or root cause.

The latest experimental patch avoids defining, manually opening,
or manually closing a separate global `DOSBase` in the executable.
The Amiga C runtime startup is expected to initialize the base used
by `proto/dos.h`; the metadata reader still guards its DOS usage
with `if(DOSBase)`. The original working demo did not manually
manage this global. This is a **testable hypothesis**, not a proven
diagnosis, until a rebuilt binary runs in WinUAE.

Three startup breadcrumbs were added to Shell output:
`M1.18 BOOT: entered main`, `public screen ready`, and
`main window ready`. If an exception remains, record the last
visible line and the WinUAE CPU debugger PC/opcode. Do not
interpret this alert as proof of FPU code or hardware damage.

**Do not mark M1.18 validated** until MC68000 / 512 KiB Chip /
Kickstart 2.04 boot and both selectors pass.

### M1.19 GUI preflight — native resizable Source/Destination pickers

After user acceptance of the M1.18 main window and file-mode UI,
the Source and Destination selectors are now **native resizable**
Intuition windows (including SizeGadget). This GUI milestone comes
before physical device discovery and before memory profiling.

At the accepted 606x195 outer pixel baseline, the modal-looking
chooser keeps its existing native GadTools layout and remains fully
usable on the 640x256 Workbench reference screen. The window
advertises `WA_MinWidth=606`, `WA_MinHeight=195` and screen-based
maxima, so it cannot be shrunk smaller than its established tested
baseline. More capable/high-resolution systems can expand it
horizontally and vertically.

New `picker_geometry()` computes the actual client-span width
from the current Intuition border sizes. LISTVIEW_KIND columns
occupy all the horizontal space (zero gutter), divide it evenly
between two Physical or three Partition lists, and their heights
expand with the additional window height. Logical mode's single
LISTVIEW_KIND fills the same span. The type Cycle widens
horizontally. File-mode Browse button, path field and file info/format
control widen with the available client area. The bottom Use
selection and Cancel buttons remain anchored along the bottom.
Selected file preview and source metadata clip against current
measured field width using the active RastPort font.

Like the already validated two-window demo, interactive resize uses
`IDCMP_SIZEVERIFY` to detach the old GadTools list before shrinking
so old-size gadget images cannot flash outside the client area.
`IDCMP_NEWSIZE` rebuilds and reattaches the gadgets using the
new dimensions. The background is explicitly cleared before new
gadgets and mode captions are drawn, preserving the M1.18
no-ghosting fix. The `GTLV_Selected` state is restored on rebuild
from the independent picker state so an in-progress hierarchical
selection survives resizing. Both roles use the same code.

This version is **not yet m68k compiled or WinUAE tested**. Test
source and destination independently at minimum, maximum
horizontal and vertical sizes; resize while switching modes,
with device/unit/partition selected; check File ASL selection
and output format Cycle after resizing. The main window and
Sheep Counter are unchanged; no physical media operation is added.

### M1.19 — selected endpoint preview in each LISTVIEW mode

Added user-requested full-width, **recessed GadTools TEXT_KIND**
selection preview **under the LISTVIEW(s), above Use selection and
Cancel** in Physical, Logical and Partition modes. The preview shows
the provisional selection immediately: selected device, selected
unit or logical name, and selected partition when applicable. It
never commits to the main window until Use selection is clicked.
Cancel retains the previous committed endpoint.

The chooser's `picker_geometry()` reserves a fixed-height preview
row above the bottom action buttons, then gives the remaining
vertical space to the list views. The preview stretches to the
current client width, uses the same system-font text measurement
as File path previews, and is rebuilt/restored when the native window
resizes or the type Cycle changes. File mode keeps its **existing**
path preview, metadata and image-format controls without a redundant
second preview.

GUI-13 through GUI-17 in `docs/TEST_MATRIX.md` cover this addition.
The new module has **not yet been cross-compiled or validated in
WinUAE**. RTG and memory tests remain deferred.

### M1.21 — first real read-only device discovery (AmigaDOS)

Logical device mode now snapshots the system AmigaDOS **registered
device names** using the OS 2.0/V36 APIs LockDosList
(LDF_DEVICES | LDF_READ), NextDosEntry and UnLockDosList.
The copy of each BSTR name into picker-owned memory is finished
**while the list is locked**; GUI drawing and other operations happen
only **after unlocking**. This is important because DOS-list entries
can change and DOS packet operations must not be made under that lock.

The picker lists up to 24 real DOS device names (with trailing ':').
It neither synthesizes DH0/DH1 entries nor assumes a particular
number of devices. If discovery is unavailable or no entries exist,
the Logical preview reports that explicitly and the list is disabled.
The first-24 limit is reported; entries whose complete device name
cannot fit the bounded name buffer are skipped, not silently renamed.
No dynamic memory allocation is required for the snapshot.

Selected logical endpoints store both a snapshot index and a
stable DOS device name in their path field; reopening the picker
matches the name to the new snapshot if it still exists.
The main window displays the real name. The demo overlap guard
rejects equal real DOS names even if enumeration indexes differ.
Physical device and Partition remain **synthetic** at this stage.

**Important semantic limit:** registration in the AmigaDOS device
list does NOT prove that a medium is present, that its filesystem
is mounted or that a related hardware unit is available.
There is no OpenDevice(), real block read/write, RDB scan,
filesystem mount, resource initialization, or DOS-list mutation.
This remains a discovery preview suitable for 68000/Kickstart 2.04.

See docs/TEST_MATRIX.md (DISC-10–DISC-20). Cross-compiler and WinUAE
runtime verification are pending. RTG tests and memory profiling
remain deferred by request.

### M1.22 — conservative DOS filter and second disk-information inset

The read-only AmigaDOS enumerator now ignores the six explicitly
specified **known non-block handler names** RAW:, CON:, RAM:, SER:,
PAR:, PRT:, with case-insensitive full-name matching before snapshot.
There is **no universal DOS-list bit** equivalent to "verified block
device": DLT_DEVICE includes stream handlers and other software-only
devices, and other third-party nonblock handlers may remain. In a
later stage classify candidates by their startup environments and
driver capabilities, with graceful "unknown" fallback.

Physical and Partition pickers now show a second full-width inset
TEXT_KIND panel under the existing endpoint-selection preview.
The new panel is reserved for actual driver-supplied SCSI INQUIRY
identity and independently obtained disk capacity. The two panels
remain responsive to chooser resizing. For the 606x195 minimum,
spacing is reduced to approximately 3 pixels between the list and
first inset, between insets, and between the second inset and buttons.
The LISTVIEW receives the remaining height. Logical and File modes
retain their single/pre-existing panels.

**No hardware probe is yet performed.** Physical device and Partition
still show an explicitly synthetic inventory. Consequently the disk
identity inset honestly says "INQUIRY: not queried (demo) | Size:
unavailable" after a unit selection. It would be incorrect and risky
to issue hardware SCSI commands to those invented driver/unit
indices, or to derive the size from INQUIRY (which does not include
disk capacity). Future verified physical enumeration can pass known
driver/unit tuples to a read-only HD_SCSICMD INQUIRY and a separate
TD_GETGEOMETRY / SCSI READ CAPACITY query. Device command and
resource error paths require their own dedicated WinUAE coverage
before connecting this UI to a real drive.

See DISC-21 through DISC-29 in docs/TEST_MATRIX.md. Not yet
cross-compiled or runtime tested. RTG and memory profiling deferred.

### M1.22 — compact top/bottom margins on classic non-square-pixel screens

The picker now derives a compact-vertical-layout flag from the
public screen's pixel dimensions: screen width > 2 * screen height.
This includes the tested 640x256 classic Workbench reference mode,
whose non-square pixels make the previous vertical whitespace
especially visible. The mode does not alter proportional 4:3-like
screens, so the previously accepted offsets remain there.

On compact screens the selection Cycle, headings, column captions,
list start and File controls shift upward by 5 px. The Use selection
and Cancel action row shifts 8 px toward the bottom window border.
The reclaimed 13 px are given to LISTVIEW height. Both Physical /
Partition recessed panels remain full-width with their existing
3 px inter-panel/list gaps, and stay anchored above the action row.
All coordinates are calculated through picker_geometry() so Source
and Destination and resizes use the same layout rules.

This is a geometry-only change; no changes to DOS discovery or disk
I/O. Newly added tests GUI-33–37 are pending 640x256 verification.
RTG/proportional mode testing remains deferred.

### M1.23 — physical candidate discovery and opt-in identity probe

M1.22 left the Physical device and Partition selection lists
synthetic. To avoid probing arbitrary guessed driver/unit pairs,
M1.23 first creates a separate **console-only** executable
\`dolly-physical-probe\`. It is not automatically invoked by the GUI
and has no write commands.

Without arguments, the executable snapshots up to 16 unique
driver/unit candidates from *registered* AmigaDOS DLT_DEVICE entries
under a read lock, validating plausible FileSysStartupMsg and DosEnvec
metadata (including TypeOfMem and BSTR checks), then unlocks before
printing. Multiple DOS partitions sharing one physical driver/unit
are deduplicated. Entries with unfamiliar startup formats are skipped.
This is conservative and still not a general proof of an object's
block capability: third-party startup messages can be driver-specific.
Unknown entries are explicitly counted rather than guessed.

\`dolly-physical-probe --probe INDEX\` sends two read-only queries to
**one** explicitly chosen candidate: TD_GETGEOMETRY reports total
sectors and sector size (if supported), and HD_SCSICMD INQUIRY
(0x12, 36-byte data) reports sanitized vendor/product/revision.
Both the CDB and returned SCSI data use Chip RAM to accommodate
older DMA requirements. Drivers may reject either query; absent
results are labelled unavailable, never fabricated. INQUIRY itself
does not report disk size. The hardware commands are synchronous,
so no transport timeout is implemented yet. Limit these tests to a
controlled WinUAE configuration before further robustness work.

No I/O is issued in normal Dolly GUI operation; no actual RDB/MBR
partition scan is attached. Only after the verified driver/unit
mapping and driver capability results have passed PHYS-01..10 can
this information replace synthetic Physical/Partition entries and
fill the second inset panel. This deliberately isolates the highest
risk new code path from the previously accepted M1.22 GUI.

Test commands and expected observations are in docs/TEST_MATRIX.md.
RTG tests and memory footprint profiling remain postponed.

### M1.24 — labels alongside recessed picker fields

At the 606x195 minimum, both Source and Destination now display
their selection preview with a left-side label **in the same row**:
"Selected device" for Physical/Logical, "Selected partition" for
Partition, and "Selected file" for File. The preview textbox begins
at x=180 (window relative) and expands to the right border.

Physical and Partition keep two vertical preview rows. Their second
row now comprises a "Device name" left label, a recessed name value,
a separate "Size" label, and a **second recessed** size value at
the far right. The capacity value is no longer embedded in the
INQUIRY identity string. Geometry derives these widths from the
live client border, expanding the name value with window width and
anchoring the 102px size value to the right. The 3px vertical
gaps and baseline action buttons remain unchanged.

File Source keeps its metadata field with the inline "File info"
caption, and File Destination retains its image format Cycle with
inline "Image format" caption. Actual physical identities and sizes
remain unavailable in the GUI until the independently gated M1.23
probe is validated; synthetic Device/Unit choices do **not** trigger
hardware queries. Read-only UI geometry only.

Tests GUI-38..46 in docs/TEST_MATRIX.md. The code has not yet
been cross-compiled or validated in WinUAE. RTG and RAM profiling
are postponed at the user's request.

### M1.24 continuation — data contract follows the revised picker

The user accepted the M1.24 GUI arrangement: the first selection row
has a role-appropriate inline Selected device/partition/file label.
Physical and Partition have a **second row with independent recessed
Device name and Size values**; the size field is at the far right.
Do not merge these values back into a single descriptive text string.

The opt-in M1.23 Shell diagnostic now emits those exact two fields:
- Device name: sanitized SCSI INQUIRY vendor and product, or Unavailable.
- Size: reported TD_GETGEOMETRY total sectors * sector size in MiB,
  or Unavailable; this is not inferred from INQUIRY.

One query failing does not clear the other result. OpenDevice failures
are also shown as unavailable, and CloseDevice is only called after
a successful OpenDevice. Neither query is automatically run by the
picker because Physical/Partition entries are still demo placeholders.
Physical GUI integration is blocked on verification of real DOS
startup driver/unit candidates and the opt-in read-only diagnostic.

This preserves current UI without adding risky media I/O.
See PHYS-11 through PHYS-17 in docs/TEST_MATRIX.md.

### M1.24 — scope of floppy and PCMCIA device support

For generic Physical device / Partition discovery and imaging,
exclude **only** `trackdisk.device` (floppy). Floppy-specific
handling is reserved for the future filesystem repair module.

`carddisk.device` **remains in the general device discovery and
picker**; its unusual PCMCIA SRAM characteristics will be documented
and tested later using the user's real Amiga and SRAM card. Do not
exclude it or infer full SCSI/IDE compatibility from the name.
`scsi.device` and `uaehf.device` continue to be used for the
immediate WinUAE investigation. The standalone diagnostic deliberately
only removes trackdisk from probeable candidates, so indexes must be
rechecked on each run.

The approved M1.24 GUI retains independent recessed fields for
Device name and Size. No automatic device probe has been connected.

### M1.25 — stable hardware query address (without UI integration)

The physical discovery list now excludes only trackdisk.device;
carddisk.device remains present and is reserved for later
device-specific validation using a physical PCMCIA SRAM card.
Filtering the floppy shifts candidate indices, so **do not assume**
an earlier \`--probe INDEX\` refers to the same unit.

The console-only diagnostic adds
\`--probe-driver NAME.device UNIT\`. It first snapshots the current
AmigaDOS registered device startup records, then requires an exact
match of both driver name and numerical unit against the validated
candidates before issuing any hardware request. No matching
candidate means no OpenDevice, TD_GETGEOMETRY or HD_SCSICMD.

The separate results remain **Device name** (SCSI INQUIRY
vendor/product) and **Size** (TD_GETGEOMETRY capacity in MiB),
with independent Unavailable states. The approved M1.24 picker
layout stays untouched, and GUI auto-probing remains disabled.
Synchronous DoIO currently has no transport timeout; this test
must be run only in WinUAE on a controlled, disposable disk image.

### M1.26 — separate identification and capacity query results

User's A1200/OS 3.1 WinUAE test reported the real identity
\`UAE-IDE rafael.hdf\` via HD_SCSICMD INQUIRY on
\`scsi.device\`, unit 0. Size was Unavailable from the original
geometry request.

The independent opt-in Shell probe now allocates a full IOExtTD
request for TD_GETGEOMETRY, logging command status, io_Error,
reported sector count and sector size. If no usable capacity is
returned, it attempts SCSI READ CAPACITY(10) (0x25) as a read-only
fallback. It calculates total bytes from (last LBA + 1) * block
size using 64-bit arithmetic, and reports Size in MiB. The CDB and
reply use allocated Chip RAM; if the 10-byte command returns
last LBA 0xFFFFFFFF, Size stays Unavailable until READ CAPACITY(16)
is added. An unsupported command never invents capacity.

Do not change the accepted M1.24 GUI layout: **Device name** comes
from INQUIRY vendor+product and **Size** from geometry/capacity.
The two recessed boxes remain independent. Device probing in the
GUI is not enabled, as its physical lists are still synthetic.
Trackdisk is excluded from generic disk discovery, while
carddisk remains a candidate for later PCMCIA SRAM testing.

### M1.27 — explicit user-driven physical Query in accepted M1.24 GUI

Physical device and Partition no longer use guessed synthetic
driver/unit combinations. Their first and second GadTools LISTVIEWs
are populated from a bounded private snapshot of registered
AmigaDOS FileSysStartupMsg driver/unit pairs (same basic pointer,
DosEnvec and BSTR checks proven by the M1.26 Shell diagnostic).
Duplicate physical units collapse; trackdisk.device is filtered out;
carddisk.device is retained. Opening either chooser performs DOS
list enumeration **only**, without OpenDevice or any device I/O.

The already accepted **Selected device / Selected partition** inset
now has an 80px **Query** button immediately to its right, in the
same row. Selecting or double-clicking an entry NEVER calls the
driver: only an IDCMP_GADGETUP event from this specific Query button
invokes the read-only hardware query for the currently verified
driver/unit tuple. The selection inset's text is clipped to make
room for Query. Query is disabled until a valid unit is selected.

Only after a deliberate Query click, Dolly calls TD_GETGEOMETRY,
HD_SCSICMD INQUIRY, and when geometry fails READ CAPACITY(10).
Each result occupies the already separate, recessed
**Device name** and **Size** fields. The buffer/CDB for SCSI
commands uses Chip RAM and each result is independently reported
as Unavailable if unsupported. Changing the selected device/unit
or mode invalidates all previous results. Resizing does not
re-issue queries and preserves the last queried display.

Partition mode preserves three columns but deliberately leaves the
partition list disabled and empty until real RDB/MBR partition
discovery exists; no fabricated partitions can be selected.
A physical unit can be committed without having pressed Query,
as choosing a device is distinct from probing it.
The main window shows the stable "driver.device / Unit N" identity
stored in the endpoint, instead of a transient list index.
There is still no disk imaging/writing action introduced.

**Safety limitation:** the user-initiated DoIO calls are currently
synchronous without a driver timeout, so the GUI may block if a
driver never responds. Test only in controlled WinUAE with a test
HDF before using real hardware, especially carddisk/SRAM.
No RTG or memory profiling yet.


### M1.28 — explicit Query gate and read-only RDB/MBR selection

**Double-click acceptance is entirely removed** from every picker
mode (Physical, Logical, Partition, File). A single selection only
updates the provisional highlighted row. The **Use selection** button
on the Physical device view stays disabled until an explicitly
invoked **Query** returned verified device identity and/or capacity.
When the driver/unit selection changes, that authorization and all
previous values are invalidated. Logical and File keep their
explicit Use selection workflow, without any double-click shortcut.

Partition mode uses the same manual Query button, but does **not**
enable Use selection after device identity alone. After successful
Query and capacity discovery, Dolly performs bounded read-only sector
reads to inspect **Amiga RDB** (RDSK plus checksummed PART chain) or
a simple **MBR** (four primary entries). It allows selecting a
partition only when the map is fully validated and one partition
is highlighted. RDB names from PART records populate the third
LISTVIEW, and MBR entries are labelled by ordinal number because
MBR primary entries have no volume-name field. Missing/invalid/
overlapping layouts leave the list empty and Use disabled.

The approved two inline recessed rows stay intact:
Selected device / Selected partition plus Query at upper-right,
followed by independent Device name and Size fields. **Partition**
adds a third, bottom recessed row, labelled Partition info, ahead
of the action buttons. It displays the selected RDB volume name,
capacity, DosType metadata (with FFS/OFS hint where applicable),
LowCyl and HighCyl, block size and sectors-per-block. For MBR
it shows size, type code, start LBA/count and logical block size;
cylinder range and volume name are absent in ordinary MBR and must
not be manufactured. Data are clipped visually to fit classic
640x256 but remain stored untruncated for the current selection.

The partition scanner reads only after a user clicks Query. It
uses CMD_READ on a verified driver/unit, a single 512-byte Chip
RAM sector buffer, and bounded up-to-16-block RDB discovery / 16
partition nodes. It checks RDB checksums, chains, device bounds
and overlap, and MBR signatures/bounds/overlap. The first version
supports 512-byte logical sectors and whole-disk RDB or simple
MBR primary partitions. **GPT, extended MBR, nested RDB inside
MBR 0x76, hybrid layouts, and devices with sector size other than
512 bytes are not yet supported**; never silently interpret them
as valid writable targets. This is a read-only preview, and no
imaging, formatting or filesystem modification operation is
added. Existing portable parsers remain the reference for later
more complete backend integration.

Hardware interactions are synchronous and currently lack an
I/O timeout. This branch is **not yet cross-compiled or tested
in WinUAE** and should be tested only with disposable HDF images
before enabling tests on real physical Amigas. Keep trackdisk
excluded; carddisk present but postpone its SRAM-specific
hardware tests. RTG and profiling tests are still deferred.

### M1.29 — persistent GadTools list selection and RDB sequence labels

GadTools LISTVIEW_KIND was created with GTLV_Selected but without
GTLV_ShowSelected, causing a short press/release highlight rather than a
persistent indication. The chooser now creates every LISTVIEW with
`GTLV_ShowSelected,NULL` and retains `GTLV_Selected` from picker
selection state through rebuilds and resize.

**Version distinction:** in GadTools V39+ (AmigaOS 3.x) this draws a
persistent highlight bar across the chosen row. On V37 (AmigaOS 2.04),
the selected item is instead shown in a separate read-only selection
display beneath the list. It is not equivalent to V39 highlighting.
No custom rendering or V39-only API is required.

Partition LISTVIEW now displays an ordinal prefix `1: `, `2: `, etc.
These are the positions in the validated, checksum-checked RDB PART
linked chain, not alphabetical order, cylinder order or RDB block number.
For MBR the same ordinal is the position among returned usable primary
partition entries; MBR labels already say `MBR n`.
The original disk label is held untouched in DollyNativePartition,
used by Selected partition, Partition info and the endpoint committed
to the main view. No disk I/O or partition parser logic changed.

Status: implementation submitted for compilation and WinUAE verification,
not yet tested by the assistant. Use the existing 640x256 A1200 / OS3.1
test HDF first; verify OS2.04 separately for the version-dependent
selection rendering. RTG remains deferred.


### M1.35 — Operation first, contextual Source/Destination pages

The main window retains its original GadTools Operation Cycle, but
displays it in the first control row, above Source and Destination.
The five existing demo operations are joined by a sixth visible
entry **Disk Vet (coming soon)**, a friendly face for the planned
DiskDoctor filesystem-first-aid project. Disk Vet needs Source only:
the Destination button is disabled, and Start is **always disabled**
because the backend is not implemented. Source may be Physical,
Partition, Logical or File (filesystem image); no repair is implied.

Each endpoint picker receives a mask of allowed modes, computed by
the portable function \`dolly_allowed_modes(operation, role,
source_kind, berserk)\`. These become a persistent NULL-terminated
\`GTCY_Labels\` array owned by that picker and an array mapping
**cycle positions** to stable \`DollyEndpointMode\` values.
No individual disabled entries are shown. The mapping is essential:
for Restore Image, Source Cycle position 0 = File (enum value 3).
A stored endpoint is preserved when Operation changes, but if its
kind does not match the new mode policy Start is disabled and a
message requests a new Source or Destination. Picker opens in
an allowed mode even if its previous selection is incompatible.

Normal policy:
- Scan: Source Physical/Partition/Logical; no Destination.
- Inventory: Source Physical/Partition/Logical/File; no Destination.
- Copy: Source any; Source Physical -> Destination Physical,
  Partition -> Partition, Logical -> Logical, File -> File.
- Restore Image: Source File; Destination Physical/Partition.
- Create Image: Source Physical/Partition/Logical; Destination File.
- Disk Vet: Source any; no Destination; Start always unavailable.

Destination button is disabled until a valid Source compatible
with the chosen Operation is committed. Operation Cycle is disabled
while either picker is open, avoiding policy changing mid-selection.
Double-click remains disabled in every picker. RDB scanning,
double-buffered LISTVIEW banks, explicit Query and separate Device
name/Size inset fields are unchanged.

M1.35 includes only **normal-mode** filtering; \`berserk_mode\` is
fixed to 0 in this GUI. The portable policy supports future Berserk
masks for exceptional combinations but it is not yet presented
as a switch and never grants real-media write authority. Current
Copy File -> File is a **selection/demo workflow only**, not an
implemented image conversion backend. See DOL-081–084 in the backlog.

Implementation prepared on a feature branch; Amiga cross-compilation
and WinUAE tests are required. RTG and RAM profiling remain deferred.

### M1.36 — final-label and compact main-window polish

On the main GadTools window (baseline 640×256) the first visible
control is now **Operation**, followed immediately by **Info**,
then **Source** and **Destination**. Removed the decorative in-client
heading "Dolly / Operation & devices" entirely, and reclaimed the
vertical row previously reserved for it. The three primary buttons
share the same calculated width (with matching 6px gaps):
**Sheep Counter**, **Sheep Inventory**, **Quit**. Sheep Inventory
is a disabled BUTTON_KIND placeholder and performs no action; it
must stay disabled even while an operation is running.

The remaining full-width action button is labelled **Start**.
Removed "demo"/"preview" from the visible Operation choices,
title bar, buttons, and Sheep Counter description. The genuine
operation backend remains a synthetic simulation (no real disk
copying/writes are enabled), regardless of display text.

Renamed **Blocks:** to **Progress:** and appended percentage in
parentheses to the progress display, e.g. `96 / 192 (50%)`.
The main count text and Sheep Counter progress use the same
OperationSession counters, but only the main view appends parentheses
because the Sheep Counter has its own separate percentage display.
The status inset now follows the Progress row closely without the
previous large gap.

This is a main-window-only UI revision. Existing Source and
Destination GadTools selection code, explicit Query, RDB/MBR,
double-buffered partition nodes, and contextual Cycle lists are
unchanged. Native cross-build and WinUAE layout verification pending.

### M1.37 — compact main window and output image format

The three-button row remains unchanged geometrically, but its middle
button (ID MAIN_INVENTORY_ID) now has an **empty label** and remains
permanently disabled. Inventory is already available as an Operation;
the reserved button is a future placeholder, not a duplicate feature.

Reduce the initial outer height of the Dolly main window from 213
to **195 pixels** while retaining the accepted Operation/Info/
Source/Destination/Progress positions and full recessed status panel.
The main minimum client-height guard is 170 pixels, accounting for
the status well ending at content offset 168. The window remains
resizable and still uses the measured WindowLimits minimum.

For **Create Image** only, when the selected Destination is a File,
the recessed path field appends a display-only suffix from the
committed endpoint image_format: (RAW), (HDF), (ADF), or (IFF DOLY).
Example: `backup.hdf (HDF)`. This is format metadata selected
in the Destination picker, never inferred from the extension.
The committed file path remains unchanged. If the field is too
narrow, trim its displayed path as needed to keep the suffix visible.
Other operations and Source formatting remain unchanged.

No changes to media I/O, operation eligibility or picker LISTVIEW
lifetime. Native build and WinUAE visual verification pending.

### M1.38 — Progress with Stop/Quit and two unused action placeholders

The upper action row retains three equally sized buttons with
6px gaps: **Sheep Counter**, a disabled blank button, and a second
disabled blank button in the former **Quit** position.

The full-width **Start** remains immediately below. The row labeled
**Progress:** now shows only a concise percentage, e.g. `0%`,
`50%`, `100%`, in a recessed 66px-wide TEXT_KIND field. No
block count or parentheses appear in the main view; the separate
Sheep Counter retains its own block count and percent indicator.
On the same row, to the **right** of the progress field, place
equal-width **Stop** and **Quit** BUTTON_KIND gadgets (Stop to the
left). Both are right aligned; each is 90px wide at typical
window widths and 70px at client widths below 360px. The client
minimum width remains unchanged.

**Stop is deliberately nonfunctional and disabled** at this
stage. Its sole future activation criterion is an explicit
operation capability for safely interruptible work. The current
`operation_supports_safe_stop()` stub returns false for all
operations, including synthetic progress; no cancellation event
handler is wired. Start/Stop safety cannot be inferred just from
a running-state flag. Quit retains its existing behavior.

The recessed main status panel remains below the Progress/Stop/Quit
row. The 195px initial main window height and the M1.37 output
image format suffix are unchanged. Main gadget pointers for Stop
and both blank placeholders are reset during resize and teardown
to prevent stale references.

No changes to source/destination pickers, partition-node bank
lifetime, Query, Disk Vet or real disk I/O. Cross-build and WinUAE
verification pending at user request.
