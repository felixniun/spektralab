import { describe, expect, it } from 'vitest';
import schema from '../../mock-host/schema-fields.json';
import {
  ALL_FEATURES,
  FILM_PARAMS_DEFAULT,
  aeMethodOf,
  applyAEMethod,
  decodeFilmParams,
  delta,
  featureGate,
  filmFormatMM,
  fullDelta,
  printExposure,
  wire,
} from './params';

const fields = new Map((schema as { name: string; layer: string; type: string; range?: number[] }[]).map((f) => [f.name, f]));

describe('the wire', () => {
  it('matches the Mac Multigrade neutral pack and per-film grain', () => {
    const p = { ...FILM_PARAMS_DEFAULT, filmStock: 'ilford_hp5_plus_400', printStock: 'ilford_multigrade_iv_rc', extendedDynamicRange: true };
    expect(fullDelta(p)).toMatchObject({ c_filter_neutral: 0, m_filter_neutral: 0, y_filter_neutral: 68, grain_amount: 1.49, extended_dynamic_range: false });
    expect(fullDelta(FILM_PARAMS_DEFAULT)).not.toHaveProperty('y_filter_neutral');
  });
  it('names only fields the engine schema declares, on the engine layer, with the right types', () => {
    // Every feature on, every optional row present.
    const p = {
      ...FILM_PARAMS_DEFAULT,
      filmEdge: { ...FILM_PARAMS_DEFAULT.filmEdge, active: true },
      dateBack: { ...FILM_PARAMS_DEFAULT.dateBack, active: true },
    };
    for (const f of wire(p, ALL_FEATURES)) {
      const s = fields.get(f.name);
      expect(s, f.name).toBeDefined();
      expect(f.layer, f.name).toBe(s!.layer);
      const t = typeof f.value;
      const want = s!.type === 'bool' ? 'boolean' : s!.type === 'str' ? 'string' : 'number';
      expect(t, f.name).toBe(want);
      if (s!.range && typeof f.value === 'number') {
        expect(f.value).toBeGreaterThanOrEqual(s!.range[0]!);
        expect(f.value).toBeLessThanOrEqual(s!.range[1]!);
      }
    }
  });

  it('sends no row for a feature the host cannot do', () => {
    const gate = featureGate(['digital_intermediate', 'scene_latitude_mapping', 'contrast_mask', 'overscan', 'date_imprint']);
    const names = new Set(wire({ ...FILM_PARAMS_DEFAULT, digitalIntermediate: true }, gate).map((f) => f.name));
    for (const n of ['digital_intermediate', 'scene_latitude_active', 'contrast_mask_active', 'overscan_active', 'date_imprint_active'])
      expect(names.has(n), n).toBe(false);
    expect(names.has('film_stock')).toBe(true);
  });

  it('sends film-edge carrier and half-frame date settings to the engine', () => {
    const p = {
      ...FILM_PARAMS_DEFAULT,
      filmEdge: { ...FILM_PARAMS_DEFAULT.filmEdge, active: true, format: '135_half', carrier: 'open', pair: true },
      dateBack: { ...FILM_PARAMS_DEFAULT.dateBack, active: true, textB: "'26 10 4", withData: true, dataText: 'f/2.8' },
    };
    const values = Object.fromEntries(wire(p, ALL_FEATURES).map((f) => [f.name, f.value]));
    expect(values.overscan_carrier).toBe('open');
    expect(values.overscan_pair).toBe(true);
    expect(values.date_imprint_text_b).toBe("'26 10 4");
    expect(values.date_imprint_data_text).toBe('f/2.8');
  });

  it('a print-side edit is a print-layer delta; a film change touches both layers', () => {
    const a = FILM_PARAMS_DEFAULT;
    const b = { ...a, printBrightnessStops: 1 };
    const d = delta(b, a);
    expect(Object.keys(d.delta)).toEqual(['print_exposure']);
    expect([...d.layers]).toEqual(['print']);
    expect(d.delta.print_exposure).toBeCloseTo(0.5);
    const c = { ...a, filmStock: 'kodak_gold_200' };
    expect([...delta(c, a).layers].sort()).toEqual(['print', 'shoot']);
    expect(Object.keys(delta(a, a).delta)).toHaveLength(0);
  });

  it('print exposure is 2^-stops, clamped', () => {
    expect(printExposure(0)).toBe(1);
    expect(printExposure(-10)).toBe(20);
    expect(printExposure(10)).toBe(0.05);
  });

  it('a legacy sidecar (no method) sends no auto_exposure_method', () => {
    const legacy = decodeFilmParams({ ...FILM_PARAMS_DEFAULT, autoExposureMethod: undefined });
    expect(legacy.autoExposureMethod).toBeNull();
    expect('auto_exposure_method' in fullDelta(legacy)).toBe(false);
    expect(aeMethodOf(legacy)).toBe('legacy');
    expect(aeMethodOf(applyAEMethod(legacy, 'custom'))).toBe('custom');
    expect(aeMethodOf(applyAEMethod(legacy, 'center'))).toBe('center');
  });

  it('decodes a sparse sidecar with defaults for the newer fields', () => {
    const p = decodeFilmParams({ filmStock: 'kodak_gold_200', printStock: 'x', autoExposureMethod: 'balanced', effects: { grain: 1.5 } });
    expect(p.filmStock).toBe('kodak_gold_200');
    expect(p.effects.grain).toBe(1.5);
    expect(p.effects.halation).toBe(1);
    expect(p.contrastMask.scale).toBe(0.03);
  });

  it('derives the long edge from the side and the photograph', () => {
    expect(filmFormatMM('short', 24, 1.5)).toBeCloseTo(36);
    expect(filmFormatMM('long', 36, 1.5)).toBe(36);
    expect(filmFormatMM('short', 56, 1)).toBe(56);
    expect(filmFormatMM('short', 56, 70 / 56)).toBeCloseTo(70);
  });
});
