// Match FrameOrder.swift: new photographs stay after their listed neighbour.
import type { FileEntry } from '../platform';
import { dirName } from '../platform';

export function frameFolder(frames: FileEntry[]): string | null {
  const parents = new Set(frames.map((f) => dirName(f.path)));
  return parents.size === 1 ? [...parents][0]! : null;
}

export function orderedFrames(listed: FileEntry[], saved: unknown): FileEntry[] {
  if (!Array.isArray(saved)) return listed;
  const rank = new Map<string, number>();
  for (const name of saved) if (typeof name === 'string' && !rank.has(name)) rank.set(name, rank.size);
  const out = listed.filter((f) => rank.has(f.name)).sort((a, b) => rank.get(a.name)! - rank.get(b.name)!);
  listed.forEach((f, i) => {
    if (rank.has(f.name)) return;
    const previous = i > 0 ? out.findIndex((p) => p.path === listed[i - 1]!.path) : -1;
    out.splice(previous + 1, 0, f);
  });
  return out;
}

/** Moving onto a cell takes its old index, as on the Mac. */
export function moveFrameOnto(frames: FileEntry[], from: string, onto: string): FileEntry[] {
  const source = frames.findIndex((f) => f.path === from);
  const target = frames.findIndex((f) => f.path === onto);
  if (source < 0 || target < 0 || source === target) return frames;
  const out = [...frames];
  out.splice(source, 1);
  out.splice(target, 0, frames[source]!);
  return out;
}

export async function orderFileName(folder: string): Promise<string> {
  const hash = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(folder));
  return [...new Uint8Array(hash)].map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 16) + '.order.json';
}
