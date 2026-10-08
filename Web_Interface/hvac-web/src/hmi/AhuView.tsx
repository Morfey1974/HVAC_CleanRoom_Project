import { useRef, useState, type PointerEvent } from 'react';
import { useTranslation } from 'react-i18next';
import type { HmiAhuItem } from '../api/client';
import { pickName } from '../lib/localized';
import { artLibrary, Box, slotsFor, demoValues, DX, DY, formatSlot, HmiDefs, P, pts, stateOf, type HmiState } from './arts';

export const CASE_H = 150;
const PAD_L = 48;
const PAD_R = 78;
const TOP = DY + 34;
const BOTTOM = 76;
const ZOOM = 1.4;

type Props = {
  items: HmiAhuItem[];
  values?: Record<string, number>;
  flowTag?: string;
  /** Library preview: every section shown in this state with sample values. */
  demo?: HmiState;
  selected?: string | null;
  onSelect?: (uid: string | null) => void;
  /** Drag reorder: section moved from one place to another. */
  onMove?: (from: number, to: number) => void;
  showNames?: boolean;
  /** Without the intake hood and the supply duct (library cards). */
  bare?: boolean;
};

type Drag = { uid: string; from: number; startX: number; dx: number };

/** Air handling unit seen at an angle: glass casing, sections glued in airflow order, units drawn inside. */
export function AhuView({ items, values, flowTag, demo, selected, onSelect, onMove, showNames = true, bare = false }: Props) {
  const { i18n } = useTranslation();
  const [drag, setDrag] = useState<Drag | null>(null);
  const svgRef = useRef<SVGSVGElement>(null);
  const H = CASE_H;

  const xs: number[] = [];
  let T = 0;
  for (const it of items) {
    xs.push(T);
    T += it.length;
  }
  T = Math.max(T, 60);
  const padL = bare ? 8 : PAD_L;
  const padR = (bare ? 8 : PAD_R) + DX;
  const width = padL + T + padR;
  const height = TOP + H + BOTTOM;
  const air = demo ? (demo === 'stopped' ? 0 : 80) : flowTag && values ? (values[flowTag] ?? 0) : 0;
  const hood = !bare && items[0]?.art === 'inlet';
  const duct = !bare && items[items.length - 1]?.art === 'outlet';

  const scale = () => {
    const r = svgRef.current?.getBoundingClientRect();
    return r && r.width ? width / r.width : 1;
  };

  const dropIndex = (d: Drag) => {
    const center = xs[d.from] + items[d.from].length / 2 + d.dx;
    let idx = 0;
    for (let i = 0; i < items.length; i++) if (i !== d.from && xs[i] + items[i].length / 2 < center) idx++;
    return idx;
  };

  const down = (uid: string, from: number) => (e: PointerEvent<SVGGElement>) => {
    e.stopPropagation();
    onSelect?.(uid);
    if (!onMove) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    setDrag({ uid, from, startX: e.clientX, dx: 0 });
  };
  const move = (e: PointerEvent<SVGGElement>) => drag && setDrag({ ...drag, dx: (e.clientX - drag.startX) * scale() });
  const up = () => {
    if (!drag) return;
    const to = dropIndex(drag);
    setDrag(null);
    if (Math.abs(drag.dx) > 4 && to !== drag.from) onMove?.(drag.from, to);
  };

  let marker: number | null = null;
  if (drag && Math.abs(drag.dx) > 4) {
    const to = dropIndex(drag);
    marker = items.filter((_, i) => i !== drag.from).slice(0, to).reduce((s, it) => s + it.length, 0);
  }

  const order = items.map((it, i) => [it, i] as const).sort((a, b) => Number(a[0].uid === drag?.uid) - Number(b[0].uid === drag?.uid));

  return (
    <div className="hmi-canvas">
      <svg ref={svgRef} viewBox={`0 0 ${width} ${height}`} width={width * ZOOM} className="hmi-svg" onPointerDown={() => onSelect?.(null)}>
        <HmiDefs />
        <g transform={`translate(${padL} ${TOP})`}>
          <polygon points={pts([[-30, H + 14, 0], [T + 30, H + 14, 0], [T + 30, H + 14, 1.2], [-30, H + 14, 1.2]])} fill="rgba(100,116,139,0.12)" />
          <Box x0={-3} x1={T + 3} y0={H} y1={H + 12} z0={0} z1={1} front="#94a3b8" top="#cbd5e1" side="#64748b" stroke="#475569" />
          {hood && (
            <g>
              <Box x0={-34} x1={0} y0={12} y1={H - 12} z0={0.12} z1={0.88} front="#e2e8f0" top="#f1f5f9" side="#cbd5e1" stroke="#94a3b8" />
              {Array.from({ length: 6 }, (_, i) => {
                const y = 22 + i * ((H - 44) / 5);
                const [x1, y1] = P(-31, y, 0.12);
                const [x2, y2] = P(-3, y + 6, 0.12);
                return <line key={i} x1={x1} y1={y1} x2={x2} y2={y2} stroke="#64748b" strokeWidth={2.5} />;
              })}
            </g>
          )}

          <polygon points={pts([[0, 0, 1], [T, 0, 1], [T, H, 1], [0, H, 1]])} fill="#e8eef5" stroke="#94a3b8" />
          <polygon points={pts([[0, H, 0], [T, H, 0], [T, H, 1], [0, H, 1]])} fill="#d5dde8" stroke="#94a3b8" />
          {xs.slice(1).map((x) => (
            <g key={x} stroke="#b6c2d1">
              <line x1={P(x, 0, 1)[0]} y1={P(x, 0, 1)[1]} x2={P(x, H, 1)[0]} y2={P(x, H, 1)[1]} />
              <line x1={P(x, H, 0)[0]} y1={P(x, H, 0)[1]} x2={P(x, H, 1)[0]} y2={P(x, H, 1)[1]} />
            </g>
          ))}

          {order.map(([it, i]) => {
            const v: Record<string, number | undefined> = {};
            if (demo) Object.assign(v, demoValues(it.art, demo, it.params));
            else for (const [slot, tag] of Object.entries(it.bind ?? {})) if (tag && values) v[slot] = values[tag];
            const state = demo ?? stateOf(it.art, v, air);
            const R = artLibrary[it.art];
            const dx = drag?.uid === it.uid ? drag.dx : 0;
            const w = it.length;
            const shown = slotsFor(it.art, it.params ?? {}).filter((s) => s.show && (demo || it.bind?.[s.key]));
            const front = pts([[0, 0, 0], [w, 0, 0], [w, H, 0], [0, H, 0]]);
            return (
              <g
                key={it.uid}
                transform={`translate(${xs[i] + dx} 0)`}
                className={onSelect ? 'hmi-section hmi-section--edit' : 'hmi-section'}
                onPointerDown={onSelect ? down(it.uid, i) : undefined}
                onPointerMove={onMove ? move : undefined}
                onPointerUp={onMove ? up : undefined}
                opacity={dx ? 0.85 : 1}
              >
                <polygon points={pts([[0, H, 0], [0, 0, 0], [0, 0, 1], [w, 0, 1], [w, H, 1], [w, H, 0]])} fill="transparent" />
                {R && <R w={w} h={H} v={v} p={it.params ?? {}} state={state} air={air} />}
                <rect x={5} y={6} width={w - 10} height={H - 12} rx={3} fill="none" stroke="rgba(100,116,139,0.35)" />
                <rect x={w - 11} y={H * 0.3} width={4} height={12} rx={1.5} fill="#475569" opacity={0.7} />
                <rect x={w - 11} y={H * 0.62} width={4} height={12} rx={1.5} fill="#475569" opacity={0.7} />
                {state === 'alarm' && <polygon points={front} fill="rgba(239,68,68,0.12)" stroke="#ef4444" strokeWidth={3} className="mn-alarm" />}
                {selected === it.uid && <polygon points={front} fill="rgba(34,211,238,0.08)" stroke="#0891b2" strokeWidth={3} strokeDasharray="7 4" />}
                {showNames && (
                  <text x={w / 2 + DX / 2} y={-DY - 10} className="hmi-name" textAnchor="middle">
                    {short(pickName(it.name, i18n.language).value, w)}
                  </text>
                )}
                {shown.map((s, k) => (
                  <g key={s.key} transform={`translate(${w / 2} ${H + 32 + k * 19})`}>
                    <rect x={-28} y={-12} width={56} height={17} rx={4} fill="#fff" stroke="#cbd5e1" />
                    <text y={1} className="hmi-val" textAnchor="middle">
                      {formatSlot(v[s.key], s)}
                    </text>
                  </g>
                ))}
              </g>
            );
          })}

          <g pointerEvents="none">
          <polygon points={pts([[0, 0, 0], [T, 0, 0], [T, 0, 1], [0, 0, 1]])} fill="rgba(241,245,249,0.35)" stroke="#94a3b8" strokeWidth={2} />
          <polygon points={pts([[T, 0, 0], [T, 0, 1], [T, H, 1], [T, H, 0]])} fill="rgba(203,213,225,0.45)" stroke="#94a3b8" strokeWidth={2} />
          <rect x={0} y={0} width={T} height={H} fill="rgba(255,255,255,0.12)" stroke="#94a3b8" strokeWidth={3} />
          {xs.slice(1).map((x) => (
            <g key={x} stroke="#94a3b8" strokeWidth={2}>
              <line x1={x} y1={0} x2={x} y2={H} />
              <line x1={x} y1={0} x2={P(x, 0, 1)[0]} y2={P(x, 0, 1)[1]} />
            </g>
          ))}
          </g>

          {duct && (
            <g>
              <Box x0={T} x1={T + 46} y0={H * 0.28} y1={H * 0.72} z0={0.28} z1={0.72} front="url(#hmiDuct)" top="#f1f5f9" side="#cbd5e1" stroke="#94a3b8" />
              {(() => {
                const [x, y] = P(T + 8, H * 0.5, 0.28);
                return (
                  <g transform={`translate(${x} ${y})`}>
                    <rect x={-2} y={-11} width={36} height={22} rx={2} fill="#1d4ed8" />
                    <text x={16} y={-2} textAnchor="middle" className="hmi-plate">
                      SUPPLY
                    </text>
                    <text x={16} y={7} textAnchor="middle" className="hmi-plate">
                      AIR
                    </text>
                  </g>
                );
              })()}
            </g>
          )}

          {marker !== null && <line x1={marker} y1={-DY - 4} x2={marker} y2={H + 16} stroke="#0891b2" strokeWidth={4} />}
        </g>
      </svg>
    </div>
  );
}

const short = (s: string, len: number) => {
  const max = Math.max(4, Math.floor(len / 6));
  return s.length > max ? `${s.slice(0, max - 1)}…` : s;
};
