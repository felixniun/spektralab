import { describe, expect, it } from 'vitest';
import { frameFolder, moveFrameOnto, orderedFrames } from './frameOrder';
const frames = (names: string[], folder = 'C:\\Photos') => names.map((name) => ({ path: folder + '\\' + name, name, size: 1, mtime_ms: 0 }));
const names = (value: ReturnType<typeof frames>) => value.map((f) => f.name);

describe('Mac filmstrip order semantics', () => {
  it('keeps newly imported photos beside their listed neighbour', () => {
    expect(names(orderedFrames(frames(['a', 'b', 'bb', 'c', 'd']), ['d', 'b', 'c', 'a'])))
      .toEqual(['d', 'b', 'bb', 'c', 'a']);
  });
  it('ignores missing/duplicate/corrupt saved names without losing frames', () => {
    expect(names(orderedFrames(frames(['a', 'b', 'c', 'd']), ['gone', 'd', 'd', null, 'a'])))
      .toEqual(['d', 'a', 'b', 'c']);
    expect(names(orderedFrames(frames(['a', 'b']), {}))).toEqual(['a', 'b']);
  });
  it('moves left and right without changing the frame objects or losing selection identity', () => {
    const original = frames(['a', 'b', 'c', 'd']);
    const moved = moveFrameOnto(original, original[0]!.path, original[2]!.path);
    expect(names(moved)).toEqual(['b', 'c', 'a', 'd']);
    expect(moved[2]).toBe(original[0]);
    expect(names(moveFrameOnto(moved, original[3]!.path, original[1]!.path))).toEqual(['d', 'b', 'c', 'a']);
    expect(names(original)).toEqual(['a', 'b', 'c', 'd']);
  });
  it('only persists an order for one folder', () => {
    expect(frameFolder(frames(['a', 'b']))).toBe('C:\\Photos');
    expect(frameFolder([...frames(['a']), ...frames(['b'], 'D:\\Other')])).toBeNull();
    expect(frameFolder([])).toBeNull();
  });
});
