// ExportPage.tsx — the export page (`Export/ExportPage.swift`, drawn to
// `reference_layout/Export_Page`): recipes on the left, the frame in the
// middle, the batch (the picked frames) on the right.
//
// **The batch holds the frame** (AGENTS.md trap 34): the exporter writes
// whatever frame is open, so while `batchExporting` is set every way a person
// can change the open frame is a no-op (`click`, `openPaths`, undo, the keys,
// the editor takes no hits). The run's own door is `select`. Stop cancels
// between frames; a frame being written is finished whole.

import { useEffect, useRef, useState } from 'react';
import { Checkbox, NumberField, PillMenu, ScrubSlider, Section } from '../controls/controls';
import { t, tz } from '../i18n';
import { platform, baseName } from '../platform';
import { select, sessionStore, setPages, useSession } from '../state/session';
import { settingsStore, useSettings } from '../state/settings';
import { Thumb } from '../windows/Filmstrip';
import { GEOMETRY_DEFAULT } from '@shared/geometry';
import { useOutput } from '../canvas/output';
import { type ExportRecipe, DEFAULT_RECIPES, NAME_TOKENS, type NameToken, decodeRecipes, extensionOf } from './recipes';
import { exportCube, exportFrame, hostWrites, outputFor, targetFor, type ExportOutcome } from './exporter';

const TOKEN_LABEL: Record<NameToken, () => string> = {
  originalName: () => tz('Org. Name', '原文件名'),
  filmStock: () => tz('Film', '胶片'),
  printStock: () => tz('Print', '相纸'),
  date: () => tz('Date', '日期'),
};

async function loadRecipes(): Promise<ExportRecipe[]> {
  try {
    return decodeRecipes(await platform().storeRead('Export', 'recipes.json'));
  } catch {
    return DEFAULT_RECIPES;
  }
}

function waitForDeveloped(path: string, timeoutMs = 600_000): Promise<boolean> {
  return new Promise((resolve) => {
    const ok = () => {
      const s = sessionStore.getState();
      return s.selection === path && !!s.engineSession && !s.developing && !!s.nativeSize;
    };
    const failed = () => {
      const s = sessionStore.getState();
      return s.selection === path && !s.developing && (!s.engineSession || !s.nativeSize);
    };
    if (ok()) return resolve(true);
    if (failed()) return resolve(false);
    const t0 = setTimeout(() => {
      un();
      resolve(false);
    }, timeoutMs);
    const un = sessionStore.subscribe(() => {
      if (ok() || failed()) {
        clearTimeout(t0);
        un();
        resolve(ok());
      }
    });
  });
}

export function ExportPage() {
  const open = useSession((s) => s.exportOpen);
  const picked = useSession((s) => s.picked);
  const selection = useSession((s) => s.selection);
  const batch = useSession((s) => s.batchExporting);
  const catalog = useSession((s) => s.catalog);
  const hello = useSession((s) => s.hello);
  const params = useSession((s) => s.sidecar.params);
  const geometry = useSession((s) => s.sidecar.geometry);
  const adjustments = useSession((s) => s.sidecar.adjustments);
  const native = useSession((s) => s.nativeSize);
  const saved = useSession((s) => s.savedGeometry);
  const output = useOutput();
  const [recipes, setRecipes] = useState<ExportRecipe[]>(DEFAULT_RECIPES);
  const recipeId = useSettings((s) => s.exportRecipeId);
  const viewMode = useSettings((s) => s.exportView);
  const gridColumns = useSettings((s) => s.exportGridColumns);
  const [recipeError, setRecipeError] = useState('');
  const recipeWrites = useRef<Promise<void>>(Promise.resolve());
  const [stopped, setStopped] = useState(false);
  const [log, setLog] = useState<{ name: string; outcome: ExportOutcome }[]>([]);
  const [progress, setProgress] = useState<{ done: number; total: number } | null>(null);
  const [sample, setSample] = useState('');
  const stop = useRef(false);
  const proof = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    let active = true;
    if (open) void recipeWrites.current.then(loadRecipes).then((loaded) => { if (active) setRecipes(loaded); });
    return () => { active = false; };
  }, [open]);

  const current = Math.max(0, recipes.findIndex((recipe) => recipe.id === recipeId));
  const setCurrent = (index: number) => settingsStore.getState().set('exportRecipeId', recipes[index]?.id ?? DEFAULT_RECIPES[0]!.id);
  const r = recipes[current] ?? DEFAULT_RECIPES[0]!;
  const save = (next: ExportRecipe[]) => {
    setRecipes(next);
    setRecipeError('');
    recipeWrites.current = recipeWrites.current.then(() => platform().storeWrite('Export', 'recipes.json', next))
      .catch(() => setRecipeError(tz('The recipe could not be saved. Changes last for this session.', '方案未能保存，当前修改只在本次运行中保留。')));
  };
  const edit = (patch: Partial<ExportRecipe>) => save(recipes.map((x, i) => (i === current ? { ...x, ...patch } : x)));

  useEffect(() => {
    if (!selection) return;
    let active = true;
    void targetFor(r, { path: selection, session: '', params, adjustments, geometry, native: native ?? { width: 1, height: 1 } }, catalog).then((x) => { if (active) setSample(`${x.base}.${extensionOf(r)}`); });
    return () => { active = false; };
  }, [r, selection, params, catalog, adjustments, geometry, native]);

  useEffect(() => {
    const c = proof.current;
    if (!c || !output) return;
    c.width = output.width;
    c.height = output.height;
    c.getContext('2d')!.putImageData(new ImageData(new Uint8ClampedArray(output.data), output.width, output.height), 0, 0);
  }, [output, open, viewMode]);

  if (!open) return null;
  const frames = picked.length ? picked : selection ? [selection] : [];
  const writesEdits = hostWrites(hello?.methods as string[] | undefined);
  const size = native ? outputFor(r, geometry, native) : null;
  const counts = { written: log.filter((l) => l.outcome.kind === 'written').length, skipped: log.filter((l) => l.outcome.kind === 'skipped').length, failed: log.filter((l) => l.outcome.kind === 'failed').length };

  const run = async () => {
    if (sessionStore.getState().batchExporting) return;
    stop.current = false;
    setStopped(false);
    setLog([]);
    sessionStore.setState({ batchExporting: true });
    const back = sessionStore.getState().selection;
    let completed = 0;
    try {
      for (let i = 0; i < frames.length; i++) {
        if (stop.current) break;
        const path = frames[i]!;
        setProgress({ done: i, total: frames.length });
        await select(path);
        const ready = await waitForDeveloped(path);
        const s = sessionStore.getState();
        if (!ready || !s.engineSession || !s.nativeSize) {
          setLog((l) => [...l, { name: baseName(path), outcome: { kind: 'failed', message: s.lastError ?? tz('the frame did not develop', '照片未能显影') } }]);
          completed++;
          setProgress({ done: completed, total: frames.length });
          continue;
        }
        const outcome = await exportFrame(
          r,
          {
            path,
            session: s.engineSession,
            params: s.sidecar.params,
            adjustments: s.sidecar.adjustments,
            geometry: s.sidecar.geometry,
            native: s.nativeSize,
            dateTaken: s.metadata?.datetime_original,
          },
          s.catalog,
          s.hello?.methods as string[] | undefined,
        );
        platform().log(outcome.kind === 'failed' ? 'error' : 'info', `export ${baseName(path)}: ${JSON.stringify(outcome)}`);
        setLog((l) => [...l, { name: baseName(path), outcome }]);
        completed++;
        setProgress({ done: completed, total: frames.length });
      }
      setStopped(stop.current && completed < frames.length);
    } catch (e) {
      setLog((rows) => [...rows, { name: baseName(frames[completed] ?? ''), outcome: { kind: 'failed', message: String(e) } }]);
      completed = Math.min(completed + 1, frames.length);
    } finally {
      setProgress({ done: completed, total: frames.length });
      sessionStore.setState({ batchExporting: false });
      if (back && sessionStore.getState().selection !== back) void select(back);
    }
  };

  return (
    <div className="export-page" data-testid="export-page">
      <div className="topbar">
        <span style={{ fontWeight: 700, fontSize: 'var(--fs-rail-title)' }}>{t('helpExport')}</span>
        <span className="bar-status">
          {progress ? tz(`${progress.done} of ${progress.total} processed`, `已处理 ${progress.done} / ${progress.total}`) + (stopped ? (batch ? tz(' · Stopping…', ' · 正在停止…') : tz(' · Stopped', ' · 已停止')) : '') : ''}
        </span>
        <div className="export-view-switch" role="group" aria-label={tz('Export view', '导出视图')}>
          <button className={'btn' + (viewMode === 'viewer' ? ' primary' : '')} aria-pressed={viewMode === 'viewer'} onClick={() => settingsStore.getState().set('exportView', 'viewer')}>{tz('Viewer', '单张')}</button>
          <button className={'btn' + (viewMode === 'grid' ? ' primary' : '')} aria-pressed={viewMode === 'grid'} onClick={() => settingsStore.getState().set('exportView', 'grid')}>{tz('Grid', '网格')}</button>
        </div>
        {viewMode === 'grid' && <input className="export-grid-size" type="range" min={2} max={6} value={gridColumns} aria-label={tz('Grid columns', '网格列数')} onChange={(e) => settingsStore.getState().set('exportGridColumns', Number(e.target.value))} />}
        <span className="pill-menu" style={{ marginRight: 12 }}>
          {tz(`${frames.length} image${frames.length === 1 ? '' : 's'}`, `${frames.length} 张`)}
        </span>
        {batch ? (
          <button className="btn" disabled={stopped} onClick={() => { stop.current = true; setStopped(true); }}>
            {stopped ? tz('Stopping…', '正在停止…') : tz('Stop', '停止')}
          </button>
        ) : (
          <>
            <button className="btn" onClick={() => setPages({ exportOpen: false })} style={{ marginRight: 8 }}>
              {tz('Close', '关闭')}
            </button>
            <button className="btn primary" disabled={!frames.length} onClick={() => void run()} data-testid="export-run">
              {tz('Export', '导出')}
            </button>
          </>
        )}
      </div>
      {progress && <progress className="export-progress" max={progress.total} value={progress.done} aria-label={tz('Export progress', '导出进度')} />}
      <div className="export-body" data-mode={viewMode}>
        <aside className="rail left" inert={batch}>
          <div className="rail-scroll">
            <Section
              id="exp-recipes"
              title={tz('Export Formula', '导出方案')}
              menu={[
                { label: tz('Restore the built-in recipes', '恢复内置方案'), onSelect: () => save(DEFAULT_RECIPES) },
                {
                  label: tz('Export the print as a .cube LUT…', '将相纸导出为 .cube LUT…'),
                  onSelect: () => void exportCube(params.printStock, `${params.printStock}.cube`).catch((e) => platform().message('SpektraLab', String(e), 'error')),
                },
              ]}
            >
              <div className="recipe-list">
                {recipes.map((x, i) => (
                  <button key={x.id} className={'stock-row' + (i === current ? ' selected' : '')} aria-pressed={i === current} onClick={() => setCurrent(i)}>
                    <span className="name">{x.name}</span>
                  </button>
                ))}
              </div>
              <div className="row" style={{ justifyContent: 'flex-end', gap: 6 }}>
                <button className="icon-btn" aria-label={tz('Add a recipe', '添加方案')} title={tz('Add a recipe', '添加方案')} onClick={() => { const id = crypto.randomUUID(); save([...recipes, { ...r, id, name: r.name + ' ' + tz('copy', '副本') }]); settingsStore.getState().set('exportRecipeId', id); }}>
                  +
                </button>
                <button className="icon-btn" aria-label={tz('Remove this recipe', '删除方案')} title={tz('Remove this recipe', '删除方案')} disabled={recipes.length <= 1} onClick={() => { const next = recipes.filter((_, i) => i !== current); save(next); settingsStore.getState().set('exportRecipeId', next[0]!.id); }}>
                  −
                </button>
              </div>
              <div className="row">
                <span className="row-label">{tz('Name', '名称')}</span>
                <input className="unit-field" aria-label={tz('Recipe name', '方案名称')} style={{ flex: 1, textAlign: 'left', padding: '0 6px' }} value={r.name} onChange={(e) => edit({ name: e.target.value })} />
              </div>
              {recipeError && <p className="caption" role="alert">{recipeError}</p>}
            </Section>
            <Section id="exp-location" title={tz('Location', '位置')}>
              <div className="row">
                <span className="row-label">{tz('Folder', '文件夹')}</span>
                <button
                  className="pill-menu fill"
                  title={r.folder.kind === 'fixed' ? r.folder.path : undefined}
                  onClick={async () => {
                    const p = await platform().pickFolder(tz('Export to…', '导出到…'));
                    if (p) edit({ folder: { kind: 'fixed', path: p } });
                  }}
                >
                  <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{r.folder.kind === 'fixed' ? r.folder.path : tz('Beside the original', '原图旁')}</span>
                </button>
                {r.folder.kind === 'fixed' && (
                  <button className="icon-btn" title={tz('Beside the original', '原图旁')} onClick={() => edit({ folder: { kind: 'besideOriginal' } })}>
                    ×
                  </button>
                )}
              </div>
              <div className="row">
                <span className="row-label">{tz('Subfolder', '子文件夹')}</span>
                <input className="unit-field" style={{ flex: 1, textAlign: 'left', padding: '0 6px' }} value={r.subfolder} onChange={(e) => edit({ subfolder: e.target.value.replace(/[\\/:]/g, '-') })} />
              </div>
              <PillMenu
                label={tz('Existing File', '已有文件')}
                value={r.existing}
                fill
                options={[
                  { value: 'addSuffix', label: tz('Add a number', '加编号') },
                  { value: 'overwrite', label: tz('Overwrite', '覆盖') },
                  { value: 'skip', label: tz('Keep (skip)', '保留（跳过）') },
                ]}
                onChange={(v) => edit({ existing: v })}
              />
            </Section>
            <Section id="exp-naming" title={tz('Naming', '命名')}>
              <div className="row" style={{ flexWrap: 'wrap', gap: 4 }}>
                <span className="row-label">{tz('Format', '格式')}</span>
                {r.naming.order.map((tok, i) => (
                  <button
                    key={tok}
                    className={'chip' + (r.naming.tokens.includes(tok) ? ' on' : '')}
                    title={tz('Click to include; Alt-click to move left', '点击加入；按住 Alt 点击左移')}
                    onClick={(e) => {
                      if (e.altKey && i > 0) {
                        const order = [...r.naming.order];
                        [order[i - 1], order[i]] = [order[i]!, order[i - 1]!];
                        edit({ naming: { ...r.naming, order } });
                        return;
                      }
                      const on = r.naming.tokens.includes(tok) ? r.naming.tokens.filter((x) => x !== tok) : [...r.naming.tokens, tok];
                      if (on.length) edit({ naming: { ...r.naming, tokens: NAME_TOKENS.filter((x) => on.includes(x)) } });
                    }}
                  >
                    {TOKEN_LABEL[tok]()}
                  </button>
                ))}
              </div>
              <div className="row">
                <span className="row-label">{tz('Sample', '示例')}</span>
                <span className="caption" style={{ userSelect: 'text' }} data-testid="export-sample">
                  {sample}
                </span>
              </div>
            </Section>
            <Section id="exp-format" title={tz('Format and Size', '格式与尺寸')}>
              <div className="row">
                <span className="row-label">{tz('Format', '格式')}</span>
                <PillMenu
                  value={r.format}
                  options={[
                    { value: 'jpeg', label: 'JPEG' },
                    { value: 'png', label: 'PNG' },
                    { value: 'tiff', label: 'TIFF' },
                    { value: 'di', label: 'Digital Intermediate', disabled: !sessionStore.getState().gate.digitalIntermediate },
                  ]}
                  onChange={(v) => edit({ format: v, bitDepth: v === 'jpeg' || v === 'png' ? 8 : r.bitDepth })}
                />
                <PillMenu
                  value={r.bitDepth}
                  disabled={r.format !== 'tiff'}
                  reason={tz('JPEG and PNG are 8-bit on this host.', '此处 JPEG 与 PNG 为 8 位。')}
                  options={[
                    { value: 8, label: '8 bit' },
                    { value: 16, label: '16 bit' },
                  ]}
                  onChange={(v) => edit({ bitDepth: v })}
                />
              </div>
              <PillMenu
                label={tz('Color Space', '色彩空间')}
                value={r.colorSpace}
                fill
                disabled={r.format === 'di'}
                options={[
                  { value: 'sRGB', label: 'sRGB' },
                  { value: 'display-p3', label: 'Display P3' },
                  { value: 'prophoto', label: 'ProPhoto RGB' },
                ]}
                onChange={(v) => edit({ colorSpace: v })}
              />
              <ScrubSlider label={tz('Quality', '质量')} value={r.quality} range={[10, 100]} zero={92} snap={5} format={(v) => v.toFixed(0)} disabled={r.format !== 'jpeg'} onChange={(v) => edit({ quality: Math.round(v) })} />
              <div className="row">
                <span className="row-label">{tz('Long Edge', '长边')}</span>
                <Checkbox on={r.longEdge === 0} label={tz('Original size', '原始尺寸')} onChange={(v) => edit({ longEdge: v ? 0 : 3000 })} />
                <span className="caption">{tz('Original', '原始')}</span>
                {r.longEdge > 0 && <NumberField value={r.longEdge} decimals={0} onCommit={(v) => edit({ longEdge: Math.max(16, Math.round(v)) })} />}
                <span className="caption num">{size ? `${size.width} × ${size.height}` : ''}</span>
              </div>
            </Section>
            <Section id="exp-summary" title={tz('Summary', '摘要')}>
              {progress && <p className="caption" role="status">{tz(`Written ${counts.written} · Skipped ${counts.skipped} · Failed ${counts.failed}`, `已写入 ${counts.written} · 跳过 ${counts.skipped} · 失败 ${counts.failed}`)}</p>}
              <div className="caption">
                {writesEdits
                  ? tz('The file is the canvas’s frame: crop, rotation and Post-Dev included.', '文件即画布上的画面：包含裁剪、旋转与显影后调整。')
                  : tz('This engine writes the print only: the crop, rotation and Post-Dev grade are not in the file.', '此引擎只写入相纸画面：裁剪、旋转与显影后调整不会写入文件。')}
              </div>
              {log.map((l, i) => (
                <div key={i} className="caption mono" style={{ color: l.outcome.kind === 'failed' ? 'var(--hist-r)' : undefined }}>
                  {l.name}: {l.outcome.kind === 'written' ? `→ ${l.outcome.path}` : l.outcome.kind === 'skipped' ? tz(`skipped (${l.outcome.path} exists)`, `已跳过（${l.outcome.path} 已存在）`) : l.outcome.message}
                </div>
              ))}
              {log.some((l) => l.outcome.kind === 'written') && (
                <button
                  className="btn"
                  onClick={() => {
                    const w = log.find((l) => l.outcome.kind === 'written');
                    if (w && w.outcome.kind === 'written') void platform().reveal(w.outcome.path);
                  }}
                >
                  {tz('Show in folder', '在文件夹中显示')}
                </button>
              )}
            </Section>
          </div>
        </aside>
        <main className={'export-proof' + (viewMode === 'grid' ? ' grid-mode' : '')}>
          {viewMode === 'viewer' ? <><canvas ref={proof} data-testid="export-proof" /><div className="export-proof-caption"><span>{tz('Canvas preview · sRGB', '画布预览 · sRGB')}</span><span>{size ? `${size.width} × ${size.height} · ` : ''}{r.format.toUpperCase()} · {r.bitDepth} bit · {r.colorSpace}</span></div></> :
            <div className="export-grid" style={{ gridTemplateColumns: `repeat(${gridColumns}, minmax(0, 1fr))` }} data-testid="export-grid">
              {frames.map((p) => <button key={p} className={'export-cell' + (p === selection ? ' open' : '')} disabled={batch} onClick={() => { if (!batch) void select(p); }} onDoubleClick={() => settingsStore.getState().set('exportView', 'viewer')} title={tz('Select to preview; double-click for Viewer', '点击预览，双击进入单张视图')}><Thumb path={p} geometry={p === selection ? geometry : (saved[p] ?? GEOMETRY_DEFAULT)} height={160} /><span>{baseName(p)}</span></button>)}
            </div>}
        </main>
        {viewMode === 'viewer' && <aside className="export-strip">
          {frames.map((p) => (
            <button key={p} className={'export-cell' + (p === selection ? ' open' : '')} disabled={batch} onClick={() => { if (!batch) void select(p); }}>
              {/* The whole photograph, turned, the crop under a mask — as the filmstrip (§7.7). */}
              <Thumb path={p} geometry={p === selection ? geometry : (saved[p] ?? GEOMETRY_DEFAULT)} height={120} />
              <span>{baseName(p)}</span>
            </button>
          ))}
        </aside>}
      </div>
    </div>
  );
}
