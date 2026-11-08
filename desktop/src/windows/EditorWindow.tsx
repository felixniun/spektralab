// EditorWindow.tsx — three regions and a filmstrip (ARCHITECTURE §7.1):
// Film and Print (left) | top bar + canvas + filmstrip | Parameters (right).

import { Canvas } from '../canvas/Canvas';
import { LeftRail } from '../panels/LeftRail';
import { RightRail } from '../panels/RightRail';
import { settingsStore, useSettings } from '../state/settings';
import { PanelResizeHandle } from './PanelResizeHandle';
import { tz } from '../i18n';
import { useSession } from '../state/session';
import { Filmstrip } from './Filmstrip';
import { TopBar } from './TopBar';
import { AboutDialog, HostFailure, SettingsDialog } from './Dialogs';
import { ExportPage } from '../export/ExportPage';

export function EditorWindow() {
  const left = useSettings((s) => s.leftCollapsed);
  const right = useSettings((s) => s.rightCollapsed);
  const strip = useSettings((s) => s.filmstripCollapsed);
  const leftWidth = useSettings((s) => s.leftWidth);
  const rightWidth = useSettings((s) => s.rightWidth);
  const batch = useSession((s) => s.batchExporting);
  return (
    <>
      <div
        className="editor"
        inert={batch}
        style={{ ['--left-col' as string]: left ? '0px' : `${leftWidth}px`, ['--right-col' as string]: right ? '0px' : `${rightWidth}px`, pointerEvents: batch ? 'none' : undefined }}
        data-testid="editor"
      >
        {left ? <div /> : <div className="rail-container"><LeftRail /><PanelResizeHandle side="left" /></div>}
        <main className="centre" style={{ position: 'relative' }}>
          <TopBar />
          <Canvas />
          <button className="filmstrip-toggle" style={{ bottom: strip ? 0 : 'var(--filmstrip-height)' }} aria-expanded={!strip} aria-label={strip ? tz('Show filmstrip', '显示胶片条') : tz('Hide filmstrip', '隐藏胶片条')} title={tz('Toggle filmstrip (Ctrl+Shift+F)', '显示或隐藏胶片条（Ctrl+Shift+F）')} onClick={() => settingsStore.getState().set('filmstripCollapsed', !strip)}>{strip ? '⌃' : '⌄'}</button>
          {!strip && <Filmstrip />}
          <HostFailure />
        </main>
        {right ? <div /> : <div className="rail-container"><RightRail /><PanelResizeHandle side="right" /></div>}
      </div>
      <SettingsDialog />
      <AboutDialog />
      <ExportPage />
    </>
  );
}
