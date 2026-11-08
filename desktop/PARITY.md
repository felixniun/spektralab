# PARITY — the macOS app (`modern_UI/Spektrafilm`) → `desktop/`

What of the SwiftUI/Metal app this TypeScript (Tauri 2) port carries, area by
area. **ported** = the behaviour is here; **partial** = present with a stated
gap; **stub** = the controls are drawn and disabled with the reason;
**skipped** = deliberately not carried, with why.

Windows status (2026-10-29): Vulkan now supports scene latitude, Cineon DI,
film edge/overscan and the date back; the host capabilities enable those
sections. The contrast mask remains refused and hidden, and dual-photo
half-frame pairing is still omitted. `params.wire()` continues to omit any
feature the active host reports as unsupported. See `WINDOWS-IMPLEMENTATION.md`
for the tested scope and release gaps.

UI update (2026-11-07): editor panel resize grips now use the Mac ranges and
persist widths; fold buttons move between rail headers and the toolbar. The
filmstrip supports pointer/keyboard reordering with folder-order persistence
and newly added neighbours as in `FrameOrder.swift`. Export adds Viewer/Grid,
grid density, persisted recipe selection, selectable worklist previews and
accurate stopped-batch counts. The preview remains the sRGB canvas output,
not a file-resolution destination-space soft proof. These changes are source
and local frontend updates; a new native EXE awaits MSVC Build Tools.

| Swift area | port | status | notes |
|---|---|---|---|
| `SpektrafilmApp.swift` window, menus, shortcuts | `windows/menus.ts`, `windows/keys.ts`, `main.tsx` | ported | Native menu bar built in TS (localised). Ctrl for ⌘. Capture One mapping: Ctrl+O/E, Ctrl+Z (+Ctrl+Shift+Z/Ctrl+Y redo — the Mac has no redo), Ctrl+Shift+C/V settings, Ctrl+A, Ctrl+=/−/0, Ctrl+B, Ctrl+Shift+F/B, Ctrl+Alt+[ ]/R, V/H/C, ←/→, `,` `.`, Y, Space (original). Single keys are not accelerators (trap 32, below). Full screen is F11 (⌃⌘F on the Mac). |
| `Windows/TypingKeyGuard.swift` (trap 32) | `windows/keys.ts` `isTypingKey` + `keys.test.ts` | ported | A focused text field gets characters, arrows and delete; Ctrl/Alt chords, Enter/Esc/Tab still reach the shortcuts. |
| `Windows/EditorWindow.swift`, `TopBar.swift` | `windows/EditorWindow.tsx`, `TopBar.tsx` | ported | Native title bar; rail header fold buttons return to toolbar when collapsed. Overlay resize grips preserve the Mac default column positions and persist within Mac width limits; double-click resets widths. Bottom filmstrip tab remains available when folded. |
| `Windows/SettingsWindow.swift` | `windows/Dialogs.tsx` `SettingsDialog` | partial | General (language, interface scale 100/115/130 %, reset layout), Rendering (preview edge, crop re-maps, decouple effects, DI blue compensation — disabled while the DI is refused), Diagnostics (versions, GPU, unsupported features, log/sidecar folders, host diagnostics, restart). Memory, disk cache, log retention, diagnostic bundle, update check and Agents pages skipped (brief §6). An in-window dialog, not a second window (one webview = one session store). |
| `Windows/AboutWindow.swift` | `Dialogs.tsx` `AboutDialog` | ported | GPL + CC BY-SA attribution texts (copied from the Mac bundle by `scripts/copy-assets.mjs`). |
| `Windows/BootWindow.swift` | `HostFailure` banner/dialog | partial | No boot window; the canvas says "waiting for the engine", the failure dialog names the reason, shows the host's stderr tail and a Vulkan hint. |
| `Localization/Strings.swift` (264 keys, en + zh-Hans) | `i18n/en.json`, `zh-Hans.json` via `scripts/extract-strings.mjs` | ported | Generated, not hand-copied. Point-of-use `L(_, zh:)` pairs are `tz(en, zh)` and were translated where the port shows them. |
| `Theme/Theme.swift` | `styles/theme.css` | ported | Colours, two section-title sizes, rail metrics. System fonts (Segoe UI Variable / Noto Sans / Noto Sans CJK SC / Microsoft YaHei); nothing bundled. |
| `Model/Params.swift` (FilmParams, wire, delta, layers) | `shared/params.ts` + test | ported | Wire names pinned against the engine's `kFields` (`mock-host/schema-fields.json`, extracted from `engine/src/core/params.cpp`). Feature gate drops refused rows. |
| `Model/Adjustments.swift`, `CurveMath.swift`, `Shaders.metal` `layer2` | `shared/adjustments.ts`, `canvas/glRenderer.ts` | ported | One GLSL pass = Layer 2 + geometry + compare. The CPU twin (`layer2Pixel`) is what the export uses. Mid-grey pivot follows the encoding (sRGB on the canvas, ROMM for a ProPhoto file). Masks skipped (withdrawn on the Mac too: `FeatureFlags.masks = false`). |
| `Model/Geometry.swift` | `shared/geometry.ts` + test | ported | Same maps (`sourcePoint`, `outputPoint`, `uniform`, straighten/fit/resize/aspect locks). The test pins the GLSL transliteration to the model. |
| `Canvas/CropOverlay.swift`, crop tool | `canvas/Canvas.tsx` `CropOverlay` | partial | Handles, thirds, move/resize with aspect locks, straighten by slider and Ctrl-drag a line with a live guide. The photograph is shown level with the crop drawn rotated; screen-pixel gesture parity still needs UI verification. |
| `Canvas/Renderer.swift`, `MetalCanvasView`, `ViewportState` | `canvas/Canvas.tsx`, `glRenderer.ts` | ported | Fit / 100 % / zoom steps / wheel zoom about the pointer / pan (Hand, middle button, or drag when zoomed). Zoom is native px per device px, so 100 % is the frame's own pixels at any tier. Pan/zoom are hand-written pointer handling rather than d3-zoom: the view state has to be the session's (zoom pill, menu, navigator), and d3-zoom's transform would be a second owner of it. |
| `Canvas/CompareOverlay.swift` | `Canvas.tsx` compare line | ported | Split with a draggable line; Before = the host's embedded/fast decode preview. |
| tiers, `full` badge (§7.3) | `state/session.ts` scheduler | ported | Every edit renders `live` at the preview edge; the original-resolution render starts 400 ms after the last edit and is dropped if anything moved; badge `preview` → `full`. One in flight, sent-vs-wanted coalescing; print-only deltas use `reprint`. |
| histogram | `canvas/histogram.ts` (+ worker) | ported (fixed) | Counts the canvas's **output** (geometry + Layer 2), unlike the Mac's known defect (§7.7). |
| `Panels/Sections/NavigatorSection.swift` | `LeftRail.tsx` | ported | Shows the cropped, turned output (the same offscreen render as the histogram — trap 33), the visible rectangle when zoomed, click/drag to pan. |
| `ClipboardSection`, `Model/SettingsClipboard.swift` | `shared/clipboard.ts` + test, `LeftRail.tsx` | ported | Six groups (masks group omitted with masks). Copy / Paste (to picked) / Sync. |
| `FilmSection`, `PrintProfileSection`, `StockCatalog` | `LeftRail.tsx`, `StockList.tsx`, `shared/stocks.ts` | ported | Mac `StockCatalog.json` + film covers. Positive/Negative, Still/Cine, Digital (stub), No Print Profile, Print Effects, EDR. A slide film greys the papers with the reason. |
| `FilmEdgeSection`, `DateBackSection` | `LeftRail.tsx` | partial | Windows Vulkan renders the edge, gate, sprockets, carrier, numbering, fog/leaks and date imprint. The UI wires the available controls; edge lettering uses a 5×7 raster font instead of CoreText, and striped overscan remains unsupported. |
| `CropSection` | `LeftRail.tsx` | ported | Aspect (+ portrait), straighten, rotate/flip, output size readout. |
| `EnlargerSection` | `LeftRail.tsx` | ported | Brightness (stops), Y/M filter shifts with print-colour tracks, pre-flash. Pair scope skipped (no pairs). |
| `LatitudeSection` | `RightRail.tsx` | partial | The host *measures* (`scene_latitude` works); the plot shows the scene histogram over the medium's range with below/within/above. Separation ramp and pull-back overlay not drawn. |
| `CameraSection`, `WhiteBalanceRows`, `WhiteBalanceBoxes` | `RightRail.tsx`, `shared/whiteBalance.ts` | partial | Metering pill (Custom = meter off), Film Exposure + As Shot, Vignetting (Layer 2). Camera Temperature / Tint at decode with the two As Shot boxes (the Mac's rules, unit-tested) and presets in the section's "•••"; edits re-decode through `redecode`, debounced 350 ms, one undo step; RAW only. The neutral picker samples a 9×9 camera-RGB patch and uses LibRaw's camera matrix; it is disabled while Film Edge is active. **Lens Correction stays disabled**: LibRaw has no lens profiles. |
| `FilmFormatSection` (ParameterSections.swift) | `RightRail.tsx` | ported | Size (cine pills) / Side / Side Length + unit, effect toggles, anti-halation layer, decoupled strengths (Settings), sub-layer grain in the menu. Film format derived from the photograph's aspect. |
| `ScenePlacementSection`, `ToneMaskSection` | `RightRail.tsx` | partial | Scene Latitude fits placement against the measured negative range and exposes the fitted parameters; full overlay/ramp and screen interaction parity remain. Tone Mask is hidden as on the Mac (`FeatureFlags.toneMask = false`). |
| Post-Dev `RightSections.swift` | `RightRail.tsx`, `CurveEditor.tsx` | partial | White Balance, Exposure (8 sliders), Curve (5 channels, add/move/remove points, histogram behind), Color Balance as a hue/saturation disc + three sliders per zone (the Mac's `ColorWheel` has more affordances). |
| `Panels/Filmstrip.swift`, `CropMaskedThumbnail` | `windows/Filmstrip.tsx`, `shared/surfaces.ts`, `shared/frameOrder.ts` + tests | ported | Whole photograph, turned/flipped, outside crop under an 82 % mask; live geometry for open frame, saved for rest. Click / Ctrl-click picks. Pointer drag and Alt+arrows reorder without importing files or changing the canvas selection; folder order persists and preserves new neighbours. |
| `Model/Session.swift` select / develop / undo / save | `state/session.ts` | ported | Undo (60 steps, 0.5 s coalescing) + redo. Host restart re-opens the frame from its sidecar. |
| `Model/Sidecar.swift` | `shared/sidecar.ts` + test, `src-tauri/src/files.rs` + test | ported | `<app data>/Sidecars/<file>-<sha256(path)[:16]>.spektra.json`, Mac JSON shape, unknown keys (masks, heldCrop…) carried through, neighbour migration, moved-file fingerprint (inode on Linux; not on Windows). |
| `Import/*` (decode, ThumbnailCache, FramePipeline, disk cache) | host `thumbnail`/`open` | partial | Decoding is the host's (LibRaw / raster). Thumbnails cached in memory per folder; no disk cache. HEIC/EXR and general input-ICC conversion remain unimplemented. |
| open folder / files / drop / open-with / single instance | `platform/*`, `files.rs`, `lib.rs` | partial | RAW + TIFF/JPEG/PNG. A second launch hands its paths to the running window. HEIC/EXR are not accepted. |
| `Export/ExportPage.swift`, `ExportRecipe.swift`, `Exporter.swift` | `export/*` + test | partial | Recipes (4 built-ins, editable, selected recipe persisted), location, overwrite policy, naming tokens, format/depth/space/quality/long edge, batch, Stop between frames, .cube and DI render. Viewer/Grid with column count, selectable previews and actual completed/written/skipped/failed counts. Cineon-file export workflow, file-resolution destination-space proof and Open With remain incomplete. |
| trap 33 surface agreement | `shared/surfaces.test.ts`, `geometry.test.ts` | ported | Crop + quarter turn: export pixels = shader map; filmstrip outline = canvas output corners. |
| `Agent/`, `Updates/`, `Diagnostics/` bundle & memory | — | skipped | Brief §6. Logging is `tauri-plugin-log` (rotating, 4 MB × 5) in the OS log dir; the host's stderr goes there too. |
| `Model/HalfFramePair.swift`, `PairSection`, `PairComposer` | — | skipped | The engine can render two half-gates in a film-edge layout; selecting two source photos, independent pair edits, `.spektrapair` save/reopen and pair export are not implemented. |
| `Model/Mask.swift`, `MasksSection` | — | skipped | Withdrawn on the Mac as well. |
| `Canvas/ColourManagement.swift`, soft proof | host transforms + `canvas/glRenderer.ts` | partial | Export transforms from ProPhoto at write time and embeds the target ICC. Canvas/navigation/histogram still use the sRGB preview path; P3 canvas and display-profile awareness are not complete. |
