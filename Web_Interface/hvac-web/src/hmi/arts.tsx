import type { ReactElement, SVGProps } from 'react';
import { useSpin } from '../mnemo/animation';
import type { HmiParams } from '../api/client';

export type HmiState = 'stopped' | 'running' | 'alarm';
export const hmiStates: HmiState[] = ['stopped', 'running', 'alarm'];

/** Oblique view: depth z 0 (front glass) … 1 (back wall) goes up and to the right. */
export const DX = 64;
export const DY = 40;
export type Pt3 = [number, number, number];
export const P = (x: number, y: number, z: number): [number, number] => [x + z * DX, y - z * DY];
export const pts = (list: Pt3[]) => list.map(([x, y, z]) => P(x, y, z).join(',')).join(' ');

export type ArtProps = { w: number; h: number; v: Record<string, number | undefined>; p: HmiParams; state: HmiState; air: number };
type Art = (props: ArtProps) => ReactElement;

/** A value the picture can show; the engineer binds it to a live tag. */
export type Slot = { key: string; unit?: string; decimals?: number; show?: boolean };

export const artSlots: Record<string, Slot[]> = {
  inlet: [{ key: 't', unit: '°C', show: true }],
  filter: [{ key: 'dp', unit: 'Pa', decimals: 0, show: true }, { key: 'dirt', unit: '%', decimals: 0, show: true }],
  coil: [{ key: 'on' }, { key: 'mode' }, { key: 'cap', unit: '%', decimals: 0, show: true }, { key: 'fault' }],
  heater: [{ key: 'stages', decimals: 0, show: true }, { key: 'trip' }],
  fan: [{ key: 'run' }, { key: 'speed', unit: '%', decimals: 0, show: true }, { key: 'fault' }, { key: 'flow' }],
  outlet: [{ key: 't', unit: '°C', show: true }, { key: 'p', unit: 'Pa', decimals: 0, show: true }, { key: 'cfm', unit: 'cfm', decimals: 0, show: true }],
  humidifier: [{ key: 'on' }, { key: 'rh', unit: '%', decimals: 0, show: true }],
  damper: [{ key: 'open', unit: '%', decimals: 0, show: true }],
  vrf: [{ key: 'on' }, { key: 'fault' }],
  ffu: [{ key: 'run' }],
  acUnit: [{ key: 'run' }],
  roomTile: [{ key: 't', unit: '°C', show: true }, { key: 'rh', unit: '%', decimals: 0, show: true }, { key: 'dp', unit: 'Pa', decimals: 0, show: true }],
};

export const damperSupplies = ['24V AC/DC', '24V AC', '24V DC', '230V AC'];
export const damperControls = ['0-10V', '2-10V', 'open/close'];
export const damperFeedbacks = ['0-10V', '2-10V', 'none'];

/** Slots of one picture; a damper depends on its drive: manual has none, a motorised one has command and feedback. */
export function slotsFor(art: string, p: HmiParams): Slot[] {
  if (art !== 'damper') return artSlots[art] ?? [];
  if (p.drive !== 'motor') return [];
  const s: Slot[] = [{ key: 'cmd', unit: '%', decimals: 0, show: true }];
  if (p.feedback !== 'none') s.push({ key: 'pos', unit: '%', decimals: 0, show: true });
  s.push({ key: 'fault' });
  return s;
}

export const arts = Object.keys(artSlots);
export const sectionArts = ['inlet', 'filter', 'coil', 'heater', 'fan', 'outlet', 'humidifier', 'damper'];

const pos = (x: number | undefined) => (x ?? 0) > 0;
const clamp01 = (x: number | undefined, max = 100) => Math.max(0, Math.min(max, x ?? 0)) / max;

export function formatSlot(v: number | undefined, s: Slot) {
  if (v === undefined || Number.isNaN(v)) return '— —';
  return `${v.toFixed(s.decimals ?? 1)}${s.unit ? ` ${s.unit}` : ''}`;
}

/** Picture state from its live values; air = supply airflow 0–100 %. */
export function stateOf(art: string, v: Record<string, number | undefined>, air: number): HmiState {
  if (pos(v.fault) || pos(v.trip) || (art === 'filter' && (v.dirt ?? 0) >= 80)) return 'alarm';
  const own = pos(v.run) || pos(v.on) || pos(v.stages) || (art === 'damper' && (pos(v.pos) || pos(v.cmd) || pos(v.open)));
  return own || (air > 0 && ['inlet', 'outlet', 'filter'].includes(art)) ? 'running' : 'stopped';
}

/** Values for library previews, so every picture can be seen stopped, running and in alarm. */
export function demoValues(art: string, s: HmiState, p: HmiParams): Record<string, number> {
  const run = s !== 'stopped' ? 1 : 0;
  const bad = s === 'alarm' ? 1 : 0;
  switch (art) {
    case 'filter': return { dp: run * 120, dirt: s === 'alarm' ? 90 : 35 };
    case 'coil': return { on: run, mode: 1, cap: run * 70, fault: bad };
    case 'heater': return { stages: run * (p.stages ?? 3), trip: bad };
    case 'fan': return { run, speed: run * 80, fault: bad, flow: run };
    case 'inlet': return { t: 12 };
    case 'outlet': return { t: 18, p: run * 160, cfm: run * 600 };
    case 'humidifier': return { on: run, rh: 45, fault: bad };
    case 'damper': return p.drive === 'motor' ? { cmd: run * 100, pos: run * 100, fault: bad } : { open: run ? 100 : 30 };
    case 'roomTile': return { t: s === 'alarm' ? 25.4 : 21.2, rh: 45, dp: 15 };
    default: return { on: run, run, fault: bad };
  }
}

// ---------- 3D primitives ----------

type Faces = { front: string; top: string; side: string; stroke?: string; strokeWidth?: number };

/** Solid box; visible faces in this view are the front, the top and the right side. */
export function Box({ x0, x1, y0, y1, z0, z1, front, top, side, stroke = '#64748b', strokeWidth = 1 }: Faces & { x0: number; x1: number; y0: number; y1: number; z0: number; z1: number }) {
  return (
    <g stroke={stroke} strokeWidth={strokeWidth} strokeLinejoin="round">
      <polygon points={pts([[x1, y0, z0], [x1, y0, z1], [x1, y1, z1], [x1, y1, z0]])} fill={side} />
      <polygon points={pts([[x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1]])} fill={top} />
      <polygon points={pts([[x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0]])} fill={front} />
    </g>
  );
}

/** Flat panel across the air stream (plane x = const). */
function XPlane({ x, y0, y1, z0, z1, ...rest }: { x: number; y0: number; y1: number; z0: number; z1: number } & SVGProps<SVGPolygonElement>) {
  return <polygon points={pts([[x, y0, z0], [x, y0, z1], [x, y1, z1], [x, y1, z0]])} {...rest} />;
}

function Line3({ a, b, ...rest }: { a: Pt3; b: Pt3 } & SVGProps<SVGLineElement>) {
  const [x1, y1] = P(...a);
  const [x2, y2] = P(...b);
  return <line x1={x1} y1={y1} x2={x2} y2={y2} {...rest} />;
}

/** Wide flat air arrow lying in the middle of the stream, like on cutaway drawings. */
export function AirArrow({ x0, x1, y, color, on }: { x0: number; x1: number; y: number; color: string; on: boolean }) {
  const hd = Math.min(18, (x1 - x0) * 0.4);
  return (
    <g className={on ? 'hmi-arrow hmi-arrow--on' : 'hmi-arrow'}>
      <polygon
        points={pts([[x0, y, 0.4], [x1 - hd, y, 0.4], [x1 - hd, y, 0.28], [x1, y, 0.5], [x1 - hd, y, 0.72], [x1 - hd, y, 0.6], [x0, y, 0.6]])}
        fill={color}
        stroke="rgba(0,0,0,0.15)"
        opacity={on ? 0.9 : 0.25}
      />
    </g>
  );
}

const Label = ({ at, text }: { at: Pt3; text?: string }) => {
  if (!text) return null;
  const [x, y] = P(...at);
  return (
    <g transform={`translate(${x} ${y})`}>
      <rect x={-17} y={-9} width={34} height={17} rx={4} fill="#fff" stroke="#94a3b8" />
      <text y={4} textAnchor="middle" className="hmi-chip-text">
        {text}
      </text>
    </g>
  );
};

// ---------- sections ----------

const Inlet: Art = ({ w, h, air }) => (
  <g>
    <XPlane x={10} y0={10} y1={h - 8} z0={0.05} z1={0.95} fill="rgba(203,213,225,0.55)" stroke="#64748b" strokeWidth={2} />
    {Array.from({ length: 7 }, (_, i) => {
      const y = 18 + i * ((h - 34) / 6);
      return <Line3 key={i} a={[10, y, 0.07]} b={[10, y, 0.93]} stroke="#94a3b8" strokeWidth={4} />;
    })}
    <AirArrow x0={18} x1={w - 4} y={h * 0.55} color="#7dd3fc" on={air > 0} />
  </g>
);

const Outlet: Art = ({ w, h, air }) => (
  <g>
    <AirArrow x0={4} x1={w - 14} y={h * 0.55} color="#fb923c" on={air > 0} />
    <XPlane x={w - 8} y0={h * 0.22} y1={h * 0.78} z0={0.25} z1={0.75} fill="rgba(203,213,225,0.5)" stroke="#64748b" strokeWidth={2} />
  </g>
);

const Filter: Art = ({ w, h, v, p, state }) => {
  const t = Math.min(18, w * 0.35);
  const x0 = (w - t) / 2 - 8, x1 = x0 + t;
  const y0 = 12, y1 = h - 8, z0 = 0.06, z1 = 0.94;
  const dirt = clamp01(v.dirt);
  const n = 16;
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={z0} z1={z1} front="#22c55e" top="#4ade80" side="#f8fafc" stroke="#15803d" strokeWidth={1.5} />
      {Array.from({ length: n + 1 }, (_, i) => {
        const z = z0 + 0.03 + ((z1 - z0 - 0.06) * i) / n;
        return <Line3 key={i} a={[x1, y0 + 5, z]} b={[x1, y1 - 5, z]} stroke={i % 2 ? '#cbd5e1' : '#e2e8f0'} strokeWidth={2.5} />;
      })}
      {dirt > 0 && <XPlane x={x1} y0={y1 - (y1 - y0) * dirt} y1={y1} z0={z0} z1={z1} fill="#92400e" opacity={0.35} className="mn-transition" />}
      <XPlane x={x1} y0={y0} y1={y1} z0={z0} z1={z1} fill="none" stroke={state === 'alarm' ? '#ef4444' : '#16a34a'} strokeWidth={3} />
      <Label at={[x1, h / 2, 0.5]} text={p.label} />
    </g>
  );
};

const Coil: Art = ({ w, h, v, p }) => {
  const x0 = w * 0.32, x1 = w * 0.68, y0 = 14, y1 = h - 8, z0 = 0.06, z1 = 0.94;
  const cap = clamp01(v.cap);
  const on = pos(v.on);
  const tint = on ? (v.mode === 2 ? `rgba(239,68,68,${0.15 + cap * 0.45})` : `rgba(59,130,246,${0.15 + cap * 0.45})`) : 'none';
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={z0} z1={z1} front="#cbd5e1" top="#e2e8f0" side="#d1d5db" stroke="#64748b" />
      {Array.from({ length: 26 }, (_, i) => {
        const z = z0 + 0.02 + ((z1 - z0 - 0.04) * i) / 25;
        return <Line3 key={i} a={[x1, y0 + 3, z]} b={[x1, y1 - 3, z]} stroke="#9ca3af" strokeWidth={0.8} />;
      })}
      {Array.from({ length: 5 }, (_, i) => {
        const z = z0 + 0.08 + ((z1 - z0 - 0.16) * i) / 4;
        const [cx, cy] = P((x0 + x1) / 2, y0, z);
        return <ellipse key={i} cx={cx} cy={cy} rx={(x1 - x0) / 2 - 2} ry={3} fill="none" stroke="#c2410c" strokeWidth={2} />;
      })}
      <XPlane x={x1} y0={y0} y1={y1} z0={z0} z1={z1} fill={tint} className="mn-transition" />
      <Line3 a={[x0 + 5, y0 + 4, 0]} b={[x0 + 5, y1 - 4, 0]} stroke="#b45309" strokeWidth={4} />
      <Line3 a={[x0 + 5, y0 + 18, 0]} b={[x0 + 5, y0 + 18, -0.45]} stroke="#b45309" strokeWidth={4} />
      <Line3 a={[x0 + 5, y1 - 18, 0]} b={[x0 + 5, y1 - 18, -0.45]} stroke="#b45309" strokeWidth={4} />
      {(() => {
        const [vx, vy] = P(x0 + 5, y0 + 18, -0.45);
        return (
          <g transform={`translate(${vx} ${vy})`}>
            <line x1={0} y1={0} x2={0} y2={-16} stroke="#475569" strokeWidth={2} />
            <ellipse cy={-18} rx={7} ry={3} fill="#22c55e" stroke="#15803d" />
          </g>
        );
      })()}
      <Label at={[x1, h / 2, 0.5]} text={p.label ?? 'DX'} />
    </g>
  );
};

const Heater: Art = ({ w, h, v, p }) => {
  const stages = Math.max(1, Math.min(6, p.stages ?? 3));
  const x0 = w * 0.28, x1 = w * 0.72, y0 = 14, y1 = h - 8, z0 = 0.06, z1 = 0.94;
  const rows = stages * 3;
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={z0} z1={z1} front="#e5e7eb" top="#f3f4f6" side="#f8fafc" stroke="#9ca3af" />
      {Array.from({ length: rows }, (_, i) => {
        const y = y0 + ((y1 - y0) * (i + 1)) / (rows + 1);
        const hot = Math.floor(i / 3) < (v.stages ?? 0);
        return (
          <g key={i}>
            {hot && <Line3 a={[x1, y, 0.1]} b={[x1, y, 0.9]} stroke="#fb923c" strokeWidth={8} opacity={0.35} />}
            <Line3 a={[x1, y, 0.1]} b={[x1, y, 0.9]} stroke={hot ? '#ef4444' : '#9ca3af'} strokeWidth={3} strokeLinecap="round" className="mn-transition" />
          </g>
        );
      })}
      <Box x0={x0 - 4} x1={x1 + 4} y0={y0 + 4} y1={y0 + 30} z0={-0.14} z1={0} front="url(#hmiHazard)" top="#fde047" side="#ca8a04" stroke="#713f12" />
    </g>
  );
};

const Fan: Art = ({ w, h, v, state }) => {
  const x0 = w * 0.1, x1 = w * 0.76, y0 = h * 0.14, y1 = h - 12, z0 = 0.12, z1 = 0.88;
  const [cx, cy] = P((x0 + x1) / 2, (y0 + y1) / 2, z0);
  const r = Math.min(x1 - x0, y1 - y0) * 0.36;
  const spin = useSpin<SVGGElement>(state === 'running' ? 60 + (v.speed ?? 50) * 7 : 0);
  return (
    <g>
      <Box x0={x0 + 4} x1={x1 - 4} y0={y1} y1={h - 4} z0={0.2} z1={0.8} front="#475569" top="#64748b" side="#334155" stroke="#1e293b" />
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={z0} z1={z1} front="#2563eb" top="#3b82f6" side="#1d4ed8" stroke="#1e3a8a" />
      {Array.from({ length: 10 }, (_, i) => {
        const a = (i / 10) * Math.PI * 2;
        return <circle key={i} cx={cx + Math.cos(a) * (r + 7)} cy={cy + Math.sin(a) * (r + 7)} r={1.6} fill="#93c5fd" />;
      })}
      <circle cx={cx} cy={cy} r={r} fill="#0f172a" stroke="#bfdbfe" strokeWidth={3} />
      <g transform={`translate(${cx} ${cy})`}>
        <g ref={spin}>
          {Array.from({ length: 8 }, (_, i) => (
            <path key={i} d={`M0 ${-r * 0.82} Q${r * 0.3} ${-r * 0.4} ${r * 0.08} ${-r * 0.12} L0 0 Z`} fill="#e5e7eb" transform={`rotate(${i * 45})`} />
          ))}
          <circle r={r * 0.18} fill="#94a3b8" stroke="#475569" />
        </g>
      </g>
      <Box x0={x1} x1={Math.min(w - 4, x1 + 18)} y0={(y0 + y1) / 2 - 12} y1={(y0 + y1) / 2 + 12} z0={0.35} z1={0.65} front="#f97316" top="#fb923c" side="#ea580c" stroke="#9a3412" />
    </g>
  );
};

const Humidifier: Art = ({ w, h, state }) => {
  const on = state === 'running';
  const x = w / 2;
  return (
    <g>
      <Box x0={x - 9} x1={x + 9} y0={h - 22} y1={h - 8} z0={0.1} z1={0.9} front="#cbd5e1" top="#e2e8f0" side="#94a3b8" />
      {[0.25, 0.5, 0.75].map((z) => (
        <g key={z}>
          <Line3 a={[x, h - 22, z]} b={[x, 22, z]} stroke="#64748b" strokeWidth={4} strokeLinecap="round" />
          {on &&
            [0.25, 0.45, 0.65].map((k) => {
              const [mx, my] = P(x + 10, 22 + (h - 44) * k, z);
              return <ellipse key={k} cx={mx} cy={my} rx={9} ry={4} fill="#e0f2fe" stroke="#7dd3fc" opacity={0.85} className="hmi-mist" />;
            })}
        </g>
      ))}
    </g>
  );
};

const Damper: Art = ({ w, h, v, p, state }) => {
  const x = w / 2;
  const motor = p.drive === 'motor';
  const open = motor ? (v.pos ?? v.cmd) : (v.open ?? 60);
  const phi = clamp01(open) * (Math.PI / 2);
  const bw = (h - 40) / 9;
  return (
    <g>
      <XPlane x={x} y0={14} y1={h - 8} z0={0.06} z1={0.94} fill="none" stroke="#64748b" strokeWidth={3} />
      {Array.from({ length: 4 }, (_, i) => {
        const y = 32 + i * ((h - 58) / 3);
        const dx = Math.sin(phi) * bw, dy = Math.cos(phi) * bw;
        return (
          <polygon
            key={i}
            points={pts([[x - dx, y - dy, 0.08], [x + dx, y + dy, 0.08], [x + dx, y + dy, 0.92], [x - dx, y - dy, 0.92]])}
            fill="#cbd5e1"
            stroke="#64748b"
            className="mn-transition"
          />
        );
      })}
      <Line3 a={[x, 32, 0]} b={[x, 32, -0.1]} stroke="#475569" strokeWidth={3} />
      {motor ? <Actuator x={x} y={32} alarm={state === 'alarm'} supply={p.supply} /> : <Lever x={x} y={32} phi={phi} />}
    </g>
  );
};

/** Electric actuator on the damper shaft (orange body, terminal cover, two cables going up). */
function Actuator({ x, y, alarm, supply }: { x: number; y: number; alarm: boolean; supply?: string }) {
  const [tx, ty] = P(x, y - 12, -0.22);
  return (
    <g>
      <Box x0={x - 13} x1={x + 13} y0={y - 12} y1={y + 12} z0={-0.22} z1={-0.08} front={alarm ? '#ef4444' : '#f97316'} top="#fdba74" side="#ea580c" stroke="#9a3412" />
      <rect x={tx + 3} y={ty + 4} width={20} height={7} rx={1.5} fill="#1f2937" />
      <text x={tx + 13} y={ty + 10} textAnchor="middle" className="hmi-plate">
        {(supply ?? '24V').split(' ')[0]}
      </text>
      <path d={`M${tx + 8} ${ty} q0 -14 -10 -20`} stroke="#334155" strokeWidth={2} fill="none" />
      <path d={`M${tx + 18} ${ty} q0 -16 -6 -24`} stroke="#64748b" strokeWidth={2} fill="none" />
    </g>
  );
}

/** Hand lever with a locking quadrant. */
function Lever({ x, y, phi }: { x: number; y: number; phi: number }) {
  const [cx, cy] = P(x, y, -0.1);
  const a = (phi * 180) / Math.PI;
  return (
    <g transform={`translate(${cx} ${cy})`}>
      <path d="M0 0 L0 -22 A22 22 0 0 1 22 0 Z" fill="#cbd5e1" stroke="#64748b" />
      <g transform={`rotate(${a})`} className="mn-transition">
        <line x1={0} y1={0} x2={0} y2={-24} stroke="#dc2626" strokeWidth={4} strokeLinecap="round" />
      </g>
      <circle r={3.5} fill="#475569" />
    </g>
  );
}

// ---------- equipment and rooms (shown on their own) ----------

const Vrf: Art = ({ w, h, state }) => {
  const spin = useSpin<SVGGElement>(state === 'running' ? 200 : 0);
  const r = Math.min(w, h) * 0.26;
  return (
    <g>
      <rect width={w} height={h} rx={4} fill="url(#hmiMetal)" stroke="#64748b" />
      <g transform={`translate(${w / 2} ${h * 0.44})`}>
        <circle r={r} fill="#334155" stroke="#64748b" />
        <g ref={spin}>
          {[0, 90, 180, 270].map((a) => (
            <path key={a} d={`M0 ${-r * 0.85} Q${r * 0.3} ${-r * 0.3} 0 0 Q${-r * 0.3} ${-r * 0.3} 0 ${-r * 0.85}`} fill="#e2e8f0" transform={`rotate(${a})`} />
          ))}
        </g>
      </g>
      <circle cx={11} cy={11} r={5} fill={state === 'alarm' ? '#ef4444' : state === 'running' ? '#22c55e' : '#94a3b8'} />
      <text x={w / 2} y={h - 10} textAnchor="middle" fontSize={14} fontWeight={700} style={{ fill: '#c2410c' }}>
        VRF
      </text>
    </g>
  );
};

const Ffu: Art = ({ w, h, state }) => {
  const spin = useSpin<SVGGElement>(state === 'running' ? 500 : 0);
  return (
    <g>
      <rect x={4} y={h / 2 - 14} width={w - 8} height={28} rx={3} fill="#f1f5f9" stroke={state === 'alarm' ? '#ef4444' : '#16a34a'} strokeWidth={2} />
      <g transform={`translate(${w / 2} ${h / 2})`}>
        <circle r={10} fill="#334155" stroke="#16a34a" />
        <g ref={spin}>
          <path d="M0 -9 L2 0 L0 9 L-2 0 z M-9 0 L0 2 L9 0 L0 -2 z" fill="#4ade80" />
        </g>
      </g>
      {[0.25, 0.5, 0.75].map((k) => (
        <path key={k} d={`M${w * k} ${h / 2 + 18} v14 m-4 -5 l4 5 l4 -5`} stroke="#3b82f6" strokeWidth={2} fill="none" opacity={state === 'running' ? 1 : 0.25} />
      ))}
    </g>
  );
};

const AcUnit: Art = ({ w, h, state }) => {
  const spin = useSpin<SVGGElement>(state === 'running' ? 250 : 0);
  return (
    <g>
      <rect y={h / 2 - 30} width={w} height={60} rx={4} fill="#fafaf5" stroke={state === 'alarm' ? '#ef4444' : '#a3a3a3'} strokeWidth={2} />
      <g transform={`translate(30 ${h / 2})`}>
        <circle r={18} fill="#e5e7eb" stroke="#a3a3a3" />
        <g ref={spin}>
          <path d="M0 -15 Q6 -5 0 0 Q-6 -5 0 -15 M15 0 Q5 6 0 0 Q5 -6 15 0 M0 15 Q-6 5 0 0 Q6 5 0 15 M-15 0 Q-5 -6 0 0 Q-5 6 -15 0" fill="#64748b" />
        </g>
      </g>
      <text x={w - 30} y={h / 2 + 4} className="hmi-name" textAnchor="middle">
        AC
      </text>
    </g>
  );
};

const RoomTile: Art = ({ w, h, v, state }) => (
  <g>
    <rect x={2} y={2} width={w - 4} height={h - 4} rx={8} fill="#f0fdf4" stroke={state === 'alarm' ? '#ef4444' : '#16a34a'} strokeWidth={2} />
    <text x={w / 2} y={h / 2 + 8} className="hmi-val" fontSize={24} textAnchor="middle">
      {formatSlot(v.t, artSlots.roomTile[0])}
    </text>
  </g>
);

export const artLibrary: Record<string, Art> = {
  inlet: Inlet,
  outlet: Outlet,
  filter: Filter,
  coil: Coil,
  heater: Heater,
  fan: Fan,
  humidifier: Humidifier,
  damper: Damper,
  vrf: Vrf,
  ffu: Ffu,
  acUnit: AcUnit,
  roomTile: RoomTile,
};

/** Gradients and patterns shared by all HMI pictures. */
export function HmiDefs() {
  return (
    <defs>
      <linearGradient id="hmiMetal" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stopColor="#f8fafc" />
        <stop offset="1" stopColor="#cbd5e1" />
      </linearGradient>
      <linearGradient id="hmiDuct" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stopColor="#f1f5f9" />
        <stop offset="0.5" stopColor="#cbd5e1" />
        <stop offset="1" stopColor="#e2e8f0" />
      </linearGradient>
      <pattern id="hmiHazard" width="10" height="10" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
        <rect width="10" height="10" fill="#facc15" />
        <rect width="5" height="10" fill="#1f2937" />
      </pattern>
    </defs>
  );
}
