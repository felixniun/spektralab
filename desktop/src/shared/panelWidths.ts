// Bounds from Theme.Metric.leftPanelRange / rightPanelRange on macOS.
export const PANEL_WIDTHS = {
  left: { min: 232, standard: 254, max: 380 },
  right: { min: 280, standard: 288, max: 364 },
} as const;

export function panelWidth(side: keyof typeof PANEL_WIDTHS, value: number): number {
  const range = PANEL_WIDTHS[side];
  return Number.isFinite(value) ? Math.min(range.max, Math.max(range.min, value)) : range.standard;
}
