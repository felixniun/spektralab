import { expect, it } from 'vitest';
import { panelWidth } from './panelWidths';
import { loadSettings } from '../state/settings';

it('recovers old/corrupt panel widths before they can hide the canvas', () => {
  const settings = loadSettings({ getItem: () => JSON.stringify({ leftWidth: -20, rightWidth: 999999 }) });
  expect(settings.leftWidth).toBe(232);
  expect(settings.rightWidth).toBe(364);
  expect(panelWidth('left', NaN)).toBe(254);
  expect(panelWidth('right', Infinity)).toBe(288);
});
