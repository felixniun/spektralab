// Filmstrip.tsx — one cell per frame (`Panels/Filmstrip.swift`). Each shows
// the **whole** photograph, turned and flipped, with the outside of the crop
// under an 82 % mask (Capture One's behaviour; ARCHITECTURE §7.7): the live
// geometry for the open frame, the saved one for the rest.
//
// Click opens (and collapses the picked set); Ctrl/Cmd-click toggles one
// frame in the set and leaves the canvas where it is.

import { useEffect, useRef, useState, useSyncExternalStore } from 'react';
import { ChevronLeft, ChevronRight } from '../controls/icons';
import { click, reorderFrame, selectRelative, useSession } from '../state/session';
import { thumbs } from '../state/thumbs';
import { type Geometry, GEOMETRY_DEFAULT } from '@shared/geometry';
import { thumbnailGeometryPlan } from '@shared/surfaces';
import { t, tz } from '../i18n';

export function Filmstrip() {
  const frames = useSession((s) => s.frames);
  const selection = useSession((s) => s.selection);
  const picked = useSession((s) => s.picked);
  const live = useSession((s) => s.sidecar.geometry);
  const saved = useSession((s) => s.savedGeometry);
  const batch = useSession((s) => s.batchExporting);
  const strip = useRef<HTMLDivElement>(null);
  const drag = useRef<{ path: string; x: number; y: number; moved: boolean; target: string | null } | null>(null);
  const suppressClick = useRef(false);
  const [dragged, setDragged] = useState<string | null>(null);
  const [dropTarget, setDropTarget] = useState<string | null>(null);
  useEffect(() => {
    const el = strip.current?.querySelector('.thumb.open');
    el?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  }, [selection]);
  return (
    <div className="filmstrip" data-testid="filmstrip">
      <button className="strip-chevron" onClick={() => selectRelative(-1)} disabled={batch} aria-label={tz('Previous photo', '上一张照片')}>
        <ChevronLeft />
      </button>
      <div className="filmstrip-cells" ref={strip}>
        {frames.length === 0 && <span className="caption">{t('statusEmptyFilmstrip').replace('⌘O', 'Ctrl+O')}</span>}
        {frames.map((f) => (
          <div
            key={f.path}
            className={'thumb' + (f.path === selection ? ' open' : '') + (picked.includes(f.path) ? ' picked' : '') + (dragged === f.path ? ' dragging' : '') + (dropTarget === f.path ? ' drop-target' : '')}
            role="button" tabIndex={batch ? -1 : 0} aria-pressed={picked.includes(f.path)}
            onClick={(e) => { if (suppressClick.current) { suppressClick.current = false; return; } click(f.path, e.ctrlKey || e.metaKey); }}
            onKeyDown={(e) => {
              if (e.altKey && (e.key === 'ArrowLeft' || e.key === 'ArrowRight')) {
                e.preventDefault(); e.stopPropagation();
                const next = frames[frames.findIndex((p) => p.path === f.path) + (e.key === 'ArrowLeft' ? -1 : 1)];
                if (next) reorderFrame(f.path, next.path);
              } else if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault(); e.stopPropagation(); click(f.path, e.ctrlKey || e.metaKey);
              }
            }}
            onPointerDown={(e) => {
              if (batch || e.button !== 0) return;
              suppressClick.current = false;
              drag.current = { path: f.path, x: e.clientX, y: e.clientY, moved: false, target: null };
              e.currentTarget.setPointerCapture(e.pointerId);
            }}
            onPointerMove={(e) => {
              const d = drag.current;
              if (!d) return;
              if (!d.moved && Math.hypot(e.clientX - d.x, e.clientY - d.y) < 6) return;
              d.moved = true; setDragged(d.path);
              const bounds = strip.current?.getBoundingClientRect();
              if (bounds) {
                if (e.clientX < bounds.left + 28) strip.current?.scrollBy(-20, 0);
                if (e.clientX > bounds.right - 28) strip.current?.scrollBy(20, 0);
              }
              const target = document.elementFromPoint(e.clientX, e.clientY)?.closest<HTMLElement>('.thumb');
              d.target = target && strip.current?.contains(target) && target.dataset.path !== d.path ? target.dataset.path ?? null : null;
              setDropTarget(d.target);
            }}
            onPointerUp={() => {
              const d = drag.current;
              if (d?.moved) { suppressClick.current = true; if (d.target) reorderFrame(d.path, d.target); }
              drag.current = null; setDragged(null); setDropTarget(null);
            }}
            onPointerCancel={() => { drag.current = null; setDragged(null); setDropTarget(null); suppressClick.current = true; }}
            onLostPointerCapture={() => { drag.current = null; setDragged(null); setDropTarget(null); }}
            title={f.name + '\n' + tz('Drag to reorder; Alt+Left/Right moves the focused photo', '拖动调整顺序；Alt+左右方向键移动焦点照片')}
            data-path={f.path}
          >
            <Thumb path={f.path} geometry={f.path === selection ? live : (saved[f.path] ?? GEOMETRY_DEFAULT)} />
            <span className="thumb-name">{f.name}</span>
          </div>
        ))}
      </div>
      <button className="strip-chevron" onClick={() => selectRelative(1)} disabled={batch} aria-label={tz('Next photo', '下一张照片')}>
        <ChevronRight />
      </button>
    </div>
  );
}

const CELL_H = 104;

export function Thumb({ path, geometry, height = CELL_H }: { path: string; geometry: Geometry; height?: number }) {
  const cell = useRef<HTMLDivElement>(null);
  const [visible, setVisible] = useState(false);
  const url = useSyncExternalStore((cb) => thumbs.onChange(cb), () => thumbs.peek(path));
  const ref = useRef<HTMLCanvasElement>(null);
  const [img, setImg] = useState<HTMLImageElement | null>(null);
  const [width, setWidth] = useState(120);
  useEffect(() => {
    const node = cell.current;
    if (!node) return;
    const observer = new IntersectionObserver(([entry]) => setVisible(!!entry?.isIntersecting), { rootMargin: '200px' });
    observer.observe(node);
    return () => observer.disconnect();
  }, []);
  useEffect(() => { if (visible && !url) thumbs.get(path); }, [visible, path, url]);
  useEffect(() => {
    if (!visible) { const timer = setTimeout(() => setImg(null), 0); return () => clearTimeout(timer); }
    if (!url) return;
    const i = new Image();
    i.onload = () => { setWidth(thumbnailGeometryPlan(geometry, { width: i.naturalWidth, height: i.naturalHeight }, height).width); setImg(i); };
    i.src = url;
    return () => { i.onload = null; };
  }, [url, visible, geometry, height]);
  useEffect(() => {
    const c = ref.current;
    if (!c || !img || !visible) return;
    const plan = thumbnailGeometryPlan(geometry, { width: img.naturalWidth, height: img.naturalHeight }, height);
    c.width = plan.width;
    c.height = plan.height;
    const ctx = c.getContext('2d')!;
    ctx.setTransform(...plan.imageTransform);
    ctx.drawImage(img, 0, 0);
    if (plan.cropPolygon) {
      // Darken outside the crop: whole cell minus the crop polygon.
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.fillStyle = 'rgba(45,45,44,0.82)';
      ctx.beginPath();
      ctx.rect(0, 0, plan.width, plan.height);
      const pts = plan.cropPolygon;
      ctx.moveTo(pts[0]!.x, pts[0]!.y);
      for (const p of pts.slice(1)) ctx.lineTo(p.x, p.y);
      ctx.closePath();
      ctx.fill('evenodd');
    }
  }, [img, geometry, height, visible]);
  return <div ref={cell} style={{ width, height, flex: 'none' }}>{url && visible ? <canvas ref={ref} /> : <div className="placeholder" style={{ width, height }} />}</div>;
}
