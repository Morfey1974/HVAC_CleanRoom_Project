import type { ReactElement, SVGProps } from 'react';
import { useSpin } from '../mnemo/animation';
import type { HmiGroup, HmiParams } from '../api/client';

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
  recovery: [{ key: 'on' }, { key: 'eff', unit: '%', decimals: 0, show: true }, { key: 'fault' }],
  controlPanel: [{ key: 'on' }, { key: 'fault' }],
  ductStraight: [{ key: 'flow', unit: '%', decimals: 0, show: true }],
  ductElbow: [{ key: 'flow', unit: '%', decimals: 0, show: true }],
  ductTee: [{ key: 'flow', unit: '%', decimals: 0, show: true }],
  ductGrille: [{ key: 'open', unit: '%', decimals: 0, show: true }],
  ductSilencer: [],
  pipeStraight: [{ key: 't', unit: '°C', show: true }],
  pipeSupply: [{ key: 't', unit: '°C', show: true }],
  pipeReturn: [{ key: 't', unit: '°C', show: true }],
  pipeH: [{ key: 't', unit: '°C', show: true }],
  pipeV: [{ key: 't', unit: '°C', show: true }],
  pipeElbow: [{ key: 't', unit: '°C', show: true }],
  pipe3way: [{ key: 'open', unit: '%', decimals: 0, show: true }],
  pipeValve: [{ key: 'open', unit: '%', decimals: 0, show: true }],
  pipeCondensate: [{ key: 'on' }],
  sensorT: [{ key: 't', unit: '°C', show: true }],
  sensorDp: [{ key: 'dp', unit: 'Pa', decimals: 0, show: true }],
  sensorP: [{ key: 'p', unit: 'Pa', decimals: 0, show: true }],
  sensorAq: [{ key: 'on' }, { key: 'fault' }],
  sensorSmoke: [{ key: 'fault' }],
  actuator: [{ key: 'pos', unit: '%', decimals: 0, show: true }, { key: 'fault' }],
  vfd: [{ key: 'run' }, { key: 'speed', unit: '%', decimals: 0, show: true }, { key: 'fault' }],
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
export const sectionArts = ['inlet', 'filter', 'coil', 'heater', 'fan', 'outlet', 'humidifier', 'damper', 'recovery', 'controlPanel'];
export const artsByGroup: Record<HmiGroup, string[]> = {
  section: sectionArts,
  duct: ['ductStraight', 'ductElbow', 'ductTee', 'ductGrille', 'ductSilencer'],
  pipe: ['pipeH', 'pipeElbow', 'pipeValve'],
  instrument: ['sensorT', 'sensorDp', 'sensorP', 'sensorAq', 'sensorSmoke', 'actuator', 'vfd'],
  equipment: ['vrf', 'ffu', 'acUnit'],
  room: ['roomTile'],
};
export const artsForGroup = (g: HmiGroup) => artsByGroup[g] ?? arts;

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
  return own || (air > 0 && ['inlet', 'outlet', 'filter', 'recovery', 'ductStraight', 'ductElbow', 'ductTee', 'ductGrille'].includes(art))
    ? 'running'
    : 'stopped';
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
    case 'recovery': return { on: run, eff: run * 72, fault: bad };
    case 'controlPanel': return { on: run, fault: bad };
    case 'ductStraight':
    case 'ductElbow':
    case 'ductTee':
    case 'ductGrille':
      return { flow: run * 80, open: run * 80 };
    case 'pipeStraight':
    case 'pipeSupply':
    case 'pipeReturn':
    case 'pipeH':
    case 'pipeV':
    case 'pipeElbow':
      return { t: art === 'pipeReturn' ? 12 : 7 };
    case 'pipe3way':
    case 'pipeValve':
      return { open: run * 80 };
    case 'pipeCondensate':
      return { on: run };
    case 'sensorT':
      return { t: 21.4 };
    case 'sensorDp':
      return { dp: run * 120 };
    case 'sensorP':
      return { p: run * 160 };
    case 'sensorAq':
      return { on: run, fault: bad };
    case 'sensorSmoke':
      return { fault: bad };
    case 'actuator':
      return { pos: run * 70, fault: bad };
    case 'vfd':
      return { run, speed: run * 80, fault: bad };
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

const ribbon = {
  cool: { top: '#7dd3fc', side: '#0284c7' },
  warm: { top: '#fdba74', side: '#c2410c' },
};

/** Wide flat air ribbon, like cutaway drawings — not a sharp triangle. */
export function AirArrow({
  x0,
  x1,
  y,
  color,
  on,
  tone,
}: {
  x0: number;
  x1: number;
  y: number;
  color?: string;
  on: boolean;
  tone?: 'cool' | 'warm';
}) {
  const pal = tone ? ribbon[tone] : { top: color ?? '#7dd3fc', side: '#0284c7' };
  const hd = Math.min(14, Math.max(10, (x1 - x0) * 0.22));
  const z0 = 0.3, z1 = 0.58, zm = 0.44;
  const t = 7;
  return (
    <g className={on ? 'hmi-arrow hmi-arrow--on' : 'hmi-arrow'} opacity={on ? 0.95 : 0.22}>
      <polygon
        points={pts([
          [x0, y + t, z0],
          [x1 - hd, y + t, z0],
          [x1, y + t, zm],
          [x1 - hd, y + t, z1],
          [x0, y + t, z1],
        ])}
        fill={pal.side}
        stroke="rgba(0,0,0,0.12)"
      />
      <polygon
        points={pts([
          [x0, y, z0],
          [x1 - hd, y, z0],
          [x1, y, zm],
          [x1 - hd, y, z1],
          [x0, y, z1],
        ])}
        fill={pal.top}
        stroke="rgba(0,0,0,0.1)"
      />
      <polygon
        points={pts([
          [x0, y, z0],
          [x1 - hd, y, z0],
          [x1, y, zm],
          [x1, y + t, zm],
          [x1 - hd, y + t, z0],
          [x0, y + t, z0],
        ])}
        fill={pal.side}
        opacity={0.85}
        stroke="rgba(0,0,0,0.08)"
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

const Inlet: Art = ({ w, h }) => (
  <g>
    <Box x0={4} x1={w * 0.42} y0={18} y1={h * 0.48} z0={0.12} z1={0.88} front="#c5cdd6" top="#e8edf2" side="#8b949e" stroke="#64748b" />
    <Box x0={4} x1={w * 0.42} y0={h * 0.54} y1={h - 16} z0={0.12} z1={0.88} front="#c5cdd6" top="#e8edf2" side="#8b949e" stroke="#64748b" />
    {[0, 1].map((row) => {
      const y0 = row === 0 ? 26 : h * 0.6;
      const y1 = row === 0 ? h * 0.42 : h - 24;
      return Array.from({ length: 5 }, (_, i) => {
        const y = y0 + ((y1 - y0) * i) / 4;
        return <Line3 key={`${row}-${i}`} a={[8, y, 0.16]} b={[w * 0.4, y + 4, 0.16]} stroke="#64748b" strokeWidth={2.2} />;
      });
    })}
    <Actuator x={w * 0.22} y={32} alarm={false} supply="24V" />
    <Actuator x={w * 0.22} y={h * 0.62} alarm={false} supply="24V" />
  </g>
);

const Outlet: Art = ({ w, h }) => (
  <g>
    <Box x0={w * 0.5} x1={w - 4} y0={h * 0.28} y1={h * 0.72} z0={0.2} z1={0.8} front="url(#hmiDuct)" top="#e8edf2" side="#94a3b8" stroke="#64748b" />
    {Array.from({ length: 5 }, (_, i) => {
      const y = h * 0.34 + i * ((h * 0.32) / 4);
      return <Line3 key={i} a={[w * 0.54, y, 0.22]} b={[w - 8, y + 3, 0.22]} stroke="#64748b" strokeWidth={2} />;
    })}
  </g>
);

function filterKind(label?: string): 'pre' | 'fine' | 'hepa' {
  const s = (label ?? '').toLowerCase();
  const n = parseInt(s, 10);
  if (s.includes('hepa') || s.includes('95') || s.includes('h13') || s.includes('h14') || n >= 80) return 'hepa';
  if (s.includes('fine') || s.includes('30') || s.includes('f7') || s.includes('f8') || s.includes('f9') || (n >= 25 && n < 80)) return 'fine';
  return 'pre';
}

const Filter: Art = ({ w, h, v, p, state }) => {
  const kind = filterKind(p.label);
  const x0 = w * 0.18, x1 = w * 0.82, y0 = 12, y1 = h - 10;
  const dirt = clamp01(v.dirt);
  const alarm = state === 'alarm' ? '#ef4444' : undefined;
  const dust = dirt > 0.08 ? `rgba(120,53,15,${0.08 + dirt * 0.3})` : undefined;
  return (
    <g>
      {kind === 'pre' && <PreFilter x0={x0} x1={x1} y0={y0} y1={y1} alarm={alarm} dust={dust} />}
      {kind === 'fine' && <BagFilter x0={x0} x1={x1} y0={y0} y1={y1} alarm={alarm} dust={dust} />}
      {kind === 'hepa' && <HepaFilter x0={x0} x1={x1} y0={y0} y1={y1} alarm={alarm} dust={dust} />}
    </g>
  );
};

type FilterBody = { x0: number; x1: number; y0: number; y1: number; alarm?: string; dust?: string };

/** Panel pre-filter: aluminium frame, white media, diamond mesh. */
function PreFilter({ x0, x1, y0, y1, alarm, dust }: FilterBody) {
  const nx = 3, ny = 4;
  const stroke = alarm ?? '#8b949e';
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={0.18} z1={0.82} front="#f3f1ea" top="#e8e6de" side="#c5c0b4" stroke={stroke} />
      {Array.from({ length: ny }, (_, r) =>
        Array.from({ length: nx }, (_, c) => {
          const xa = x0 + 5 + (c / nx) * (x1 - x0 - 10);
          const xb = x0 + 5 + ((c + 1) / nx) * (x1 - x0 - 10);
          const ya = y0 + 8 + (r / ny) * (y1 - y0 - 16);
          const yb = y0 + 8 + ((r + 1) / ny) * (y1 - y0 - 16);
          return (
            <g key={`${r}-${c}`} stroke="#9aa3ad" strokeWidth={1.1}>
              <Line3 a={[xa, ya, 0.2]} b={[xb, yb, 0.2]} />
              <Line3 a={[xb, ya, 0.2]} b={[xa, yb, 0.2]} />
            </g>
          );
        }),
      )}
      {dust && <XPlane x={x1} y0={y0 + 8} y1={y1 - 8} z0={0.2} z1={0.8} fill={dust} className="mn-transition" />}
      <Box x0={x0} x1={x1} y0={y0} y1={y0 + 7} z0={0.16} z1={0.84} front="#c5cdd6" top="#e8edf2" side="#94a3b8" stroke={stroke} />
      <Box x0={x0} x1={x1} y0={y1 - 7} y1={y1} z0={0.16} z1={0.84} front="#c5cdd6" top="#e8edf2" side="#94a3b8" stroke={stroke} />
    </g>
  );
}

/** Fine bag filter: metal header and hanging white pockets. */
function BagFilter({ x0, x1, y0, y1, alarm, dust }: FilterBody) {
  const bags = 5;
  const stroke = alarm ?? '#8b949e';
  return (
    <g>
      {Array.from({ length: bags }, (_, i) => {
        const za = 0.16 + (i / bags) * 0.7;
        const zb = za + (0.7 / bags) * 0.78;
        const fill = i % 2 ? '#f4efe4' : '#e7e0d2';
        return (
          <g key={i}>
            <polygon
              points={pts([
                [x0 + 8, y0 + 10, za],
                [x1 - 6, y0 + 14, za],
                [x1 - 2, (y0 + y1) / 2, za],
                [x1 - 6, y1 - 14, za],
                [x0 + 8, y1 - 10, za],
              ])}
              fill={fill}
              stroke="#b8b09e"
              strokeWidth={0.9}
            />
            <polygon
              points={pts([
                [x1 - 6, y0 + 14, za],
                [x1 - 6, y0 + 14, zb],
                [x1 - 2, (y0 + y1) / 2, zb],
                [x1 - 2, (y0 + y1) / 2, za],
              ])}
              fill="#d4ccbc"
              stroke="#b8b09e"
              strokeWidth={0.6}
            />
          </g>
        );
      })}
      {dust && <XPlane x={x1 - 2} y0={y0 + 12} y1={y1 - 12} z0={0.16} z1={0.86} fill={dust} className="mn-transition" />}
      <Box x0={x0} x1={x0 + 10} y0={y0} y1={y1} z0={0.12} z1={0.88} front="#c5cdd6" top="#e8edf2" side="#94a3b8" stroke={stroke} />
    </g>
  );
}

/** HEPA cassette: same aluminium casing as the other filters, denser pleats. */
function HepaFilter({ x0, x1, y0, y1, alarm, dust }: FilterBody) {
  const stroke = alarm ?? '#8b949e';
  const pleats = Math.max(12, Math.round((x1 - x0) / 3));
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={0.1} z1={0.9} front="#f3f1ea" top="#e8e6de" side="#c5c0b4" stroke={stroke} />
      {Array.from({ length: pleats }, (_, i) => {
        const step = (x1 - x0 - 12) / pleats;
        const xa = x0 + 6 + i * step;
        return (
          <polygon
            key={i}
            points={pts([
              [xa, y0 + 8, 0.14],
              [xa + step * 0.85, y0 + 8, 0.14],
              [xa + step * 0.85, y1 - 8, 0.14],
              [xa, y1 - 8, 0.14],
            ])}
            fill={i % 2 ? '#f8fafc' : '#e2e8f0'}
            stroke="#cbd5e1"
            strokeWidth={0.4}
          />
        );
      })}
      {Array.from({ length: 5 }, (_, r) => {
        const y = y0 + 8 + ((r + 1) / 6) * (y1 - y0 - 16);
        return <Line3 key={r} a={[x0 + 6, y, 0.15]} b={[x1 - 6, y, 0.15]} stroke="#94a3b8" strokeWidth={0.7} />;
      })}
      {dust && <XPlane x={x1 - 4} y0={y0 + 8} y1={y1 - 8} z0={0.14} z1={0.86} fill={dust} className="mn-transition" />}
      <Box x0={x0} x1={x1} y0={y0} y1={y0 + 8} z0={0.08} z1={0.92} front="#c5cdd6" top="#e8edf2" side="#94a3b8" stroke={stroke} />
      <Box x0={x0} x1={x1} y0={y1 - 8} y1={y1} z0={0.08} z1={0.92} front="#c5cdd6" top="#e8edf2" side="#94a3b8" stroke={stroke} />
    </g>
  );
}

function CoilPack({ x0, x1, y0, y1, cool, on, cap }: { x0: number; x1: number; y0: number; y1: number; cool: boolean; on: boolean; cap: number }) {
  const tube = cool ? (on ? '#2563eb' : '#60a5fa') : on ? '#dc2626' : '#f87171';
  const body = cool ? '#dbeafe' : '#fee2e2';
  const fin = cool ? '#93c5fd' : '#fecaca';
  const glow = on ? (cool ? `rgba(37,99,235,${0.1 + cap * 0.28})` : `rgba(220,38,38,${0.1 + cap * 0.28})`) : 'transparent';
  const pairs = 8;
  const pitch = (y1 - y0 - 8) / pairs;
  const bulge = 8;
  return (
    <g>
      <Box x0={x0} x1={x1} y0={y0} y1={y1} z0={0.12} z1={0.88} front={body} top="#f8fafc" side="#cbd5e1" stroke="#7b8aa0" />
      {Array.from({ length: 16 }, (_, i) => {
        const z = 0.16 + (0.7 * i) / 15;
        return <Line3 key={`f${i}`} a={[x1, y0 + 4, z]} b={[x1, y1 - 4, z]} stroke={fin} strokeWidth={1.4} />;
      })}
      {Array.from({ length: pairs }, (_, i) => {
        const yA = y0 + 6 + i * pitch;
        const yB = yA + pitch * 0.7;
        const [a, b, c, d] = [P(x0, yA, 0.14), P(x0 - bulge, yA, 0.14), P(x0 - bulge, yB, 0.14), P(x0, yB, 0.14)];
        return (
          <path
            key={i}
            d={`M${a[0]} ${a[1]} L${b[0]} ${b[1]} L${c[0]} ${c[1]} L${d[0]} ${d[1]}`}
            fill="none"
            stroke={tube}
            strokeWidth={2.2}
            strokeLinecap="round"
            strokeLinejoin="round"
          />
        );
      })}
      <XPlane x={x1} y0={y0} y1={y1} z0={0.12} z1={0.88} fill={glow} className="mn-transition" />
    </g>
  );
}

const Coil: Art = ({ w, h, v, p }) => {
  const cool = (v.mode ?? 1) !== 2;
  const on = pos(v.on);
  const cap = clamp01(v.cap);
  const x0 = w * 0.28, x1 = w * 0.78, y0 = 16, y1 = h - 18;
  return (
    <g>
      <CoilPack x0={x0} x1={x1} y0={y0} y1={y1} cool={cool} on={on} cap={cap} />
      <Box x0={x0 - 4} x1={x1 + 4} y0={y1 - 2} y1={h - 8} z0={0.15} z1={0.85} front="#94a3b8" top="#cbd5e1" side="#64748b" stroke="#475569" />
      <Label at={[x1, h / 2, 0.5]} text={p.label ?? (cool ? 'DX' : 'HEAT')} />
    </g>
  );
};

const Heater: Art = ({ w, h, v, p }) => {
  const stages = Math.max(1, Math.min(6, p.stages ?? 3));
  const onN = v.stages ?? 0;
  const x0 = w * 0.26, x1 = w * 0.78, y0 = 16, y1 = h - 14;
  return (
    <g>
      <CoilPack x0={x0} x1={x1} y0={y0 + 18} y1={y1} cool={false} on={onN > 0} cap={onN / stages} />
      <Box x0={x0 - 6} x1={x1 + 6} y0={y0} y1={y0 + 22} z0={-0.12} z1={0.05} front="url(#hmiHazard)" top="#fde047" side="#ca8a04" stroke="#713f12" />
      {Array.from({ length: stages }, (_, i) => {
        const hot = i < onN;
        const y = y0 + 28 + ((y1 - y0 - 36) * (i + 0.5)) / stages;
        return <Line3 key={i} a={[x1, y, 0.16]} b={[x1, y, 0.84]} stroke={hot ? '#ef4444' : '#9ca3af'} strokeWidth={hot ? 5 : 2.5} className="mn-transition" />;
      })}
    </g>
  );
};

const Fan: Art = ({ w, h, v, state }) => {
  const cx = w * 0.4;
  const cy = h * 0.58;
  const rx = Math.min(w * 0.38, h * 0.38);
  const ry = rx * 0.88;
  const spin = useSpin<SVGGElement>(state === 'running' ? 70 + (v.speed ?? 50) * 6 : 0);
  const hornY0 = cy - ry * 0.95;
  const hornY1 = hornY0 + ry * 0.78;
  return (
    <g>
      <ellipse cx={cx + DX * 0.3} cy={cy - DY * 0.3} rx={rx} ry={ry} fill="#b8c0c8" stroke="#5b6a7a" strokeWidth={1.6} />
      <Box x0={cx + rx * 0.1} x1={w - 4} y0={hornY0} y1={hornY1} z0={0.08} z1={0.55} front="#cfd6de" top="#e8edf2" side="#9aa3ad" stroke="#5b6a7a" />
      <ellipse cx={cx} cy={cy} rx={rx} ry={ry} fill="#d4dbe3" stroke="#475569" strokeWidth={2.4} />
      <ellipse cx={cx} cy={cy} rx={rx * 0.58} ry={ry * 0.58} fill="#1e2937" stroke="#94a3b8" strokeWidth={2} />
      <g transform={`translate(${cx} ${cy})`}>
        <g ref={spin} className="hmi-fan-impeller">
          {Array.from({ length: 12 }, (_, i) => (
            <path
              key={i}
              d={`M0 ${-rx * 0.12} Q${rx * 0.34} ${-rx * 0.14} ${rx * 0.58} ${-rx * 0.02} Q${rx * 0.36} ${rx * 0.12} 0 ${rx * 0.04} Z`}
              fill="#d8dde4"
              stroke="#9aa3ad"
              strokeWidth={0.5}
              transform={`rotate(${i * 30})`}
            />
          ))}
          <circle r={rx * 0.14} fill="#8b949e" stroke="#4b5563" />
        </g>
      </g>
      <ellipse cx={cx} cy={cy} rx={rx * 0.28} ry={ry * 0.26} fill="none" stroke="#e2e8f0" strokeWidth={4} />
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
      <Box x0={x - 8} x1={x + 8} y0={14} y1={h - 8} z0={0.08} z1={0.92} front="#c5cdd6" top="#e8edf2" side="#8b949e" stroke="#64748b" />
      {Array.from({ length: 5 }, (_, i) => {
        const y = 28 + i * ((h - 52) / 4);
        const dx = Math.sin(phi) * bw, dy = Math.cos(phi) * bw;
        return (
          <polygon
            key={i}
            points={pts([[x - dx, y - dy, 0.1], [x + dx, y + dy, 0.1], [x + dx, y + dy, 0.9], [x - dx, y - dy, 0.9]])}
            fill="#d1d5db"
            stroke="#64748b"
            className="mn-transition"
          />
        );
      })}
      <Line3 a={[x, 28, 0]} b={[x, 28, -0.12]} stroke="#475569" strokeWidth={3} />
      {motor ? <Actuator x={x} y={28} alarm={state === 'alarm'} supply={p.supply} /> : <Lever x={x} y={28} phi={phi} />}
    </g>
  );
};

const Recovery: Art = ({ w, h, state }) => {
  const cx = w / 2, cy = h * 0.5, r = Math.min(w, h) * 0.36;
  const on = state === 'running';
  const spin = useSpin<SVGGElement>(on ? 36 : 0);
  return (
    <g>
      <circle cx={cx} cy={cy} r={r} fill="#c5cdd6" stroke="#64748b" strokeWidth={3} />
      <g transform={`translate(${cx} ${cy})`}>
        <g ref={spin}>
          <path d={`M0 0 L0 ${-r} A${r} ${r} 0 0 1 0 ${r} Z`} fill="#fb923c" opacity={0.88} />
          <path d={`M0 0 L0 ${r} A${r} ${r} 0 0 1 0 ${-r} Z`} fill="#38bdf8" opacity={0.88} />
          {Array.from({ length: 12 }, (_, i) => (
            <line key={i} x1={0} y1={0} x2={0} y2={-r * 0.92} stroke="#94a3b8" strokeWidth={0.8} transform={`rotate(${i * 30})`} />
          ))}
        </g>
      </g>
      <circle cx={cx} cy={cy} r={r * 0.18} fill="#9ca3af" stroke="#64748b" />
    </g>
  );
};

const ControlPanel: Art = ({ w, h, state }) => (
  <g>
    <Box x0={10} x1={w - 10} y0={12} y1={h - 10} z0={0.05} z1={0.9} front="#d1d5db" top="#e5e7eb" side="#9ca3af" stroke="#4b5563" />
    <rect x={w * 0.28} y={h * 0.22} width={w * 0.44} height={h * 0.28} rx={3} fill="#0f172a" stroke="#334155" />
    <rect x={w * 0.32} y={h * 0.26} width={w * 0.36} height={h * 0.18} rx={2} fill={state === 'alarm' ? '#7f1d1d' : '#164e63'} />
    <circle cx={w * 0.3} cy={h * 0.68} r={6} fill={state === 'alarm' ? '#ef4444' : state === 'running' ? '#22c55e' : '#94a3b8'} />
    <circle cx={w * 0.48} cy={h * 0.68} r={6} fill="#eab308" />
    <rect x={w * 0.62} y={h * 0.62} width={18} height={12} rx={2} fill="#1e3a8a" />
  </g>
);

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

function DuctBody({ w, h, y0 = h * 0.28, y1 = h * 0.72 }: { w: number; h: number; y0?: number; y1?: number }) {
  return <Box x0={2} x1={w - 2} y0={y0} y1={y1} z0={0.18} z1={0.82} front="url(#hmiDuct)" top="#e8edf2" side="#94a3b8" stroke="#64748b" />;
}

const DuctStraight: Art = ({ w, h, air }) => (
  <g>
    <DuctBody w={w} h={h} />
    <AirArrow x0={8} x1={w - 8} y={h * 0.5} color="#38bdf8" on={air > 0} />
  </g>
);

const DuctElbow: Art = ({ w, h, air }) => (
  <g>
    <Box x0={2} x1={w * 0.55} y0={h * 0.28} y1={h * 0.72} z0={0.18} z1={0.82} front="url(#hmiDuct)" top="#e8edf2" side="#94a3b8" stroke="#64748b" />
    <Box x0={w * 0.38} x1={w - 2} y0={8} y1={h * 0.48} z0={0.18} z1={0.82} front="url(#hmiDuct)" top="#e8edf2" side="#94a3b8" stroke="#64748b" />
    <AirArrow x0={8} x1={w * 0.42} y={h * 0.5} color="#38bdf8" on={air > 0} />
  </g>
);

const DuctTee: Art = ({ w, h, air }) => (
  <g>
    <DuctBody w={w} h={h} />
    <Box x0={w * 0.38} x1={w * 0.62} y0={8} y1={h * 0.32} z0={0.18} z1={0.82} front="url(#hmiDuct)" top="#e8edf2" side="#94a3b8" stroke="#64748b" />
    <AirArrow x0={8} x1={w - 8} y={h * 0.5} color="#38bdf8" on={air > 0} />
  </g>
);

const DuctGrille: Art = ({ w, h, v }) => {
  const open = clamp01(v.open ?? 80);
  return (
    <g>
      <Box x0={8} x1={w - 8} y0={h * 0.18} y1={h * 0.82} z0={0.1} z1={0.9} front="#c5cdd6" top="#e8edf2" side="#8b949e" stroke="#64748b" />
      {Array.from({ length: 6 }, (_, i) => {
        const y = h * 0.24 + i * ((h * 0.52) / 5);
        return <Line3 key={i} a={[14, y, 0.12]} b={[w - 14, y + 5 * (1 - open), 0.12]} stroke="#475569" strokeWidth={2.4} />;
      })}
    </g>
  );
};

const DuctSilencer: Art = ({ w, h }) => (
  <g>
    <DuctBody w={w} h={h} y0={h * 0.22} y1={h * 0.78} />
    {Array.from({ length: 5 }, (_, i) => (
      <rect key={i} x={12 + i * ((w - 28) / 5)} y={h * 0.32} width={8} height={h * 0.36} rx={1} fill="#94a3b8" opacity={0.55} />
    ))}
  </g>
);

type PipePaint = { body: string; hi: string; lo: string; stroke: string; flange: string };
const PAINT: Record<'cold' | 'hot', PipePaint> = {
  cold: { body: '#38bdf8', hi: '#e0f2fe', lo: '#0284c7', stroke: '#0c4a6e', flange: '#7dd3fc' },
  hot: { body: '#fb923c', hi: '#ffedd5', lo: '#c2410c', stroke: '#7c2d12', flange: '#fdba74' },
};
const paintOf = (p: HmiParams): PipePaint => PAINT[p.fluid === 'hot' ? 'hot' : 'cold'];
const R = 8;

function FlangeH({ x, cy, c }: { x: number; cy: number; c: PipePaint }) {
  return <rect x={x - 3} y={cy - R - 3} width={6} height={R * 2 + 6} rx={1} fill={c.flange} stroke={c.stroke} />;
}
function FlangeV({ x, y, c }: { x: number; y: number; c: PipePaint }) {
  return <rect x={x - R - 3} y={y - 3} width={R * 2 + 6} height={6} rx={1} fill={c.flange} stroke={c.stroke} />;
}

function TubeH({ x0, x1, cy, cap = 'both', c }: { x0: number; x1: number; cy: number; cap?: 'both' | 'start' | 'end' | 'none'; c: PipePaint }) {
  const y = cy - R;
  const w = Math.max(8, x1 - x0);
  const a = cap === 'both' || cap === 'start';
  const b = cap === 'both' || cap === 'end';
  return (
    <g>
      <rect x={x0} y={y} width={w} height={R * 2} rx={R} fill={c.body} stroke={c.stroke} />
      <line x1={x0 + 6} y1={cy - 4} x2={x1 - 6} y2={cy - 4} stroke={c.hi} strokeWidth={2} />
      {a && <FlangeH x={x0} cy={cy} c={c} />}
      {b && <FlangeH x={x1} cy={cy} c={c} />}
    </g>
  );
}

function TubeV({ x, y0, y1, cap = 'both', c }: { x: number; y0: number; y1: number; cap?: 'both' | 'start' | 'end' | 'none'; c: PipePaint }) {
  const h = Math.max(8, y1 - y0);
  const a = cap === 'both' || cap === 'start';
  const b = cap === 'both' || cap === 'end';
  return (
    <g>
      <rect x={x - R} y={y0} width={R * 2} height={h} rx={R} fill={c.body} stroke={c.stroke} />
      <line x1={x - 4} y1={y0 + 6} x2={x - 4} y2={y1 - 6} stroke={c.hi} strokeWidth={2} />
      {a && <FlangeV x={x} y={y0} c={c} />}
      {b && <FlangeV x={x} y={y1} c={c} />}
    </g>
  );
}

function Bend({ x, y, c }: { x: number; y: number; c: PipePaint }) {
  return <circle cx={x} cy={y} r={R + 2} fill={c.lo} stroke={c.stroke} />;
}

/** Three-way body; round pad on the stem is where a motor will sit later. */
function ValveBody({ x, y, handle, c }: { x: number; y: number; handle?: number; c: PipePaint }) {
  return (
    <g>
      <rect x={x - 11} y={y - 11} width={22} height={22} rx={3} fill={c.lo} stroke={c.stroke} />
      <line x1={x} y1={y - 11} x2={x} y2={y - 20} stroke={c.stroke} strokeWidth={3} />
      <circle cx={x} cy={y - 26} r={8} fill="#94a3b8" stroke="#334155" />
      <rect x={x - 9} y={y - 30} width={18} height={6} rx={1} fill="#e2e8f0" stroke="#334155" transform={`rotate(${handle ?? 0} ${x} ${y - 26})`} />
      <circle cx={x} cy={y} r={3} fill={c.hi} />
    </g>
  );
}

const PipeH: Art = ({ w, h, p }) => {
  const c = paintOf(p);
  const rot = ((p.rot ?? 0) % 360 + 360) % 360;
  if (rot === 90 || rot === 270) return <TubeV x={w / 2} y0={0} y1={h} c={c} />;
  return <TubeH x0={0} x1={w} cy={h / 2} c={c} />;
};
const PipeV: Art = ({ w, h, p }) => <TubeV x={w / 2} y0={0} y1={h} c={paintOf(p)} />;

const PipeElbow: Art = ({ w, h, p }) => {
  const rot = ((p.rot ?? 0) % 360 + 360) % 360;
  const c = paintOf(p);
  const cx = w / 2, cy = h / 2;
  if (rot === 90)
    return (
      <g>
        <TubeV x={cx} y0={cy} y1={h} cap="end" c={c} />
        <Bend x={cx} y={cy} c={c} />
        <TubeH x0={cx} x1={w} cy={cy} cap="end" c={c} />
      </g>
    );
  if (rot === 180)
    return (
      <g>
        <TubeH x0={cx} x1={w} cy={cy} cap="end" c={c} />
        <Bend x={cx} y={cy} c={c} />
        <TubeV x={cx} y0={0} y1={cy} cap="start" c={c} />
      </g>
    );
  if (rot === 270)
    return (
      <g>
        <TubeV x={cx} y0={0} y1={cy} cap="start" c={c} />
        <Bend x={cx} y={cy} c={c} />
        <TubeH x0={0} x1={cx} cy={cy} cap="start" c={c} />
      </g>
    );
  return (
    <g>
      <TubeH x0={0} x1={cx} cy={cy} cap="start" c={c} />
      <Bend x={cx} y={cy} c={c} />
      <TubeV x={cx} y0={cy} y1={h} cap="end" c={c} />
    </g>
  );
};

const Pipe3way: Art = ({ w, h, v, p }) => {
  const rot = ((p.rot ?? 0) % 360 + 360) % 360;
  const c = paintOf(p);
  const cx = w / 2, cy = h / 2;
  const open = clamp01(v.open ?? 50);
  const arms =
    rot === 90 ? (
      <g>
        <TubeV x={cx} y0={0} y1={h} c={c} />
        <TubeH x0={cx} x1={w} cy={cy} cap="end" c={c} />
      </g>
    ) : rot === 180 ? (
      <g>
        <TubeH x0={0} x1={w} cy={cy} c={c} />
        <TubeV x={cx} y0={0} y1={cy} cap="start" c={c} />
      </g>
    ) : rot === 270 ? (
      <g>
        <TubeV x={cx} y0={0} y1={h} c={c} />
        <TubeH x0={0} x1={cx} cy={cy} cap="start" c={c} />
      </g>
    ) : (
      <g>
        <TubeH x0={0} x1={w} cy={cy} c={c} />
        <TubeV x={cx} y0={cy} y1={h} cap="end" c={c} />
      </g>
    );
  return (
    <g>
      {arms}
      <ValveBody x={cx} y={cy} handle={open * 90 - 45} c={c} />
    </g>
  );
};

const PipeValve: Art = Pipe3way;

const PipeStraight: Art = PipeH;
const PipeSupply: Art = PipeH;
const PipeReturn: Art = PipeH;
const PipeCondensate: Art = PipeH;

const SensorT: Art = ({ w, h, v }) => {
  const cx = w / 2, cy = h * 0.42;
  return (
    <g>
      <rect x={cx - 10} y={cy - 16} width={20} height={22} rx={3} fill="#f8fafc" stroke="#64748b" />
      <circle cx={cx} cy={cy - 5} r={5} fill="#ef4444" />
      <line x1={cx} y1={cy + 6} x2={cx} y2={h * 0.78} stroke="#94a3b8" strokeWidth={3} />
      <text x={cx} y={h - 8} textAnchor="middle" className="hmi-val">
        {formatSlot(v.t, artSlots.sensorT[0])}
      </text>
    </g>
  );
};

const SensorDp: Art = ({ w, h, v }) => {
  const cx = w / 2, cy = h * 0.4;
  return (
    <g>
      <rect x={cx - 16} y={cy - 14} width={32} height={28} rx={4} fill="#e2e8f0" stroke="#64748b" />
      <line x1={cx - 8} y1={cy + 14} x2={cx - 8} y2={h * 0.78} stroke="#64748b" strokeWidth={2} />
      <line x1={cx + 8} y1={cy + 14} x2={cx + 8} y2={h * 0.78} stroke="#64748b" strokeWidth={2} />
      <text x={cx} y={h - 8} textAnchor="middle" className="hmi-val">
        {formatSlot(v.dp, artSlots.sensorDp[0])}
      </text>
    </g>
  );
};

const SensorP: Art = ({ w, h, v }) => {
  const cx = w / 2, cy = h * 0.4;
  return (
    <g>
      <rect x={cx - 14} y={cy - 14} width={28} height={28} rx={4} fill="#e0f2fe" stroke="#0284c7" />
      <line x1={cx} y1={cy + 14} x2={cx} y2={h * 0.78} stroke="#0284c7" strokeWidth={2} />
      <text x={cx} y={h - 8} textAnchor="middle" className="hmi-val">
        {formatSlot(v.p, artSlots.sensorP[0])}
      </text>
    </g>
  );
};

const SensorAq: Art = ({ w, h, state }) => {
  const cx = w / 2, cy = h * 0.48;
  return (
    <g>
      <rect x={cx - 16} y={cy - 14} width={32} height={28} rx={4} fill="#dcfce7" stroke="#16a34a" />
      <circle cx={cx} cy={cy} r={6} fill={state === 'alarm' ? '#ef4444' : '#22c55e'} />
    </g>
  );
};

const SensorSmoke: Art = ({ w, h, state }) => {
  const cx = w / 2, cy = h * 0.5;
  return (
    <g>
      <circle cx={cx} cy={cy} r={22} fill="#e5e7eb" stroke="#64748b" strokeWidth={2} />
      <circle cx={cx} cy={cy} r={10} fill={state === 'alarm' ? '#ef4444' : '#94a3b8'} />
    </g>
  );
};

const ActuatorArt: Art = ({ w, h, v, state }) => (
  <g transform={`translate(${w / 2} ${h * 0.45})`}>
    <Actuator x={0} y={0} alarm={state === 'alarm'} supply="24V" />
    <text y={42} textAnchor="middle" className="hmi-val">
      {formatSlot(v.pos, artSlots.actuator[0])}
    </text>
  </g>
);

const Vfd: Art = ({ w, h, v, state }) => (
  <g>
    <rect x={w * 0.18} y={h * 0.18} width={w * 0.64} height={h * 0.64} rx={4} fill="#d1d5db" stroke="#4b5563" />
    <rect x={w * 0.28} y={h * 0.28} width={w * 0.44} height={h * 0.22} rx={2} fill="#0f172a" />
    <text x={w / 2} y={h * 0.42} textAnchor="middle" className="hmi-plate">
      {state === 'running' ? `${Math.round(v.speed ?? 0)}%` : 'OFF'}
    </text>
    <circle cx={w * 0.32} cy={h * 0.64} r={5} fill={state === 'alarm' ? '#ef4444' : state === 'running' ? '#22c55e' : '#94a3b8'} />
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
  recovery: Recovery,
  controlPanel: ControlPanel,
  ductStraight: DuctStraight,
  ductElbow: DuctElbow,
  ductTee: DuctTee,
  ductGrille: DuctGrille,
  ductSilencer: DuctSilencer,
  pipeStraight: PipeStraight,
  pipeSupply: PipeSupply,
  pipeReturn: PipeReturn,
  pipeH: PipeH,
  pipeV: PipeV,
  pipeElbow: PipeElbow,
  pipe3way: Pipe3way,
  pipeValve: PipeValve,
  pipeCondensate: PipeCondensate,
  sensorT: SensorT,
  sensorDp: SensorDp,
  sensorP: SensorP,
  sensorAq: SensorAq,
  sensorSmoke: SensorSmoke,
  actuator: ActuatorArt,
  vfd: Vfd,
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
        <stop offset="0" stopColor="#e8edf2" />
        <stop offset="0.5" stopColor="#c5cdd6" />
        <stop offset="1" stopColor="#9aa3ad" />
      </linearGradient>
      <linearGradient id="hmiSteel" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stopColor="#e5e7eb" />
        <stop offset="0.45" stopColor="#9ca3af" />
        <stop offset="1" stopColor="#6b7280" />
      </linearGradient>
      <linearGradient id="hmiFlowWarm" x1="0" y1="0" x2="1" y2="0">
        <stop offset="0" stopColor="#ff7a18" stopOpacity="0" />
        <stop offset="0.38" stopColor="#ff8a2a" stopOpacity="0.35" />
        <stop offset="0.72" stopColor="#ff7a18" />
        <stop offset="1" stopColor="#ff5c00" />
      </linearGradient>
      <linearGradient id="hmiFlowCool" x1="0" y1="0" x2="1" y2="0">
        <stop offset="0" stopColor="#4fc3f7" stopOpacity="0" />
        <stop offset="0.38" stopColor="#5ecfff" stopOpacity="0.35" />
        <stop offset="0.72" stopColor="#42c0f5" />
        <stop offset="1" stopColor="#1ea7e6" />
      </linearGradient>
      <pattern id="hmiHazard" width="10" height="10" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
        <rect width="10" height="10" fill="#facc15" />
        <rect width="5" height="10" fill="#1f2937" />
      </pattern>
    </defs>
  );
}
