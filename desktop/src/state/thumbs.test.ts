import { beforeEach, expect, it, vi } from 'vitest';
const pending = vi.hoisted(() => [] as (() => void)[]);
vi.mock('../host/client', () => ({ host: () => ({
  thumbnail: vi.fn(() => new Promise((resolve) => { pending.push(() => resolve({})); })),
}) }));
vi.mock('./frames', () => ({ rgbaToURL: async () => 'blob:test' }));
import { ThumbCache } from './thumbs';
beforeEach(() => { pending.length = 0; vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => {}); });
const settle = async () => { await new Promise((r) => setTimeout(r, 0)); };
it('pauses background thumbnails while an interactive render owns the host', async () => {
  const cache = new ThumbCache();
  cache.setBusy(true); cache.get('a'); cache.get('b');
  expect(cache.stats()).toMatchObject({ queued: 2, running: 0 });
  cache.setBusy(false);
  expect(cache.stats()).toMatchObject({ queued: 1, running: 1 });
  cache.setBusy(true); pending.shift()!(); await settle();
  expect(cache.stats()).toMatchObject({ queued: 1, running: 0 });
});
it('does not let an old folder response repopulate the cache after reset', async () => {
  const cache = new ThumbCache(); cache.get('old'); cache.reset();
  pending.shift()!(); await settle();
  expect(cache.peek('old')).toBeNull();
  expect(URL.revokeObjectURL).toHaveBeenCalledWith('blob:test');
});
