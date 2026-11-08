// The store's edit doors, against a stand-in host and platform: what makes an
// undo step and what does not.

import { beforeEach, describe, expect, it, vi } from 'vitest';

const calls: string[] = [];
vi.mock('../host/client', () => {
  class HostCallError extends Error {
    code = 'internal';
  }
  const h = {
    sceneLatitude: vi.fn(),
    setParams: vi.fn(async () => {
      calls.push('set_params');
      return {};
    }),
    render: vi.fn(async (_sid: string, tier: string) => {
      calls.push(`render:${tier}`);
      return { width: 4, height: 4, data: new Uint8Array(64), colorSpace: 'sRGB', tier, reprinted: false, ms: 1 };
    }),
    redecode: vi.fn(async (_sid: string, decode: object) => {
      calls.push('redecode:' + JSON.stringify(decode));
      return { session: 's1', width: 6000, height: 4000, kind: 'raw', metadata: { orientation: 1 }, params: {} };
    }),
    pickNeutral: vi.fn(async () => ({ temperature_k: 4800, tint: 12 })),
  };
  return { HostCallError, host: () => h };
});
vi.mock('../platform', () => ({
  platform: () => ({ log: () => {}, sidecarSave: async () => '' }),
  baseName: (p: string) => p.split(/[\\/]/).pop() ?? p,
  dirName: (p: string) => p,
}));

import { pickNeutralAt, placeScene, recomputeFilmFormat, requestPrint, sessionStore, setDecode, setGeometry, setParams, setWhiteBalancePicker, undo } from './session';
import { host, type RenderReply } from '../host/client';
import type { HelloResult } from '@shared/protocol';
import { settingsStore } from './settings';
import { newSidecar } from '@shared/sidecar';
import { StockCatalog, EMPTY_CATALOG } from '@shared/stocks';
import { ALL_FEATURES } from '@shared/params';

// Undo steps coalesce within 500 ms; each case starts well past the last one.
let clock = 0;
beforeEach(() => {
  clock += 10_000;
  vi.spyOn(performance, 'now').mockReturnValue(clock);
  calls.length = 0;
  sessionStore.setState({
    selection: '/x/a.cr2',
    engineSession: 's1',
    nativeSize: { width: 6000, height: 4000 },
    sidecar: newSidecar(),
    undo: [],
    redo: [],
    batchExporting: false,
    gate: ALL_FEATURES,
    catalog: EMPTY_CATALOG,
  });
  settingsStore.getState().set('recalculateEffectsAfterCrop', true);
});

describe('positive film stage rule', () => {
  it('repairs the actual selection door and cannot undo back into printing a slide', () => {
    const stock = 'kodak_ektachrome_100';
    sessionStore.setState({ catalog: new StockCatalog([{ id: stock, name: stock, type: 'positive', stage: 'filming', use: 'still', hasPreviewLUT: false }]) });
    setParams((p) => ({ ...p, filmStock: stock }));
    expect(sessionStore.getState().sidecar.params.scanFilm).toBe(true);
    expect(sessionStore.getState().sidecar.params.printStock).toBe(newSidecar().params.printStock);
    sessionStore.setState({ undo: [{ params: { ...newSidecar().params, filmStock: stock, scanFilm: false }, adjustments: newSidecar().adjustments, geometry: newSidecar().geometry, decode: newSidecar().decode }] });
    undo();
    expect(sessionStore.getState().sidecar.params.scanFilm).toBe(true);
  });
});

it('renders the newly selected session when an old in-flight render completes', async () => {
  const render = vi.mocked(host().render);
  render.mockClear();
  let release!: (value: RenderReply) => void;
  render.mockImplementationOnce(() => new Promise<RenderReply>((resolve) => { release = resolve; }));
  sessionStore.setState({ engineSession: 'old', nativeSize: { width: 1200, height: 800 } });
  const previous = requestPrint();
  await vi.waitFor(() => expect(render).toHaveBeenCalled());
  sessionStore.setState({ engineSession: 'new' });
  void requestPrint();
  release({ width: 4, height: 4, data: new Uint8Array(64), colorSpace: 'sRGB', tier: 'live', reprinted: false, ms: 1 });
  await previous;
  await vi.waitFor(() => expect(render).toHaveBeenCalledWith('new', 'live', expect.anything()));
});

describe('scene placement', () => {
  const fit = { valid: true, issues: [], highlight: { pull_back: 1 }, shadow: { pull_back: 0 },
    params_delta: { scene_latitude_active: true, scene_latitude_norm: 'power',
      scene_latitude_highlight_knee: 1.5, scene_latitude_highlight_room: 2,
      scene_latitude_shadow_knee: -2, scene_latitude_shadow_room: 0,
      scene_latitude_rolloff: 2, scene_latitude_max_lift: 4 } };

  it('commits the fitted curve as an undoable edit', async () => {
    vi.mocked(host().sceneLatitude).mockResolvedValue({ fit });
    expect(await placeScene(1, 0)).toBeNull();
    expect(sessionStore.getState().sidecar.params.sceneLatitude).toMatchObject({
      active: true, highlightPullBack: 1, highlightKnee: 1.5, highlightRoom: 2,
    });
    expect(sessionStore.getState().undo).toHaveLength(1);
    undo();
    expect(sessionStore.getState().sidecar.params.sceneLatitude.active).toBe(false);
  });

  it('does not commit a refused fit', async () => {
    const before = sessionStore.getState().sidecar;
    vi.mocked(host().sceneLatitude).mockResolvedValue({ fit: { valid: false, issues: [{ message: 'below minimum' }] } });
    expect(await placeScene(0.25, 0)).toBe('below minimum');
    expect(sessionStore.getState().sidecar).toBe(before);
    expect(sessionStore.getState().undo).toHaveLength(0);
  });

  it('discards a fit when the frame changes while waiting', async () => {
    let resolve!: (reply: Record<string, unknown>) => void;
    vi.mocked(host().sceneLatitude).mockImplementation(() => new Promise((done) => { resolve = done; }));
    const pending = placeScene(1, 0);
    await vi.waitFor(() => expect(resolve).toBeTypeOf('function'));
    sessionStore.setState({ selection: '/x/other.cr2', engineSession: 's2' });
    resolve({ fit });
    expect(await pending).toBeNull();
    expect(sessionStore.getState().sidecar.params.sceneLatitude.active).toBe(false);
    expect(sessionStore.getState().undo).toHaveLength(0);
  });
});

describe('the derived film format', () => {
  it('is written without an undo step', () => {
    const before = sessionStore.getState().sidecar.params.filmFormatMM;
    sessionStore.setState({ sidecar: { ...newSidecar(), params: { ...newSidecar().params, filmFormatMM: before + 7 } } });
    recomputeFilmFormat();
    const s = sessionStore.getState();
    expect(s.sidecar.params.filmFormatMM).toBeCloseTo(before, 3);
    expect(s.undo).toHaveLength(0);
  });

  it('rides along with the crop that moved it: one undo step, and undo restores both', async () => {
    const mm0 = sessionStore.getState().sidecar.params.filmFormatMM;
    setGeometry((g) => ({ ...g, crop: { x: 0.25, y: 0.25, width: 0.5, height: 0.5 } }));
    const s = sessionStore.getState();
    expect(s.undo).toHaveLength(1);
    expect(s.sidecar.params.filmFormatMM).not.toBeCloseTo(mm0, 3);
    undo();
    const u = sessionStore.getState();
    expect(u.sidecar.geometry.crop.width).toBe(1);
    expect(u.sidecar.params.filmFormatMM).toBeCloseTo(mm0, 3);
    await vi.waitFor(() => expect(calls).toContain('render:live'));
  });

  it('a real edit still makes an undo step', () => {
    setParams((p) => ({ ...p, exposureCompensationEV: 1 }));
    expect(sessionStore.getState().undo).toHaveLength(1);
  });
});

describe('white balance at decode (R2)', () => {
  const hello = { methods: ['open', 'redecode', 'render'] } as unknown as HelloResult;

  it('a decode edit is one undo step and re-decodes the session, then renders', async () => {
    sessionStore.setState({ hello, frameKind: 'raw' });
    setDecode((d) => ({ ...d, whiteBalance: 'Custom', temperature: 3200, tint: 0 }));
    expect(sessionStore.getState().undo).toHaveLength(1);
    await vi.waitFor(() => expect(calls).toContain('redecode:{"white_balance":{"mode":"custom","temperature_k":3200,"tint":0}}'));
    await vi.waitFor(() => expect(calls.indexOf('render:live')).toBeGreaterThan(calls.findIndex((c) => c.startsWith('redecode'))));
    calls.length = 0;
    undo();
    expect(sessionStore.getState().sidecar.decode.whiteBalance).toBe('As Shot');
    await vi.waitFor(() => expect(calls).toContain('redecode:{"white_balance":{"mode":"as_shot"}}'));
  });

  it('a raster frame is not re-decoded', async () => {
    sessionStore.setState({ hello, frameKind: 'tiff', selection: '/x/a.tif' });
    setDecode((d) => ({ ...d, whiteBalance: 'Custom', temperature: 3200 }));
    await vi.waitFor(() => expect(calls).toContain('render:live'));
    expect(calls.some((c) => c.startsWith('redecode'))).toBe(false);
  });

  it('turns a picked RAW neutral into one undoable decode edit', async () => {
    sessionStore.setState({ hello: { methods: ['redecode', 'pick_neutral'] } as unknown as HelloResult, frameKind: 'raw' });
    setWhiteBalancePicker(true);
    await pickNeutralAt(0.4, 0.6);
    expect(host().pickNeutral).toHaveBeenCalledWith('/x/a.cr2', 0.4, 0.6);
    expect(sessionStore.getState().sidecar.decode).toMatchObject({ whiteBalance: 'Custom', temperature: 4800, tint: 12 });
    expect(sessionStore.getState().undo).toHaveLength(1);
    await vi.waitFor(() => expect(calls.some((c) => c.startsWith('redecode:'))).toBe(true));
  });

  it('does not apply a neutral picked from a frame that was switched away', async () => {
    let resolve!: (v: { temperature_k: number; tint: number }) => void;
    vi.mocked(host().pickNeutral).mockImplementation(() => new Promise((done) => { resolve = done; }));
    sessionStore.setState({ hello: { methods: ['pick_neutral'] } as unknown as HelloResult, frameKind: 'raw' });
    setWhiteBalancePicker(true);
    const pending = pickNeutralAt(0.4, 0.6);
    await vi.waitFor(() => expect(resolve).toBeTypeOf('function'));
    sessionStore.setState({ selection: '/x/b.cr2' });
    resolve({ temperature_k: 3500, tint: -5 });
    await pending;
    expect(sessionStore.getState().sidecar.decode.whiteBalance).toBe('As Shot');
    expect(sessionStore.getState().undo).toHaveLength(0);
  });
});
