# Windows Qt Quick frontend

## Native lighttable

The toolbar's **观片台** opens a separate native Qt Quick window. `Lighttable`
owns the local board model, millimetre dimensions, private PNG snapshots and
versioned `Lighttable/board.json`. Import and composition run through
QtConcurrent; a completed import is flushed even when the application closes
before the worker's queued completion reaches the GUI. Unreadable/future board
documents are preserved and block new imports instead of being overwritten.

The editor sends its published, postprocessed preview image. Film Edge snapshots
use the actual edge format and strip/filed dimensions. Imported images use a
user-selected format and conservative aspect-based edge detection. This is a
nominal physical comparison, not a calibrated screen ruler. Scene textures use
1024-pixel thumbnails; the 3× loupe uses the cached image (up to 4096 pixels).
Each snapshot is independent of later photo edits. Removing/clearing deletes
only generated UUID assets, never the imported source files.

Drag, click/place, pointer-centred wheel zoom, pan, corner resize, rotation,
arrange, loupe, PNG save/copy and reopening the local board are supported.
The website's Three.js optical materials, depth of field and browser share
sheet are not reproduced. No website code, browser runtime or network storage
is included. See the Chinese [user guide](../README.md#观片台).

`spk_qt_lighttable` in CTest checks physical sizes, composed photo pixels,
no-overwrite, clipboard, persistence, close-during-import and invalid-record
preservation. The real editor UI gate is:

```powershell
./build/qt-native/windows_UI/spk_qt_history_ui_test.exe `
  ./build/qt-native/app/resources C:/fixtures/input.ARW `
  ./build/validation/lighttable-ui lighttable
```

It owns a RAW copy and checks the actual editor snapshot pixels, toolbar entry,
mouse drag, wheel, corner resize, click/place, shortcuts, rendered rotation and
PNG export. Offscreen validation does not test native file dialogs or physical
monitor colour management.

This is a native Qt Quick/C++ application using the existing synchronous
`spk_desktop_host`. There is no Python, Node, browser or subprocess renderer at
runtime. The upstream SwiftUI theme supplied the grey rails, centre canvas,
warm accent and bottom strip. The native rails follow the source's section
order and dense controls; this is an ongoing port, not a simplified redesign.

As of 2026-11-07 this is the Windows application mainline. The root CMake
build enables `SPEKTRALAB_BUILD_QT_UI` on Windows; engine-only build scripts
explicitly disable it. The Tauri frontend is retained as a reference and does
not participate in this runtime or package.

Image parity is not established against the latest Mac release. The existing
33 film/paper profiles match their counterparts in audited upstream main
`2ad7f8e`, but the Windows port still uses the earlier 1.3.3 core. The latest
CineStill stock/back-reflection model, RAW decoding and full ProPhoto display
chain differ. See [the dated source audit](PARITY-AUDIT.md); Windows workflow
tests are not evidence of Mac/Windows pixel equality.

The native workspace now supports folder import, a sampled logarithmic RGB
histogram of the displayed frame, persistent rail widths/collapse and strip
visibility, and drag/double-click rail resizing. Layout settings live in
`workspace.ini` under the application's local data directory. Test/snapshot
instances redirect them to the same isolated directory as their edit store.
Command-line positional RAW paths and a single folder are supported as well
as `--open`. No user RAW or old Tauri sidecar is overwritten. Old Tauri edits
are not automatically imported into the native edit store yet.

Film/paper compatibility is enforced: black-and-white films select Multigrade
when the previous paper is incompatible; returning to colour chooses a
compatible colour paper. Slides always scan directly and retain remembered
paper controls. The host uses the Mac application's Multigrade grade-2 neutral
pack and per-film grain multipliers without changing measured profiles.

Implemented: multi-file RAW import and drag/drop; independent per-photo
film/paper, decode, exposure, grain, halation, glare, film format and filter
settings; a navigator and selected-photo strip; free/ratio cropping; and
TIFF16, PNG8/16, JPEG8 export with JPEG quality and optional downsize. Batch
export processes selected photos serially, supports cancellation between files,
and skips existing files. Single export refuses overwrites. Output stays sRGB
with the bundled ICC profile. Select-all and synchronization copy supported
settings to selected photos, preserving their own crops by default.

Ctrl+O imports, Ctrl+E opens export options, Ctrl+0 fits and Ctrl+1 selects
physical 100%. Double-click also switches fit/100%. Changes update the preview
automatically. Wheel input zooms about the pointer without a modifier; left or
middle drag pans, including from fit view, with hand/grabbing cursors. High
resolution pixel wheel events are supported. Zoom is limited to 1%–1600%; small
images stay reachable when panned. Toolbar zoom preserves the canvas-centre
image point. These operations change the viewport without invoking the engine.
Layer 1 changes update the preview
automatically after a 180 ms settling delay. The filmstrip restores each
photo's wanted edits, including edits pending when the user switches away.
Edits automatically save under the application's local data directory (`Edits`)
and restore when the same photo is reopened. RAW files and their directories
are not modified. Save failures, incompatible records, changed sources and
conflicting writes from another app instance remain visible and preserve the
existing record. After a concurrent-write conflict, restart the application and
reopen the photo to load the other instance's saved settings. Undo/redo is per
photo, bounded to 64 in-memory steps, and a
slider drag is one step. Undo history itself is not persisted. Only the current
photo's timeline appears in the visible History section below Navigator; a
toolbar History button and View menu toggle it. Steps have adjustment names,
the current state is highlighted and future redo steps remain visible. Click
any retained step to restore it in one transaction; editing from an older
state retires the redo branch. Toolbar and menu undo/redo are available as well.
Numeric/text parameter entry releases focus after Enter commits it, so Ctrl+Z
then reaches photo history; active typing retains the text input's own undo.
Clicking the canvas also returns keyboard focus to the viewer. Only the current
photo is fully decoded/developed; unvisited
photos load camera-embedded thumbnails on demand, explicitly labelled RAW.
Opening a photo replaces this with its developed result. The list model updates
only the changed row, keeping scroll position. Small thumbnails are retained,
not one full RAW/GPU session per imported photo.

Without Film Edge, crop is post-render geometry and does not re-run the emulsion. Preview,
navigator and exports share the same rounded source-pixel edges. The first
Windows crop supports free drag, 1:1, 3:2, 4:3 and 16:9, clockwise/counterclockwise
quarter turns, horizontal/vertical flips and -45 to +45 degree straightening.
Straightening applies on release, uses original 16-bit samples and zooms to
fill the crop; quarter turns/flips are lossless. The crop editor uses the
unrotated source explicitly, while presets describe the final output ratio.
Masked full-photo strip thumbnails remain pending. The strip currently
shows the cropped output. See [DESIGN_ALIGNMENT.md](DESIGN_ALIGNMENT.md) for the
author's drawing transcription and upstream comparison.

Slide film is scanned directly, matching the upstream macOS film-stage rule.
The host resolves this from each profile's `info.type`, including Provia 100F,
Velvia 100, Ektachrome 100 and Kodachrome 64. Paper and paper-brightness controls
are disabled for slides; their previous settings return with negative film.
The colour model, profile data and shader maths are unchanged.

All engine work runs on one serial worker. Edits during a render are coalesced
into the newest settings; superseded results never replace the displayed frame
or newer controls. File dialogs pause pending updates until they close; opening
and exporting lock conflicting commands. A failed
current render retains the displayed frame and restores its controls; a retry
is available. Export captures that same immutable frame, including when a new
preview is pending, and pending edits resume after export succeeds or fails.
Closing during work waits for the current operation. A failed image-publication
allocation is reconciled against the displayed source/decode mode on the next
render.

Ctrl+Z undoes the current photo; Ctrl+Shift+Z / Ctrl+Y redo. Text editors retain
their own undo shortcuts. Synchronization is undoable on each recipient photo.
Batch export and file dialogs lock history commands along with other edits.
`--edit-store <absolute-directory>` overrides the persistent directory; all
automated tests inject an isolated directory, including `--self-test` and
`--snapshot`, so no test reads or writes the user's real edit store. Persistent
photo identity includes canonical path, size, modification time and first/last
64 KiB hash. Renamed/moved source files are not automatically matched yet.

## Build

### Source UI and process restoration

The left rail restores Navigator, Clipboard, Film, Film Edge, Date Back, Print,
Crop and Enlarger. The right rail offers Pre-Dev latitude, camera, film format,
scene placement, and Post-Dev white balance, exposure, five-channel monotone
curves and four-zone colour balance. Solve and scene placement call the engine.
Original and split compare use the retained decoded RAW, not a second film look.

Film Edge and Date Back pass the source wire fields to the existing Vulkan
pipeline. Edge geometry is applied inside the gate; preview/export must retain
the whole generated border instead of cropping it again in Qt. Camera white
balance uses the source LibRaw camera-matrix helper and re-decodes; expanded
highlight mode currently uses as-shot WB only. Date Back initializes from the
capture date where available. Windows uses the existing 5x7 glyph implementation,
so it is not CoreText pixel parity.

Enabling an edge holds the previous geometry, reshapes the crop about its centre
to the source gate ratio, and restores the held crop when disabled. A format
change or crop gesture stays constrained to that gate. Cropping uses the decoded
RAW as the guide. Film pitch uses the cropped gate's pixel dimensions, including
cached-negative reprints; otherwise the border is physically oversized. The
canvas comparison test reads rendered left/right pixels and covers enabling
split on an already-live node, the path that exposed a null-texture crash.

Layer 2 transcribes the Swift/Metal tone, WB, zone balance, vignette and
Fritsch-Carlson curve maths. Post-only edits reuse the held emulsion frame on
the serial worker and never re-run the film. A neutral grade bypasses exactly;
preview and file export share the same processed 16-bit pixels. Post edits
settle after 24 ms; Layer 1 remains 180 ms. The current engine boundary still
returns compressed sRGB: nonneutral Layer 2 converts those samples into encoded
ProPhoto for the maths and back to sRGB. This does not restore lost gamut, and
is not the Mac's complete wide-gamut/ColorSync pipeline. Interactive full-size
CPU processing can still be expensive; no fixed speed improvement is claimed.

Use the official Qt 6.11.3 Windows MinGW SDK with its MinGW-w64 13.1 compiler.
Do not link these Qt binaries with the earlier GCC 15 Windows build. Build into
an independent directory; the script changes only its own process PATH.
Use `-Fresh` to discard an existing CMake configuration before rebuilding.

```powershell
# From the repository root. The bootstrap uses curl and 7-Zip (or Windows
# bsdtar with 7z support), and checks pinned SHA256 for every archive/license.
./windows_UI/setup-qt.ps1
./windows_UI/build.ps1 -RunTests
./windows_UI/package.ps1 -OutputDirectory ./build/release-qt-native-v2
```

Local dependencies are in `build/deps/qt-6.11.3`, alongside Vulkan
headers, glslang and LibRaw used by the engine. `-DependencyDirectory` overrides
that location. The output `app/` is self-contained for this Windows machine;
keep the complete directory with its DLLs, QML plugins, resources and notices.
The Vulkan driver is supplied by Windows/the installed GPU driver.

CMake defaults to portable `build/deps/cmake-3.31.6-windows-x86_64/bin`;
`-CMakeDirectory` can point to another CMake 3.22+ installation. The Qt build
uses MinGW from the SDK and does not require MSVC or Windows SDK installation.
Qt archive pins were refreshed to the official 2026-10-22 Qt 6.11.3 rebuild;
official SHA1 files were checked before recording SHA256 hashes.

Packaging creates a new directory and refuses an existing destination. It
verifies executable and source fingerprints plus all 33 catalog profiles,
deploys only runtime files, removes unused optional control styles and QML
debugging plugins, and retains license texts and official JSON SBOMs. The SDK
retains alternate SBOM formats/source inventories. The measured compact
bundle is about 83.2 MiB unpacked / 32.5 MiB ZIP, versus the old V4 ZIP's
383.8 MiB. Rebuilds and compression versions may change these sizes.

`dependencies/qt-packages.json` lists the exact official archives and hashes;
`dependencies/qt-licenses.json` lists each official source license and hash.
`setup-qt.ps1 -VerifyOnly` verifies the existing archive/license cache and tool
versions without network or extraction. The initial download is about 278 MB.
This script does not install Vulkan headers, glslang, CMake or LibRaw; set those
up using the repository's Windows engine instructions first. It neither
downloads an unpinned "latest" SDK nor depends on a private local manifest.

For a parent build, enable the native RAW and desktop host targets and call
`add_subdirectory(windows_UI)` after `add_subdirectory(engine)`. The Qt frontend
links the existing `spk_desktop_host`; the macOS build is untouched. Resources
are synchronized on every build, including builds where the executable itself
does not need relinking. Deployment is an explicit build-script step.

## Validation

`spk_qt_history_ui_test <resources> <RAW> <new-output-directory> menus` exercises
actual editor dropdowns. The original delegates had 24 px rows but inherited
12 px padding on both vertical sides, leaving a zero-height text area. The
regression first reproduces that invisible-text failure; the fix uses 26 px
rows and 4 px vertical padding, explicit text colours and selected-row feedback.
The gate opens 11 menus / 50 options, checks labels and rendered text pixels,
chooses values by mouse/keyboard and verifies controller values and disabled
options. Evidence: `build/validation/qt-menu-workflow/menus-ui.json` and the
popup PNGs alongside it. Option data alone is not evidence that a popup draws.

`spk_qt_history_ui_test <resources> <RAW> <new-output-directory>` builds the
same Main.qml and controller in an isolated QtTest runner. It uses actual Qt
keyboard dispatch and mouse hits: commit a numeric parameter, Ctrl+Z,
Ctrl+Shift+Z and Ctrl+Y, preserve text-only undo while typing, click toolbar
buttons and past/future timeline rows. The old behavior was shown red with
`Ctrl+Z after numeric commit was swallowed by text focus`. Final evidence is
under `build/validation/qt-history-ui-final`. The RAW/editing workflow passed
108 checks including multi-step history jumps, per-photo isolation, branching,
in-flight renders, export agreement and restart behavior. QtTest is needed only
for this test runner and is explicitly rejected by the deployment script.

The first validated build used Qt 6.11.3 / MinGW 13.1 on an RTX 5070 Ti:
16 engine CTests passed, followed by the real 4688 x 7028 ARW scene test below.
The same decoded input produced byte-identical deterministic RGBA16 full and
print-edit outputs under this compiler and the existing GCC 15.2 baseline.
The Vulkan startup gate uses an exact product-residual reference because the
legacy MSVCRT `fmaf` returns incorrect residuals for some of the probe inputs;
the GPU comparison remains exact and also has fixed-bit regression vectors.

```powershell
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_BACKEND='software'
./build/qt-native/app/SpektraLabQt.exe --open C:/photos/supported.ARW `
  --reject C:/photos/unsupported-HE.NEF --self-test C:/SpektraLab-validation/new-qt-test
```

The output directory must not exist. The test drives actual QML/controller
operations and the native Vulkan engine, saves fit/100%/final PNG captures,
exports the displayed frame, checks negative-cache use, film/paper changes,
failed RAW retention, overwrite refusal with pending settings, single flight,
panning and physical-pixel scale, old-frame immutability, and injected display
publication failure recovery. It also checks automatic selection/brightness,
rapid edits before and during rendering, returning to the displayed settings,
all four slide films, negative/slide transitions, automatic decode changes and
reset. It exits nonzero on failure and writes JSON and scene captures.

`spk_qt_frame_output` is an additional CTest for shared crop geometry, actual
PNG/JPEG/TIFF pixels/depth/ICC, JPEG quality, resize, extension agreement,
atomic no-overwrite publication (including concurrent writers), and cleanup.
The explicit full-RAW library gate uses isolated copies of both photo inputs:

```powershell
# Use the same Qt/MinGW runtime PATH as build.ps1 and the SDK's plugin directory.
./build/qt-native/windows_UI/spk_qt_library_workflow_test.exe ./build/qt-native/app/resources `
  C:/photos/supported.ARW C:/photos/unsupported-HE.NEF `
  C:/SpektraLab-validation/new-library-test
```

It checks pending edits while switching photos, independent crops, synchronization,
preview/export pixel agreement, PNG16 and JPEG size, batch source/filename
agreement, selection locks, no-overwrite, cancellation and failed RAW retention.
The output directory must be new; missing fixtures are a hard failure.

`spk_qt_edit_store` adds strict document/version validation, atomic save failure,
source replacement (including before first save), concurrent-instance conflicts
and preservation of damaged/future records to CTest. `spk_qt_editing_workflow_test`
uses the same four arguments as the library gate and verifies history during
in-flight renders, geometry/PNG agreement, mirrored rotation direction,
destructor flushing of pending changes, recreation/reopening, per-photo history,
batch locks and visible persistence warnings. It owns copies of all inputs.

The captures are **offscreen scene captures**, not a live monitor screenshot.
Software scene display does not make the engine a CPU renderer. This gate does
not automate the native file dialogs or verify monitor profiles. At this phase
the host renders encoded sRGB and the GUI shows an 8-bit SDR texture. A QImage
sRGB tag is not a proof that the default Windows Qt swapchain applies a monitor
ICC transform. Wide-gamut, EDR/HDR, ColourSync-equivalent display validation,
perspective geometry, half-frame pairing, lens correction, DI export and
file-resolution soft proof are future work. TIFF and PNG16 retain 16-bit
pixels; PNG8/JPEG8 use 8-bit samples, and resizing is explicitly requested.

Nikon HE/HE* RAW remains explicitly unsupported by this LibRaw build. A failed
NEF open must leave the previous ARW and its session usable.

### V2 native gates (2026-11-08)

The final build passed 20/20 CTests, including the live-node split texture
regression and the physical pitch regression under cropped film edges (both
shown red before their fixes). The compact v2 package passed the full QML/RAW
scene test with a Windows-only PATH and a separately supplied offscreen plugin:
35 frame publications / 32 operations, including real curve/slider bindings,
original/split, edge/date-back captures and the existing film/geometry/error gates.
The native library workflow passed 151 checks, including background thumbnails
without model resets. The final colour workflow checks arbitrary-crop gate
constraints, square-format changes, crop edits under an edge, restoration of
the held crop, post-only frame reuse and exact preview/PNG agreement.

Evidence: `build/validation/qt-source-package-v2`, `qt-source-library-v3` and
`qt-source-colors-final`. The new Windows package was opened through the desktop
UI and its import and pre/post controls confirmed. The tests do not prove
physical monitor calibration or complete Mac pixel parity. V2 is 32.87 MiB ZIP /
83.88 MiB unpacked; 434 deployment files and all 435 ZIP file entries were
hash-verified. The source fingerprint and executable hash are in the manifest.

### Earlier v1 native gates (2026-11-07)

19 CTests passed with Qt 6.11.3 / MinGW 13.1. The full QML scene self-test
passed (30 frame publications / 27 completed operations), including all four
slide stocks in both decode modes, immutable-frame export, failure retention,
geometry, undo/redo and export UI. Repeat scene validation on the compact
package also passed with PATH restricted to Windows directories: no SDK DLLs,
compiler runtime from PATH, WebView2 or web server were available. Only the
offscreen platform plugin was provided separately for this test; the shipped
runtime uses its own Windows platform plugin. The rejected RAW fixture in this
run was deliberately malformed; this does not newly validate Nikon HE/HE*.
The real-RAW library workflow additionally passed 147 checks for per-photo
settings/crops, synchronization, displayed/exported pixel agreement, PNG16 and
JPEG options, batch locking/cancellation, no-overwrite and failed-item reporting.
Evidence is under `build/validation/qt-native-library`. The shipped Windows
window was also opened and its loaded RAW, histogram and editing controls were
confirmed through the native accessibility interface. The older Vite service
has been stopped; the Qt editor continues independently.

`spk_qt_color_workflow_test <resources> <RAW> <new-output-directory>` checks
Ektachrome -> Portra 800 -> four B&W films -> Portra 400 and folder import using
the native controller. All four B&W outputs had maximum 8-bit channel spread
1; colour was restored after leaving B&W. Evidence is under
`build/validation/qt-native-colors`. Interactive rendering still uses native
resolution; the Tauri preview/full scheduling policy has not been migrated.
