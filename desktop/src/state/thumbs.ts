// thumbs.ts — the filmstrip's pictures: the host's `thumbnail` (the embedded
// preview) until the frame has been printed in this session, then the print.
// Uncropped on purpose (ARCHITECTURE §7.7): each surface applies geometry.

import { host } from '../host/client';
import { rgbaToURL } from './frames';

interface Entry {
  url: string;
  print: boolean;
}

const MAX_CONCURRENT = 1;

export class ThumbCache {
  private generation = 0;
  private busy = false;
  private map = new Map<string, Entry>();
  private queue: string[] = [];
  private pending = new Set<string>();
  private running = 0;
  private listeners = new Set<() => void>();
  private failed = new Set<string>();

  get(path: string): string | null {
    const e = this.map.get(path);
    if (e) return e.url;
    if (!this.pending.has(path) && !this.failed.has(path)) {
      this.pending.add(path);
      this.queue.push(path);
      this.pump();
    }
    return null;
  }
  peek(path: string): string | null { return this.map.get(path)?.url ?? null; }
  stats() { return { cached: this.map.size, queued: this.queue.length, running: this.running, busy: this.busy }; }
  setBusy(busy: boolean) { this.busy = busy; if (!busy) this.pump(); }

  setPrint(path: string, url: string) {
    const old = this.map.get(path);
    if (old) URL.revokeObjectURL(old.url);
    this.map.set(path, { url, print: true });
    this.emit();
  }

  reset() {
    this.generation++;
    for (const e of this.map.values()) URL.revokeObjectURL(e.url);
    this.map.clear();
    this.queue = [];
    this.pending.clear();
    this.failed.clear();
    this.emit();
  }

  onChange(cb: () => void) {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  private emit() {
    for (const l of this.listeners) l();
  }

  private pump() {
    while (!this.busy && this.running < MAX_CONCURRENT && this.queue.length) {
      const path = this.queue.shift()!;
      const generation = this.generation;
      this.running++;
      void (async () => {
        try {
          const img = await host().thumbnail(path, 320);
          const url = await rgbaToURL(img);
          if (generation !== this.generation || this.map.get(path)?.print) { if (url) URL.revokeObjectURL(url); }
          else if (url) this.map.set(path, { url, print: false });
          else this.failed.add(path);
        } catch {
          if (generation === this.generation) this.failed.add(path);
        } finally {
          if (generation === this.generation) this.pending.delete(path);
          this.running--;
          this.emit();
          this.pump();
        }
      })();
    }
  }
}

export const thumbs = new ThumbCache();
