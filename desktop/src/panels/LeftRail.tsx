// LeftRail.tsx — "Film and Print" (`Panels/LeftPanel.swift`):
// Navigator · Settings Clipboard · Film · Film Edge · Date Back · Print · Crop
// · Enlarger. Everything here runs in the engine except Crop (geometry, one
// draw).

import { useEffect, useRef } from 'react';
import { NumberField, ScrubSlider, Section, ToggleRow, PillMenu, Checkbox, signed } from '../controls/controls';
import { FlipH, FlipV, RotateLeft, RotateRight, SidebarLeft } from '../controls/icons';
import { settingsStore } from '../state/settings';
import { t, tz, useI18n } from '../i18n';
import {
  copySettings,
  pasteSettings,
  setGeometry,
  setPages,
  setParams,
  selectFilmStock,
  selectPrintStock,
  syncSettings,
  useSession,
  zoomToFit,
  sessionStore,
  setView,
} from '../state/session';
import { CLIPBOARD_GROUPS, type ClipboardGroup } from '@shared/clipboard';
import {
  CROP_ASPECT_PICKER,
  type CropAspect,
  aspectCanonical,
  aspectHasOrientation,
  aspectIsPortrait,
  aspectLabel,
  aspectTransposed,
  outputSize,
  straightened,
  turned,
  withAspect,
} from '@shared/geometry';
import { clamp } from '@shared/params';
import { useOutput } from '../canvas/output';
import { StockList } from './StockList';
import { unsupportedReason } from './gate';

export function LeftRail() {
  const { t: tr } = useI18n();
  return (
    <aside className="rail left" data-testid="left-rail">
      <div className="rail-header">{tr('railFilmAndPrint')}<button className="icon-btn rail-collapse" title={tr('helpDevelopRailHide')} onClick={() => settingsStore.getState().set('leftCollapsed', true)}><SidebarLeft /></button></div>
      <div className="hairline" />
      <div className="rail-scroll">
        <NavigatorSection />
        <ClipboardSection />
        <FilmSection />
        <FilmEdgeSection />
        <DateBackSection />
        <PrintSection />
        <CropSection />
        <EnlargerSection />
      </div>
    </aside>
  );
}

// ---------------------------------------------------------------- Navigator

function NavigatorSection() {
  const out = useOutput();
  const view = useSession((s) => s.view);
  const canvasPx = useSession((s) => s.canvasPx);
  const native = useSession((s) => s.nativeSize);
  const geometry = useSession((s) => s.sidecar.geometry);
  const o = native ? outputSize(geometry, native) : null;
  // The part of the output the canvas shows, in output uv.
  const vis =
    o && !view.fit
      ? (() => {
          const w = Math.min(1, canvasPx.width / (o.width * view.zoom));
          const h = Math.min(1, canvasPx.height / (o.height * view.zoom));
          return { x: clamp(view.cx - w / 2, 0, 1 - w), y: clamp(view.cy - h / 2, 0, 1 - h), w, h };
        })()
      : null;
  const pan = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!o || !out) return;
    const r = e.currentTarget.getBoundingClientRect();
    // The output is letterboxed inside the well (object-fit: contain).
    const k = Math.min(r.width / out.width, r.height / out.height);
    const ox = (r.width - out.width * k) / 2;
    const oy = (r.height - out.height * k) / 2;
    const u = clamp((e.clientX - r.left - ox) / (out.width * k), 0, 1);
    const v = clamp((e.clientY - r.top - oy) / (out.height * k), 0, 1);
    const s = sessionStore.getState().view;
    setView({ fit: false, zoom: s.fit ? Math.max(1, sessionStore.getState().fitZoom * 2) : s.zoom, cx: u, cy: v });
  };
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    const ctx = c.getContext('2d')!;
    if (!out) {
      ctx.clearRect(0, 0, c.width, c.height);
      return;
    }
    c.width = out.width;
    c.height = out.height;
    ctx.putImageData(new ImageData(new Uint8ClampedArray(out.data), out.width, out.height), 0, 0);
  }, [out]);
  return (
    <Section id="navigator" title={t('sectionNavigator')}>
      <div className="navigator" data-testid="navigator" onPointerDown={pan} onPointerMove={(e) => e.buttons === 1 && pan(e)}>
        <canvas ref={ref} />
        {vis && out && <NavigatorBox vis={vis} aspect={out.width / out.height} />}
        <button className="navigator-fit" onClick={zoomToFit} title={tz('Fit (,)', '适合窗口（,）')}>
          {view.fit ? t('helpFit') : `${Math.round(view.zoom * 100)} %`}
        </button>
      </div>
    </Section>
  );
}

function NavigatorBox({ vis, aspect }: { vis: { x: number; y: number; w: number; h: number }; aspect: number }) {
  // Drawn in the well's own box: the picture's letterbox is computed by CSS
  // with a percentage-based inner frame of the same aspect.
  return (
    <div style={{ position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', pointerEvents: 'none' }}>
      <div style={{ position: 'relative', aspectRatio: String(aspect), maxWidth: '100%', maxHeight: '100%', width: aspect >= 203 / 183 ? '100%' : 'auto', height: aspect >= 203 / 183 ? 'auto' : '100%' }}>
        <div
          style={{
            position: 'absolute',
            left: `${vis.x * 100}%`,
            top: `${vis.y * 100}%`,
            width: `${vis.w * 100}%`,
            height: `${vis.h * 100}%`,
            border: '1px solid var(--accent)',
          }}
        />
      </div>
    </div>
  );
}

// ---------------------------------------------------------------- Clipboard

const GROUP_LABEL: Record<ClipboardGroup, () => string> = {
  filmAndPaper: () => t('clipFilmAndPaper'),
  exposure: () => t('clipExposure'),
  whiteBalance: () => t('clipWhiteBalance'),
  filmEffects: () => t('clipFilmEffects'),
  printEffects: () => t('clipPrintEffects'),
  scenePlacement: () => t('clipScenePlacement'),
};

function ClipboardSection() {
  const groups = useSession((s) => s.clipboardGroups);
  const clip = useSession((s) => s.clipboard);
  const picked = useSession((s) => s.picked);
  const selection = useSession((s) => s.selection);
  const others = picked.filter((p) => p !== selection).length;
  const toggle = (g: ClipboardGroup) =>
    setPages({ clipboardGroups: groups.includes(g) ? groups.filter((x) => x !== g) : CLIPBOARD_GROUPS.filter((x) => x === g || groups.includes(x)) });
  return (
    <Section id="clipboard" title={t('sectionClipboard')}>
      {CLIPBOARD_GROUPS.map((g) => (
        <div className="row toggle-row" key={g}>
          <span className="row-label">{GROUP_LABEL[g]()}</span>
          <Checkbox on={groups.includes(g)} onChange={() => toggle(g)} label={GROUP_LABEL[g]()} />
        </div>
      ))}
      <div className="row" style={{ gap: 8 }}>
        <button className="btn" disabled={!selection || !groups.length} onClick={copySettings} title={t('clipCopyHelp')}>
          {t('clipCopy')}
        </button>
        <button className="btn" disabled={!clip || !selection} onClick={() => void pasteSettings()}>
          {others > 0 ? t('clipPasteTo').replace('%d', String(others + 1)) : t('clipPaste')}
        </button>
        <button className="btn" disabled={!selection || others === 0 || !groups.length} onClick={() => void syncSettings()}>
          {others > 0 ? t('clipSyncTo').replace('%d', String(others)) : t('clipSync')}
        </button>
      </div>
      <div className="caption">{clip ? tz(`Holds: ${clip.sourceName} · ${clip.groups.length} groups`, `内容：${clip.sourceName} · ${clip.groups.length} 组`) : t('clipEmpty')}</div>
    </Section>
  );
}

// --------------------------------------------------------------------- Film

function FilmSection() {
  const catalog = useSession((s) => s.catalog);
  const film = useSession((s) => s.sidecar.params.filmStock);
  const rows = catalog.filmGroups().flatMap((g) => [
    { id: '__g' + g.title, name: g.title === 'Positive' ? t('filmGroupPositive') : t('filmGroupNegative'), header: true },
    ...g.films.map((f) => ({ id: f.id, name: f.name, cine: f.use === 'cine', cover: catalog.coverURL(f.id) })),
  ]);
  return (
    <Section
      id="film"
      title={t('sectionFilm')}
      menu={[
        {
          label: t('helpUseFilmPaper'),
          disabled: !catalog.stock(film)?.targetPrint,
          onSelect: () => {
            const tp = catalog.stock(film)?.targetPrint;
            if (tp) setParams((p) => ({ ...p, printStock: tp, scanFilm: false }));
          },
        },
      ]}
    >
      <StockList rows={rows} selected={film} onSelect={selectFilmStock} testId="film-list" />
    </Section>
  );
}

// --------------------------------------------------------- Film Edge, Date Back

function FilmEdgeSection() {
  const gate = useSession((s) => s.gate);
  const fe = useSession((s) => s.sidecar.params.filmEdge);
  const reason = unsupportedReason('overscan', gate.overscan);
  return (
    <Section id="filmEdge" title={t('sectionFilmEdge')} unsupported={reason}>
      <ToggleRow label={t('sectionFilmEdge')} on={fe.active} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, active: v } }))} />
      <PillMenu
        label={t('edgeFormat')}
        value={fe.format}
        options={['135', '135_half', '135_xpan', '120_645', '120_6x6', '120_6x7', '120_6x8', '120_6x9', '120_6x12', '120_6x17'].map((v) => ({ value: v, label: v.replace('_', ' ') }))}
        onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, format: v, pair: v === '135_half' ? p.filmEdge.pair : false } }))}
      />
      <PillMenu
        label={t('edgeView')}
        value={fe.view}
        options={[
          { value: 'strip', label: t('edgeViewStrip') },
          { value: 'filed', label: t('edgeViewFiled') },
        ]}
        onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, view: v } }))}
      />
      <PillMenu label={t('edgeGate')} value={fe.gate} options={[
        { value: 'auto', label: t('edgeGateAuto') },
        { value: 'square', label: t('edgeGateSquare') },
        { value: 'rounded', label: t('edgeGateRounded') },
        { value: 'eared', label: t('edgeGateEared') },
        { value: 'shouldered', label: t('edgeGateShouldered') },
        { value: 'kicked', label: t('edgeGateKicked') },
      ]} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, gate: v } }))} />
      <PillMenu label={t('edgeHoles')} value={fe.holes} options={[
        { value: 'white', label: t('edgeHolesWhite') }, { value: 'black', label: t('edgeHolesBlack') },
      ]} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, holes: v } }))} />
      <PillMenu label={t('edgeCarrier')} value={fe.carrier} options={[
        { value: 'none', label: t('edgeCarrierNone') },
        { value: 'black', label: t('edgeCarrierBlack') },
        { value: 'open', label: t('edgeCarrierOpen') },
      ]} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, carrier: v } }))} />
      <ToggleRow label={t('edgePair')} on={fe.pair} disabled={fe.format !== '135_half'} reason={t('edgePairNeedsHalf')}
        onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, pair: v } }))} />
      <div className="row">
        <span className="row-label">{t('edgeFrameNumber')}</span>
        <NumberField value={fe.frameNumber} decimals={0} label={t('edgeFrameNumber')} width={64}
          onCommit={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, frameNumber: clamp(Math.round(v), 0, 99) } }))} />
        <span className="caption">0 = {t('edgeFrameNumberAuto')}</span>
      </div>
      <TextSetting label={t('edgeText')} value={fe.edgeText}
        onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, edgeText: v } }))} />
      <div className="row">
        <span className="row-label">{t('edgeFNumber')}</span>
        <NumberField value={fe.fNumber} decimals={1} label={t('edgeFNumber')} width={64}
          onCommit={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, fNumber: clamp(v, 0, 64) } }))} />
      </div>
      <ScrubSlider label={t('edgeFog')} value={fe.fog} range={[0, 4]} zero={1} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, fog: v } }))} />
      <ScrubSlider label={t('edgeLeaks')} value={fe.leaks} range={[0, 4]} onChange={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, leaks: v } }))} />
      <div className="row">
        <span className="row-label">{t('edgeCameraSeed')}</span>
        <NumberField value={fe.cameraSeed} decimals={0} label={t('edgeCameraSeed')} width={84}
          onCommit={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, cameraSeed: clamp(Math.round(v), 0, 2147483647) } }))} />
      </div>
      <div className="row">
        <span className="row-label">{t('edgeFrameSeed')}</span>
        <NumberField value={fe.frameSeed} decimals={0} label={t('edgeFrameSeed')} width={84}
          onCommit={(v) => setParams((p) => ({ ...p, filmEdge: { ...p.filmEdge, frameSeed: clamp(Math.round(v), 0, 2147483647) } }))} />
      </div>
    </Section>
  );
}

function DateBackSection() {
  const gate = useSession((s) => s.gate);
  const d = useSession((s) => s.sidecar.params.dateBack);
  const fe = useSession((s) => s.sidecar.params.filmEdge);
  const reason = unsupportedReason('date_imprint', gate.dateImprint);
  const set = (patch: Partial<typeof d>) => setParams((p) => ({ ...p, dateBack: { ...p.dateBack, ...patch } }));
  return (
    <Section id="dateBack" title={t('sectionDateBack')} unsupported={reason}>
      <ToggleRow label={t('dateImprint')} on={d.active} onChange={(v) => set({ active: v })} />
      <PillMenu
        label={t('dateFace')}
        value={d.face}
        options={[
          { value: 'lcd', label: 'LCD' },
          { value: 'dots', label: t('dateFaceDots') },
          { value: 'data', label: t('dateFaceData'), disabled: !fe.active },
        ]}
        onChange={(v) => set({ face: v })}
      />
      <PillMenu
        label={t('dateCorner')}
        value={d.corner}
        options={['br', 'bl', 'tr', 'tl'].map((v) => ({ value: v, label: v.toUpperCase() }))}
        onChange={(v) => set({ corner: v })}
      />
      <PillMenu label={t('datePlacement')} value={d.placement} options={[
          { value: 'frame', label: t('dateInFrame') }, { value: 'rebate', label: t('dateBetween'), disabled: !fe.active },
        ]} onChange={(v) => set({ placement: v })} />
      {!fe.active && <PillMenu label={t('dateCamera')} value={d.camera ?? '135'} options={[
        { value: '135', label: '135' }, { value: '135_half', label: '135 Half' },
      ]} onChange={(v) => set({ camera: v })} />}
      <TextSetting label={t('dateText')} value={d.customText ?? d.text} maxLength={24}
        onChange={(v) => set({ customText: v })} />
      {fe.active && fe.pair && <TextSetting label={t('dateSecondText')} value={d.textB} maxLength={24}
        onChange={(v) => set({ textB: v })} />}
      <ToggleRow label={t('dateWithData')} on={d.withData} onChange={(v) => set({ withData: v })} />
      {d.withData && <TextSetting label={t('dateDataText')} value={d.dataText} maxLength={40}
        onChange={(v) => set({ dataText: v })} />}
      {d.placement === 'frame' && d.face !== 'data' && <>
        <ScrubSlider label={t('dateInsetX')} value={d.insetXMM} range={[0, 30]} zero={3} format={(v) => `${v.toFixed(1)} mm`}
          onChange={(v) => set({ insetXMM: v })} />
        <ScrubSlider label={t('dateInsetY')} value={d.insetYMM} range={[0, 30]} zero={2.4} format={(v) => `${v.toFixed(1)} mm`}
          onChange={(v) => set({ insetYMM: v })} />
      </>}
      <ScrubSlider label={t('dateSize')} value={d.size} range={[0.4, 3]} zero={1} onChange={(v) => set({ size: v })} />
      <ScrubSlider label={t('dateBrightness')} sublabel={t('dateStops')} value={d.brightnessEV} range={[-2, 8]} zero={3.5} onChange={(v) => set({ brightnessEV: v })} />
    </Section>
  );
}

function TextSetting(p: { label: string; value: string; onChange: (v: string) => void; maxLength?: number }) {
  return (
    <div className="row">
      <span className="row-label">{p.label}</span>
      <input className="unit-field" style={{ flex: 1, textAlign: 'left', padding: '0 6px' }} type="text"
        value={p.value} maxLength={p.maxLength} aria-label={p.label} onChange={(e) => p.onChange(e.target.value)} />
    </div>
  );
}

// -------------------------------------------------------------------- Print

const POSITIVE_ID = '__positive';
const DIGITAL_ID = '__digital';

function PrintSection() {
  const catalog = useSession((s) => s.catalog);
  const p = useSession((s) => s.sidecar.params);
  const gate = useSession((s) => s.gate);
  const positive = catalog.isPositive(p.filmStock);
  const positiveReason = t('reasonPositiveFilmDisablesPaper');
  const diReason = unsupportedReason('digital_intermediate', gate.digitalIntermediate);
  const rows = [
    ...catalog.paperGroups().flatMap((g) =>
      g.papers.length
        ? [
            { id: '__g' + g.title, name: g.title === 'Still' ? t('printGroupStill') : t('printGroupCine'), header: true },
            ...g.papers.map((s) => ({ id: s.id, name: s.name, cine: s.use === 'cine', disabled: positive || !catalog.canPrint(p.filmStock, s.id), reason: positive ? positiveReason : tz('Colour film requires colour paper; black-and-white film requires black-and-white paper.', '彩色胶片使用彩色相纸，黑白胶片使用黑白相纸。') })),
          ]
        : [],
    ),
    { id: '__gDigital', name: t('printGroupDigital'), header: true },
    { id: DIGITAL_ID, name: t('printDigitalIntermediate'), disabled: positive || !!diReason, reason: diReason ?? positiveReason },
    { id: '__gPositive', name: tz('Positive', '正片'), header: true },
    { id: POSITIVE_ID, name: t('printNone') },
  ];
  const selected = p.scanFilm || positive ? POSITIVE_ID : p.digitalIntermediate && gate.digitalIntermediate ? DIGITAL_ID : p.printStock;
  return (
    <Section id="print" title={t('sectionPrint')}>
      <StockList
        rows={rows}
        selected={selected}
        testId="paper-list"
        onSelect={(id) => {
          if (id === POSITIVE_ID) setParams((q) => ({ ...q, scanFilm: true, digitalIntermediate: false }));
          else if (id === DIGITAL_ID) setParams((q) => ({ ...q, scanFilm: false, digitalIntermediate: true }));
          else selectPrintStock(id);
        }}
      />
      <ToggleRow label={t('printEffects')} on={p.printEffects} onChange={(v) => setParams((q) => ({ ...q, printEffects: v }))} />
      <ToggleRow
        label={t('printEDR')}
        sublabel={t('statusEDRScope')}
        on={p.extendedDynamicRange}
        disabled={p.scanFilm || positive}
        reason={t('reasonEDRDisabledInScanFilm')}
        onChange={(v) => setParams((q) => ({ ...q, extendedDynamicRange: v }))}
      />
    </Section>
  );
}

// --------------------------------------------------------------------- Crop

function CropSection() {
  const g = useSession((s) => s.sidecar.geometry);
  const native = useSession((s) => s.nativeSize);
  const size = native ?? { width: 3000, height: 2000 };
  const out = native ? outputSize(g, native) : null;
  const portrait = aspectIsPortrait(g.aspect);
  return (
    <Section
      id="crop"
      title={t('sectionCrop')}
      onReset={() => setGeometry(() => ({ ...g, crop: { x: 0, y: 0, width: 1, height: 1 }, angle: 0, aspect: 'free', intendedSize: null, lockedRatio: null }))}
      resetHelp={t('helpResetCrop')}
      menu={[
        { label: t('helpCropWholeFrame'), onSelect: () => setGeometry((q) => ({ ...q, crop: { x: 0, y: 0, width: 1, height: 1 }, intendedSize: null })) },
        { label: t('helpStraightenZero'), onSelect: () => setGeometry((q) => straightened(q, 0, size)) },
      ]}
    >
      <div className="row">
        <span className="row-label">{t('cropAspect')}</span>
        <PillMenu<CropAspect>
          fill
          value={aspectCanonical(g.aspect)}
          options={CROP_ASPECT_PICKER.map((a) => ({ value: a, label: a === 'free' ? t('cropAspectFree') : a === 'original' ? t('cropAspectOriginal') : aspectLabel(a) }))}
          onChange={(a) => setGeometry((q) => withAspect(q, portrait && aspectHasOrientation(a) ? aspectTransposed(a) : a, size))}
          testId="crop-aspect"
        />
        <Checkbox
          on={portrait}
          disabled={!aspectHasOrientation(g.aspect)}
          label={tz('Portrait', '竖向')}
          onChange={() => setGeometry((q) => withAspect(q, aspectTransposed(q.aspect), size))}
        />
      </div>
      <ScrubSlider
        label={t('cropStraighten')}
        sublabel={t('cropStraightenUnit')}
        value={g.angle}
        range={[-45, 45]}
        snap={0.5}
        format={(v) => signed(1)(v) + '°'}
        onChange={(v) => setGeometry((q) => straightened(q, v, size), { coalesce: true })}
        testId="straighten"
      />
      <div className="row">
        <span className="row-label">{t('cropRotate')}</span>
        <button className="icon-btn" title={tz('Rotate Left (Ctrl+Alt+[)', '向左旋转（Ctrl+Alt+[）')} onClick={() => setGeometry((q) => turned(q, -1))}>
          <RotateLeft />
        </button>
        <button className="icon-btn" title={tz('Rotate Right (Ctrl+Alt+])', '向右旋转（Ctrl+Alt+]）')} onClick={() => setGeometry((q) => turned(q, 1))} data-testid="rotate-right">
          <RotateRight />
        </button>
        <button className="icon-btn" title={tz('Flip Horizontally', '水平翻转')} onClick={() => setGeometry((q) => ({ ...q, flipH: !q.flipH }))}>
          <FlipH />
        </button>
        <button className="icon-btn" title={tz('Flip Vertically', '垂直翻转')} onClick={() => setGeometry((q) => ({ ...q, flipV: !q.flipV }))}>
          <FlipV />
        </button>
      </div>
      {out && (
        <div className="caption num" data-testid="crop-size">
          {out.width} × {out.height} · {((out.width * out.height) / 1e6).toFixed(1)} MP
        </div>
      )}
    </Section>
  );
}

// ----------------------------------------------------------------- Enlarger

function EnlargerSection() {
  const p = useSession((s) => s.sidecar.params);
  return (
    <Section
      id="enlarger"
      title={t('sectionEnlarger')}
      onReset={() => setParams((q) => ({ ...q, printBrightnessStops: 0, yFilterShift: 0, mFilterShift: 0, preflashExposure: 0 }))}
      resetHelp={t('helpResetEnlarger')}
    >
      <ScrubSlider
        label={tz('Brightness', '亮度')}
        sublabel={tz('stops', '档')}
        value={p.printBrightnessStops}
        range={[-3, 3]}
        snap={0.25}
        format={signed(2)}
        onChange={(v) => setParams((q) => ({ ...q, printBrightnessStops: v }))}
        testId="enlarger-brightness"
      />
      <ScrubSlider
        label={tz('Yellow', '黄')}
        sublabel={tz('→ blue', '→ 蓝')}
        value={p.yFilterShift}
        range={[-1, 1]}
        snap={0.05}
        format={signed(2)}
        gradient="linear-gradient(90deg,#b8a860,#8a8a8a,#6f7fb0)"
        onChange={(v) => setParams((q) => ({ ...q, yFilterShift: v }))}
      />
      <ScrubSlider
        label={tz('Magenta', '品红')}
        sublabel={tz('→ green', '→ 绿')}
        value={p.mFilterShift}
        range={[-1, 1]}
        snap={0.05}
        format={signed(2)}
        gradient="linear-gradient(90deg,#b07cae,#8a8a8a,#7ca87c)"
        onChange={(v) => setParams((q) => ({ ...q, mFilterShift: v }))}
      />
      <ScrubSlider
        label={t('enlargerPreflash')}
        sublabel="×100"
        value={p.preflashExposure * 100}
        range={[0, 3]}
        snap={0.25}
        disabled={!p.printEffects}
        reason={t('reasonPrintEffectsOff')}
        onChange={(v) => setParams((q) => ({ ...q, preflashExposure: clamp(v / 100, 0, 0.03) }))}
      />
    </Section>
  );
}

void sessionStore;
