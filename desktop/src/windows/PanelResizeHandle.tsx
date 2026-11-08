import { useRef } from 'react';
import { settingsStore, useSettings } from '../state/settings';
import { PANEL_WIDTHS, panelWidth } from '@shared/panelWidths';
import { tz } from '../i18n';

export function PanelResizeHandle({ side }: { side: 'left' | 'right' }) {
  const key = side === 'left' ? 'leftWidth' : 'rightWidth';
  const width = useSettings((s) => s[key]);
  const drag = useRef<{ x: number; width: number } | null>(null);
  const setWidth = (value: number) => settingsStore.getState().set(key, panelWidth(side, value));
  const bounds = PANEL_WIDTHS[side];
  return <div
    className={'panel-resize ' + side}
    role="separator" tabIndex={0} aria-orientation="vertical"
    aria-label={side === 'left' ? tz('Resize film panel', '调整显影栏宽度') : tz('Resize adjustments panel', '调整参数栏宽度')}
    aria-valuemin={bounds.min} aria-valuemax={bounds.max} aria-valuenow={width}
    title={tz('Drag to resize; double-click to reset', '拖动调整宽度；双击恢复默认宽度')}
    onDoubleClick={() => setWidth(bounds.standard)}
    onKeyDown={(e) => {
      const direction = side === 'left' ? 1 : -1;
      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        e.preventDefault(); e.stopPropagation();
        setWidth(width + (e.key === 'ArrowRight' ? 1 : -1) * direction * (e.shiftKey ? 20 : 4));
      } else if (e.key === 'Home' || e.key === 'End') {
        e.preventDefault(); setWidth(e.key === 'Home' ? bounds.min : bounds.max);
      }
    }}
    onPointerDown={(e) => {
      if (e.button !== 0) return;
      e.preventDefault();
      drag.current = { x: e.clientX, width };
      e.currentTarget.setPointerCapture(e.pointerId);
    }}
    onPointerMove={(e) => {
      if (drag.current) setWidth(drag.current.width + (e.clientX - drag.current.x) * (side === 'left' ? 1 : -1));
    }}
    onPointerUp={() => { drag.current = null; }}
    onPointerCancel={() => { drag.current = null; }}
    onLostPointerCapture={() => { drag.current = null; }}
  />;
}
