import { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { portDirs, portRoles, portTypes, type GraphicLabel, type GraphicPort, type ModuleGraphic, type PortRole } from '../api/client';
import { ModuleBlock, PORT_COLORS, PortCaption, portColor, portSide, posStyle } from './ModuleBlock';

type Sel = { kind: 'block' } | { kind: 'port'; id: string } | { kind: 'label'; id: string };
type Drag = { kind: 'port' | 'label' | 'resize'; id: string; startX: number; startY: number; w: number; h: number };

const GRID = 5;
const EDGE_SNAP = 8;
const HISTORY = 50;

const PRESETS: { fill: string; header: string }[] = [
  { fill: '#dcfce7', header: '#16a34a' },
  { fill: '#ede9fe', header: '#7c3aed' },
  { fill: '#fef3c7', header: '#d97706' },
  { fill: '#e0f2fe', header: '#0369a1' },
  { fill: '#fce7f3', header: '#be185d' },
  { fill: '#ffedd5', header: '#ea580c' },
  { fill: '#fee2e2', header: '#dc2626' },
  { fill: '#f1f5f9', header: '#475569' },
];

const round = (v: number) => Math.round(v * 1000) / 1000;
const clamp01 = (v: number) => Math.min(1, Math.max(0, v));

function nextId(prefix: string, used: string[]) {
  for (let i = 1; ; i++) if (!used.includes(`${prefix}${i}`)) return `${prefix}${i}`;
}

/** Spreads the points of each side evenly along it, keeping their order. */
function distribute(ports: GraphicPort[]): GraphicPort[] {
  const out = ports.map((p) => ({ ...p }));
  for (const side of ['top', 'bottom', 'left', 'right'] as const) {
    const group = out.filter((p) => portSide(p) === side);
    const horizontal = side === 'top' || side === 'bottom';
    group.sort((a, b) => (horizontal ? a.x - b.x : a.y - b.y));
    group.forEach((p, i) => {
      const v = round((i + 0.5) / group.length);
      if (horizontal) p.x = v;
      else p.y = round(0.2 + v * 0.8);
    });
  }
  return out;
}

type Props = { graphic: ModuleGraphic; article: string; name: string; onChange: (g: ModuleGraphic) => void };

/** Visual editor of a module block: size, colors, texts, connection points and captions dragged with the mouse. */
export function BlockEditor({ graphic: g, article, name, onChange }: Props) {
  const { t } = useTranslation();
  const [sel, setSel] = useState<Sel>({ kind: 'block' });
  const [snap, setSnap] = useState(true);
  const [zoom, setZoom] = useState(() => (g.width <= 200 && g.height <= 200 ? 2 : g.width <= 400 ? 1.5 : 1));
  const [history, setHistory] = useState<ModuleGraphic[]>([]);
  const [initial] = useState(g);
  const blockRef = useRef<HTMLDivElement>(null);
  const drag = useRef<Drag | null>(null);
  const latest = useRef(g);
  latest.current = g;

  const push = () => setHistory((h) => [...h.slice(-HISTORY + 1), latest.current]);
  const commit = (next: ModuleGraphic, remember = true) => {
    if (remember) push();
    onChange(next);
  };
  const undo = () =>
    setHistory((h) => {
      if (!h.length) return h;
      onChange(h[h.length - 1]);
      return h.slice(0, -1);
    });

  const port = sel.kind === 'port' ? g.ports.find((p) => p.id === sel.id) : undefined;
  const label = sel.kind === 'label' ? g.labels.find((l) => l.id === sel.id) : undefined;

  const setPort = (id: string, patch: Partial<GraphicPort>, remember = true) =>
    commit({ ...latest.current, ports: latest.current.ports.map((p) => (p.id === id ? { ...p, ...patch } : p)) }, remember);
  const setLabel = (id: string, patch: Partial<GraphicLabel>, remember = true) =>
    commit({ ...latest.current, labels: latest.current.labels.map((l) => (l.id === id ? { ...l, ...patch } : l)) }, remember);
  const setBlock = (patch: Partial<ModuleGraphic>) => commit({ ...g, ...patch });

  const addPort = () => {
    const id = nextId('p', g.ports.map((p) => p.id));
    commit({ ...g, ports: [...g.ports, { id, label: id.toUpperCase(), type: 'can', dir: 'bi', x: 0.5, y: 1 }] });
    setSel({ kind: 'port', id });
  };
  const addLabel = () => {
    const id = nextId('t', g.labels.map((l) => l.id));
    commit({ ...g, labels: [...g.labels, { id, text: t('blockEditor.newText'), x: 0.5, y: 0.6, size: 11, bold: false }] });
    setSel({ kind: 'label', id });
  };
  const duplicate = () => {
    if (port) {
      const id = nextId('p', g.ports.map((p) => p.id));
      const horizontal = portSide(port) === 'top' || portSide(port) === 'bottom';
      commit({ ...g, ports: [...g.ports, { ...port, id, x: horizontal ? clamp01(port.x + 0.1) : port.x, y: horizontal ? port.y : clamp01(port.y + 0.15) }] });
      setSel({ kind: 'port', id });
    } else if (label) {
      const id = nextId('t', g.labels.map((l) => l.id));
      commit({ ...g, labels: [...g.labels, { ...label, id, y: clamp01(label.y + 0.12) }] });
      setSel({ kind: 'label', id });
    }
  };
  const remove = () => {
    if (port) commit({ ...g, ports: g.ports.filter((p) => p.id !== port.id) });
    else if (label) commit({ ...g, labels: g.labels.filter((l) => l.id !== label.id) });
    setSel({ kind: 'block' });
  };

  // Mouse dragging of points, captions and the block corner.
  useEffect(() => {
    const move = (e: PointerEvent) => {
      const d = drag.current;
      const box = blockRef.current?.getBoundingClientRect();
      if (!d || !box) return;
      const cur = latest.current;
      const z = box.width / cur.width;
      if (d.kind === 'resize') {
        const step = snap ? GRID * 2 : 1;
        const w = Math.round(Math.min(1600, Math.max(80, d.w + (e.clientX - d.startX) / z)) / step) * step;
        const h = Math.round(Math.min(1200, Math.max(50, d.h + (e.clientY - d.startY) / z)) / step) * step;
        if (w !== cur.width || h !== cur.height) onChange({ ...cur, width: w, height: h });
        return;
      }
      // Block units, independent of the editor zoom.
      let px = Math.min(cur.width, Math.max(0, (e.clientX - box.left) / z));
      let py = Math.min(cur.height, Math.max(0, (e.clientY - box.top) / z));
      if (snap) {
        px = Math.round(px / GRID) * GRID;
        py = Math.round(py / GRID) * GRID;
      }
      if (d.kind === 'port') {
        // Points stick to the nearest edge when close to it, so wires leave the block cleanly.
        if (px < EDGE_SNAP) px = 0;
        if (px > cur.width - EDGE_SNAP) px = cur.width;
        if (py < EDGE_SNAP) py = 0;
        if (py > cur.height - EDGE_SNAP) py = cur.height;
        setPort(d.id, { x: round(px / cur.width), y: round(py / cur.height) }, false);
      } else setLabel(d.id, { x: round(px / cur.width), y: round(py / cur.height) }, false);
    };
    const up = () => (drag.current = null);
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    return () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [snap]);

  const startDrag = (e: React.PointerEvent, kind: Drag['kind'], id: string) => {
    e.preventDefault();
    e.stopPropagation();
    push();
    drag.current = { kind, id, startX: e.clientX, startY: e.clientY, w: g.width, h: g.height };
    if (kind === 'port') setSel({ kind: 'port', id });
    if (kind === 'label') setSel({ kind: 'label', id });
  };

  const onKey = (e: React.KeyboardEvent) => {
    if ((e.target as HTMLElement).closest('input, select, textarea')) return;
    if ((e.key === 'Delete' || e.key === 'Backspace') && sel.kind !== 'block') {
      e.preventDefault();
      remove();
    }
    if (e.key === 'z' && (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      undo();
    }
    const arrows: Record<string, [number, number]> = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
    const a = arrows[e.key];
    if (a && (port || label)) {
      e.preventDefault();
      const stepX = (e.shiftKey ? 10 : 1) / g.width;
      const stepY = (e.shiftKey ? 10 : 1) / g.height;
      const target = (port ?? label)!;
      const patch = { x: round(clamp01(target.x + a[0] * stepX)), y: round(clamp01(target.y + a[1] * stepY)) };
      if (port) setPort(port.id, patch);
      else setLabel(label!.id, patch);
    }
  };

  const color = (value: string, onValue: (v: string) => void) => (
    <span className="color-field">
      <input type="color" value={value} onChange={(e) => onValue(e.target.value)} />
      <input dir="ltr" value={value} maxLength={7} onChange={(e) => /^#[0-9a-fA-F]{6}$/.test(e.target.value) && onValue(e.target.value)} />
    </span>
  );

  const pct = (v: number) => Math.round(v * 1000) / 10;

  return (
    <div className="block-editor" tabIndex={0} onKeyDown={onKey}>
      <div className="block-editor__toolbar">
        <button type="button" className="btn btn-small" onClick={addPort}>
          + {t('blockEditor.addPort')}
        </button>
        <button type="button" className="btn btn-small" onClick={addLabel}>
          + {t('blockEditor.addLabel')}
        </button>
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={sel.kind === 'block'} onClick={duplicate}>
          {t('blockEditor.duplicate')}
        </button>
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={sel.kind === 'block'} onClick={remove}>
          {t('common.delete')}
        </button>
        <span className="block-editor__sep" />
        <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => commit({ ...g, ports: distribute(g.ports) })}>
          {t('blockEditor.distribute')}
        </button>
        <label className="checkbox-row small">
          <input type="checkbox" checked={snap} onChange={(e) => setSnap(e.target.checked)} />
          {t('blockEditor.snap')}
        </label>
        <span className="block-editor__sep" />
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={zoom <= 0.5} onClick={() => setZoom((z) => Math.max(0.5, z - 0.25))}>
          −
        </button>
        <span className="small ltr-value" title={t('blockEditor.zoom')}>
          {Math.round(zoom * 100)}%
        </span>
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={zoom >= 3} onClick={() => setZoom((z) => Math.min(3, z + 0.25))}>
          +
        </button>
        <span className="block-editor__sep" />
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={!history.length} onClick={undo} title="Ctrl+Z">
          ↶ {t('blockEditor.undo')}
        </button>
        <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => commit(initial)}>
          {t('blockEditor.revert')}
        </button>
      </div>

      <div className="block-editor__main">
        <div className="block-editor__canvas" dir="ltr" onPointerDown={() => setSel({ kind: 'block' })}>
          <div ref={blockRef} className="block-editor__block" style={{ width: g.width * zoom, height: g.height * zoom }}>
            <ModuleBlock
              graphic={g}
              article={article}
              name={name}
              scale={zoom}
              selected={sel.kind === 'block'}
              renderPort={(p) => (
                <div
                  key={p.id}
                  className={`mport mport--edit${sel.kind === 'port' && sel.id === p.id ? ' mport--selected' : ''}`}
                  style={posStyle(p.x, p.y)}
                  onPointerDown={(e) => startDrag(e, 'port', p.id)}
                  title={`${p.label} · ${t(`blockEditor.types.${p.type}`)}`}
                >
                  <span className={`mport__dot mport__dot--${p.dir}`} style={{ background: portColor(p) }} />
                  <PortCaption port={p} scale={zoom} />
                </div>
              )}
              renderLabel={(l) => (
                <span
                  key={l.id}
                  className={`mblock__label mblock__label--edit${sel.kind === 'label' && sel.id === l.id ? ' mblock__label--selected' : ''}`}
                  style={{ ...posStyle(l.x, l.y), color: l.color || undefined, fontSize: l.size * zoom, fontWeight: l.bold ? 700 : 400 }}
                  onPointerDown={(e) => startDrag(e, 'label', l.id)}
                >
                  {l.text || '…'}
                </span>
              )}
            />
            <span className="block-editor__resize" onPointerDown={(e) => startDrag(e, 'resize', '')} title={t('blockEditor.resize')} />
          </div>
          <div className="block-editor__size muted small">
            {g.width} × {g.height}
          </div>
        </div>

        <div className="block-editor__props">
          {sel.kind === 'block' && (
            <>
              <h4>{t('blockEditor.block')}</h4>
              <div className="form-grid-2">
                <label>
                  {t('blockEditor.width')}
                  <input type="number" dir="ltr" min={80} max={1600} value={g.width} onChange={(e) => setBlock({ width: Number(e.target.value) || 80 })} />
                </label>
                <label>
                  {t('blockEditor.height')}
                  <input type="number" dir="ltr" min={50} max={1200} value={g.height} onChange={(e) => setBlock({ height: Number(e.target.value) || 50 })} />
                </label>
              </div>
              <label>
                {t('blockEditor.title')}
                <input value={g.title} placeholder={article} maxLength={40} onChange={(e) => setBlock({ title: e.target.value })} />
              </label>
              <label>
                {t('blockEditor.subtitle')}
                <input value={g.subtitle} maxLength={80} onChange={(e) => setBlock({ subtitle: e.target.value })} />
              </label>
              <label className="checkbox-row">
                <input type="checkbox" checked={g.showArticle} onChange={(e) => setBlock({ showArticle: e.target.checked })} />
                {t('blockEditor.showArticle')}
              </label>
              <div className="block-editor__presets">
                {PRESETS.map((p) => (
                  <button
                    key={p.header}
                    type="button"
                    className="preset-swatch"
                    style={{ background: p.fill, borderColor: p.header }}
                    onClick={() => setBlock({ fill: p.fill, header: p.header, border: p.header, headerText: '#ffffff' })}
                  >
                    <span style={{ background: p.header }} />
                  </button>
                ))}
              </div>
              <div className="block-editor__colors">
                <label>
                  {t('blockEditor.fill')}
                  {color(g.fill, (v) => setBlock({ fill: v }))}
                </label>
                <label>
                  {t('blockEditor.header')}
                  {color(g.header, (v) => setBlock({ header: v }))}
                </label>
                <label>
                  {t('blockEditor.headerText')}
                  {color(g.headerText, (v) => setBlock({ headerText: v }))}
                </label>
                <label>
                  {t('blockEditor.textColor')}
                  {color(g.textColor, (v) => setBlock({ textColor: v }))}
                </label>
                <label>
                  {t('blockEditor.border')}
                  {color(g.border, (v) => setBlock({ border: v }))}
                </label>
              </div>
              <p className="muted small">{t('blockEditor.hint')}</p>
            </>
          )}

          {port && (
            <>
              <h4>
                {t('blockEditor.port')} <span className="ltr-value muted">{port.id}</span>
              </h4>
              <label>
                {t('blockEditor.label')}
                <input dir="ltr" value={port.label} maxLength={32} onChange={(e) => setPort(port.id, { label: e.target.value })} />
              </label>
              <div className="form-grid-2">
                <label>
                  {t('blockEditor.type')}
                  <select value={port.type} onChange={(e) => setPort(port.id, { type: e.target.value as GraphicPort['type'] })}>
                    {portTypes.map((pt) => (
                      <option key={pt} value={pt}>
                        {t(`blockEditor.types.${pt}`)}
                      </option>
                    ))}
                  </select>
                </label>
                <label>
                  {t('blockEditor.dir')}
                  <select value={port.dir} onChange={(e) => setPort(port.id, { dir: e.target.value as GraphicPort['dir'] })}>
                    {portDirs.map((d) => (
                      <option key={d} value={d}>
                        {t(`blockEditor.dirs.${d}`)}
                      </option>
                    ))}
                  </select>
                </label>
              </div>
              <label>
                {t('blockEditor.role')}
                <select
                  value={port.role ?? ''}
                  onChange={(e) => {
                    const role = (e.target.value || null) as PortRole | null;
                    setPort(port.id, { role, line: role === 'line' ? (port.line ?? 1) : null, index: role === 'hubPort' ? (port.index ?? 1) : null });
                  }}
                >
                  <option value="">{t('blockEditor.roleNone')}</option>
                  {portRoles.map((r) => (
                    <option key={r} value={r}>
                      {t(`blockEditor.roles.${r}`)}
                    </option>
                  ))}
                </select>
                <span className="field-hint">{t(`blockEditor.roleHints.${port.role || 'none'}`)}</span>
              </label>
              {port.role === 'line' && (
                <label>
                  {t('blockEditor.line')}
                  <select value={port.line ?? 1} onChange={(e) => setPort(port.id, { line: Number(e.target.value) })}>
                    {[1, 2, 3].map((n) => (
                      <option key={n} value={n}>
                        {t(`hardware.lines.${n}`)}
                      </option>
                    ))}
                  </select>
                </label>
              )}
              {port.role === 'hubPort' && (
                <label>
                  {t('blockEditor.index')}
                  <input type="number" dir="ltr" min={1} max={9} value={port.index ?? 1} onChange={(e) => setPort(port.id, { index: Number(e.target.value) })} />
                </label>
              )}
              <label>
                {t('blockEditor.color')}
                <span className="color-field">
                  <label className="checkbox-row small">
                    <input
                      type="checkbox"
                      checked={!!port.color}
                      onChange={(e) => setPort(port.id, { color: e.target.checked ? PORT_COLORS[port.type] : null })}
                    />
                    {t('blockEditor.ownColor')}
                  </label>
                  {port.color && <input type="color" value={port.color} onChange={(e) => setPort(port.id, { color: e.target.value })} />}
                </span>
              </label>
              <div className="form-grid-2">
                <label>
                  X, %
                  <input type="number" dir="ltr" min={0} max={100} step={0.5} value={pct(port.x)} onChange={(e) => setPort(port.id, { x: clamp01(Number(e.target.value) / 100) })} />
                </label>
                <label>
                  Y, %
                  <input type="number" dir="ltr" min={0} max={100} step={0.5} value={pct(port.y)} onChange={(e) => setPort(port.id, { y: clamp01(Number(e.target.value) / 100) })} />
                </label>
              </div>
            </>
          )}

          {label && (
            <>
              <h4>{t('blockEditor.labelText')}</h4>
              <label>
                {t('blockEditor.text')}
                <input value={label.text} maxLength={80} onChange={(e) => setLabel(label.id, { text: e.target.value })} />
              </label>
              <div className="form-grid-2">
                <label>
                  {t('blockEditor.fontSize')}
                  <input type="number" dir="ltr" min={8} max={32} value={label.size} onChange={(e) => setLabel(label.id, { size: Number(e.target.value) || 11 })} />
                </label>
                <label className="checkbox-row">
                  <input type="checkbox" checked={label.bold} onChange={(e) => setLabel(label.id, { bold: e.target.checked })} />
                  {t('blockEditor.bold')}
                </label>
              </div>
              <label>
                {t('blockEditor.color')}
                {color(label.color || g.textColor, (v) => setLabel(label.id, { color: v }))}
              </label>
            </>
          )}
        </div>
      </div>

      <div className="block-editor__list">
        <table className="data-table compact">
          <thead>
            <tr>
              <th />
              <th>{t('blockEditor.label')}</th>
              <th>{t('blockEditor.type')}</th>
              <th>{t('blockEditor.dir')}</th>
              <th>{t('blockEditor.role')}</th>
            </tr>
          </thead>
          <tbody>
            {g.ports.map((p) => (
              <tr key={p.id} className={sel.kind === 'port' && sel.id === p.id ? 'row-selected' : ''} onClick={() => setSel({ kind: 'port', id: p.id })}>
                <td>
                  <span className={`mport__dot mport__dot--${p.dir} mport__dot--inline`} style={{ background: portColor(p) }} />
                </td>
                <td className="ltr-value">{p.label}</td>
                <td>{t(`blockEditor.types.${p.type}`)}</td>
                <td>{t(`blockEditor.dirs.${p.dir}`)}</td>
                <td className="small">
                  {p.role ? t(`blockEditor.roles.${p.role}`) : ''}
                  {p.role === 'line' && p.line ? ` ${p.line}` : ''}
                  {p.role === 'hubPort' && p.index ? ` ${p.index}` : ''}
                </td>
              </tr>
            ))}
            {g.ports.length === 0 && (
              <tr>
                <td colSpan={5} className="muted">
                  {t('blockEditor.noPorts')}
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
