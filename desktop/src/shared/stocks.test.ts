import { expect, it } from 'vitest';
import { StockCatalog, applyFilmStageRule, selectFilmParams } from './stocks';
import stockData from '../../public/assets/StockCatalog.json';
import { FILM_PARAMS_DEFAULT, fullDelta } from './params';
const catalog = new StockCatalog([
  ...['fujifilm_provia_100f', 'fujifilm_velvia_100', 'kodak_ektachrome_100', 'kodak_kodachrome_64'].map((id) => ({ id, name: id, stage: 'filming', use: 'still', type: 'positive', hasPreviewLUT: false })),
  { id: 'kodak_portra_400', name: 'Portra', stage: 'filming', use: 'still', type: 'negative', hasPreviewLUT: false },
]);

it('sends every slide stock through direct scan, retaining the chosen paper', () => {
  for (const stock of catalog.films.filter((s) => s.type === 'positive')) {
    const old = { ...FILM_PARAMS_DEFAULT, filmStock: stock.id, scanFilm: false, extendedDynamicRange: true };
    const fixed = applyFilmStageRule(old, catalog);
    expect(fullDelta(fixed)).toMatchObject({ scan_film: true, extended_dynamic_range: false });
    expect(fixed.printStock).toBe(old.printStock);
    expect(old.scanFilm).toBe(false);
  }
});
it('keeps a negative film’s explicit scan or paper choice', () => {
  for (const scanFilm of [false, true]) {
    const p = { ...FILM_PARAMS_DEFAULT, scanFilm };
    expect(applyFilmStageRule(p, catalog)).toBe(p);
  }
});

it('round-trips every slide to every negative using the catalogue paper, never carrying slide scan into a print', () => {
  const actual = new StockCatalog(stockData.stocks);
  for (const slide of actual.films.filter((s) => s.type === 'positive')) {
    const slideParams = selectFilmParams(FILM_PARAMS_DEFAULT, slide.id, actual);
    expect(slideParams.scanFilm).toBe(true);
    for (const negative of actual.films.filter((s) => s.type === 'negative')) {
      const print = selectFilmParams(slideParams, negative.id, actual);
      expect(fullDelta(print).scan_film, `${slide.id} -> ${negative.id}`).toBe(false);
      expect(print.printStock).toBe(negative.targetPrint);
      expect(actual.canPrint(print.filmStock, print.printStock)).toBe(true);
      expect(selectFilmParams(print, slide.id, actual).scanFilm).toBe(true);
    }
  }
});

it('keeps colour and silver papers on their matching materials', () => {
  const actual = new StockCatalog(stockData.stocks);
  expect(actual.canPrint('ilford_hp5_plus_400', 'kodak_supra_endura')).toBe(false);
  expect(actual.canPrint('kodak_portra_800', 'ilford_multigrade_iv_rc')).toBe(false);
});
