import { expect, it } from 'vitest';
import { needsNativeDetail } from './renderPolicy';
it('avoids native renders at fit zoom while retaining native detail at 1:1', () => {
  expect(needsNativeDetail(6000, 2560, 0.2, 'zoom')).toBe(false);
  expect(needsNativeDetail(6000, 2560, 1, 'zoom')).toBe(true);
  expect(needsNativeDetail(6000, 2560, 0.2, 'idle')).toBe(true);
  expect(needsNativeDetail(1800, 2560, 1, 'zoom')).toBe(false);
});
