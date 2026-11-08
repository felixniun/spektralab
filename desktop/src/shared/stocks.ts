// stocks.ts — the film and paper catalogue (`Model/StockCatalog.swift`),
// read from the Mac app's `StockCatalog.json` (copied to public/assets).
import type { FilmParams } from './params';
import bundledStocks from '../../../modern_UI/Spektrafilm/Spektrafilm/Resources/StockCatalog.json';

/** Session.applyFilmStageRule: slides are scanned, never printed on paper.
 * Keep the chosen paper and the negative-scan preference for other stocks.
 */
export function applyFilmStageRule(params: FilmParams, catalog: StockCatalog): FilmParams {
  return catalog.isPositive(params.filmStock) && !params.scanFilm ? { ...params, scanFilm: true } : params;
}

/** A stock-list choice selects a viewable workflow. An explicit negative
 * scan is a separate paper-list choice and must not leak out of a slide.
 * Follow the catalogue's declared pairing, as Session.selectFilmStock does.
 */
export function selectFilmParams(params: FilmParams, id: string, catalog: StockCatalog): FilmParams {
  const film = catalog.stock(id);
  if (!film || film.stage !== 'filming') return params;
  const target = film.targetPrint ? catalog.stock(film.targetPrint) : undefined;
  const paper = target?.stage === 'printing' ? target.id : params.printStock;
  return { ...params, filmStock: id, printStock: paper, scanFilm: catalog.isPositive(id) };
}

export interface Stock {
  id: string;
  name: string;
  stage: 'filming' | 'printing' | string;
  use: 'still' | 'cine' | string;
  type?: string | null;
  targetPrint?: string | null;
  hasPreviewLUT: boolean;
  pairedFilm?: string | null;
  cover?: string | null;
  formats?: string[] | null;
  monochrome?: boolean;
}

export class StockCatalog {
  readonly stocks: Stock[];
  constructor(stocks: Stock[]) {
    this.stocks = stocks;
  }
  get films() {
    return this.stocks.filter((s) => s.stage === 'filming');
  }
  get papers() {
    return this.stocks.filter((s) => s.stage === 'printing');
  }
  stock(id: string) {
    return this.stocks.find((s) => s.id === id);
  }
  isPositive(id: string) {
    return this.stock(id)?.type === 'positive';
  }
  canPrint(film: string, paper: string) {
    const f = this.stock(film), p = this.stock(paper);
    return !f || !p || !!f.monochrome === !!p.monochrome;
  }
  /** Still first, then cine, as the Mac picker orders them; then by polarity. */
  filmGroups(): { title: 'Positive' | 'Negative'; films: Stock[] }[] {
    const ordered = [...this.films.filter((s) => s.use !== 'cine'), ...this.films.filter((s) => s.use === 'cine')];
    return [
      { title: 'Positive', films: ordered.filter((s) => s.type === 'positive') },
      { title: 'Negative', films: ordered.filter((s) => s.type !== 'positive') },
    ];
  }
  paperGroups(): { title: 'Still' | 'Cine'; papers: Stock[] }[] {
    return [
      { title: 'Still', papers: this.papers.filter((s) => s.use !== 'cine') },
      { title: 'Cine', papers: this.papers.filter((s) => s.use === 'cine') },
    ];
  }
  coverURL(id: string): string | null {
    const c = this.stock(id)?.cover;
    return c ? `assets/FilmCovers/${c}` : null;
  }
}

export const EMPTY_CATALOG = new StockCatalog([]);
export const BUILTIN_CATALOG = new StockCatalog(bundledStocks.stocks);

export async function loadCatalog(): Promise<StockCatalog> {
  try {
    const r = await fetch('assets/StockCatalog.json');
    const j = (await r.json()) as { stocks: Stock[] };
    return new StockCatalog(j.stocks ?? []);
  } catch {
    return BUILTIN_CATALOG;
  }
}
