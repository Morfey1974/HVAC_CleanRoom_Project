import type { ReactElement } from 'react';
import { useDash, useSpin } from './animation';
import type { ElementRenderProps, Loc } from './types';

type Renderer = (props: ElementRenderProps) => ReactElement;

const num = (p: Record<string, unknown>, k: string, d: number) => (typeof p[k] === 'number' ? (p[k] as number) : d);
const str = (p: Record<string, unknown>, k: string, d = '') => (typeof p[k] === 'string' ? (p[k] as string) : d);
const on = (v: number | undefined) => (v ?? 0) > 0;

export function formatValue(v: number | undefined, decimals = 1, unit = '') {
  if (v === undefined || Number.isNaN(v)) return `— —${unit ? ` ${unit}` : ''}`;
  return `${v.toFixed(decimals)}${unit ? ` ${unit}` : ''}`;
}

/** Housing with top and side faces for a 3D look. */
const Box3d: Renderer = ({ p, text }) => {
  const w = num(p, 'w', 200), h = num(p, 'h', 100), d = num(p, 'depth', 28);
  const walls = (p.walls as number[] | undefined) ?? [];
  return (
    <g>
      <polygon points={`0,0 ${d},${-d * 0.8} ${w + d},${-d * 0.8} ${w},0`} fill="url(#mnTop)" stroke="#94a3b8" />
      <polygon points={`${w},0 ${w + d},${-d * 0.8} ${w + d},${h - d * 0.8} ${w},${h}`} fill="url(#mnSide)" />
      <rect width={w} height={h} fill="#e2e8f0" stroke="#94a3b8" />
      {walls.map((x) => (
        <line key={x} x1={x} y1={0} x2={x} y2={h} stroke="#94a3b8" strokeWidth={1.5} />
      ))}
      {p.title !== undefined && (
        <text x={w / 2} y={-d * 0.8 - 6} className="mn-caption" textAnchor="middle">
          {text(p.title as Loc)}
        </text>
      )}
    </g>
  );
};

const Label: Renderer = ({ p, text }) => (
  <text
    className="mn-label"
    fontSize={num(p, 'size', 13)}
    fill={str(p, 'color', 'var(--mn-text)')}
    textAnchor={str(p, 'anchor', 'start') as 'start' | 'middle' | 'end'}
  >
    {text(p.text as Loc)}
  </text>
);

const Value: Renderer = ({ v, p }) => (
  <text className="mn-value" fontSize={num(p, 'size', 13)} textAnchor={str(p, 'anchor', 'middle') as 'middle'}>
    {formatValue(v.value, num(p, 'decimals', 1), str(p, 'unit'))}
  </text>
);

const Filter: Renderer = ({ v, p }) => {
  const w = num(p, 'w', 40), h = num(p, 'h', 120);
  const dirt = Math.max(0, Math.min(100, v.dirt ?? 0)) / 100;
  return (
    <g>
      <rect width={w} height={h} fill="url(#mnFilter)" stroke="#64748b" />
      <rect y={h - h * dirt} width={w} height={h * dirt} fill="#78350f" opacity={0.45} />
      <text x={w / 2} y={h + 16} className="mn-small-dark" textAnchor="middle">
        {str(p, 'label')}
      </text>
    </g>
  );
};

const Coil: Renderer = ({ v, p }) => {
  const w = num(p, 'w', 55), h = num(p, 'h', 120);
  const cap = (v.cap ?? 0) / 100;
  const fill = on(v.on) ? (v.mode === 1 ? `rgba(59,130,246,${0.35 + cap * 0.6})` : `rgba(239,68,68,${0.35 + cap * 0.6})`) : '#94a3b8';
  return (
    <g>
      <rect width={w} height={h} fill={fill} stroke="#475569" />
      {Array.from({ length: 7 }, (_, i) => (
        <line key={i} x1={0} x2={w} y1={15 * (i + 1)} y2={15 * (i + 1)} stroke="#475569" />
      ))}
      <text x={w / 2} y={h + 16} className="mn-small-dark" textAnchor="middle">
        {str(p, 'label', 'DX')}
      </text>
    </g>
  );
};

const Heater: Renderer = ({ v, p }) => {
  const stages = num(p, 'stages', 3), gap = num(p, 'gap', 50);
  const wave = 'q6 -8 12 0 t12 0 t12 0 t12 0 t12 0 t12 0 t12 0 t12 0';
  return (
    <g>
      {Array.from({ length: stages }, (_, i) => (
        <path
          key={i}
          d={`M0 ${i * gap} ${wave}`}
          stroke={i < (v.on ?? 0) ? '#ef4444' : '#64748b'}
          strokeWidth={5}
          fill="none"
          className="mn-transition"
        />
      ))}
      <text x={48} y={(stages - 1) * gap + 27} className="mn-small-dark" textAnchor="middle">
        {str(p, 'label')}
      </text>
    </g>
  );
};

const Fan: Renderer = ({ v, p }) => {
  const r = num(p, 'r', 62);
  const running = on(v.running);
  const spin = useSpin<SVGGElement>(running ? (v.speed ?? 0) * 7 : 0);
  return (
    <g>
      <circle r={r} fill="#e2e8f0" stroke="#475569" strokeWidth={3} />
      <circle r={r * 0.78} fill="#0b1222" />
      <g ref={spin}>
        {[0, 60, 120, 180, 240, 300].map((a) => (
          <path key={a} d={`M0 ${-r * 0.7} Q${r * 0.26} ${-r * 0.3} 0 0 Q${-r * 0.16} ${-r * 0.32} 0 ${-r * 0.7}`} fill="#94a3b8" transform={`rotate(${a})`} />
        ))}
        <circle r={8} fill="#475569" />
      </g>
      {p.motor !== false && (
        <g transform={`translate(${r + 8} -17)`}>
          <rect width={60} height={34} rx={6} fill="#475569" />
          {[10, 22, 34, 46].map((x) => (
            <line key={x} x1={x} x2={x} y1={0} y2={34} stroke="#334155" strokeWidth={2} />
          ))}
        </g>
      )}
    </g>
  );
};

const Vsd: Renderer = ({ v, alarm }) => (
  <g>
    <rect width={70} height={46} rx={4} fill="#1e293b" stroke={alarm ? '#ef4444' : '#22d3ee'} />
    <text x={35} y={18} className="mn-tag-text" textAnchor="middle">
      VSD
    </text>
    <text x={35} y={37} className="mn-value" fontSize={12} textAnchor="middle">
      {on(v.fault) ? 'FAULT' : formatValue(v.hz, 1, 'Hz')}
    </text>
  </g>
);

/** ISA-style instrument bubble (TT, DPT, TS, FS…), optional value above. */
const Tag: Renderer = ({ v, p }) => {
  const r = num(p, 'r', 17);
  const stem = num(p, 'stem', 0);
  return (
    <g>
      {stem !== 0 && <line x1={0} y1={stem > 0 ? r : -r} x2={0} y2={stem > 0 ? r + stem : -r + stem} stroke="#22d3ee" />}
      <circle r={r} className="mn-tag" />
      <text y={4} className="mn-tag-text" textAnchor="middle">
        {str(p, 'code')}
      </text>
      {'value' in v && (
        <text y={-r - 12} className="mn-value" textAnchor="middle">
          {formatValue(v.value, num(p, 'decimals', 1), str(p, 'unit'))}
        </text>
      )}
    </g>
  );
};

/** Local dial gauge (PI) with needle. */
const Gauge: Renderer = ({ v, p }) => {
  const max = num(p, 'max', 300);
  const val = v.value ?? 0;
  const angle = -90 + Math.min(180, (val / max) * 180);
  return (
    <g>
      <circle r={17} className="mn-tag" />
      <line x1={0} y1={0} x2={0} y2={-13} stroke="#22d3ee" strokeWidth={2} transform={`rotate(${angle})`} className="mn-transition" />
      <text y={12} className="mn-tag-text" textAnchor="middle">
        {str(p, 'code', 'PI')}
      </text>
      <text x={26} y={-10} className="mn-value" fontSize={11}>
        {formatValue(v.value, 0, 'Pa')}
      </text>
    </g>
  );
};

const Duct: Renderer = ({ p }) => <rect width={num(p, 'w', 100)} height={num(p, 'h', 44)} className="mn-duct" />;

const Line: Renderer = ({ p }) => (
  <path d={str(p, 'd')} stroke={str(p, 'color', '#22d3ee')} strokeWidth={num(p, 'width', 1)} strokeDasharray={str(p, 'dash') || undefined} fill="none" />
);

/** Moving air/water stream; speed 0–100 %. */
const Flow: Renderer = ({ v, p }) => {
  const active = 'active' in v ? on(v.active) : true;
  const speed = active ? 20 + (v.speed ?? 50) * 0.8 : 0;
  const ref = useDash<SVGPathElement>(speed);
  return (
    <path
      ref={ref}
      d={str(p, 'd')}
      className="mn-flow"
      stroke={str(p, 'color', '#e879f9')}
      strokeWidth={num(p, 'width', 6)}
      opacity={active ? num(p, 'opacity', 1) : 0.12}
    />
  );
};

const Vrf: Renderer = ({ v, alarm }) => {
  const spin = useSpin<SVGGElement>(on(v.on) ? 200 : 0);
  return (
    <g>
      <rect width={90} height={100} rx={4} fill="url(#mnMetal)" stroke="#475569" />
      <g transform="translate(45 44)">
        <circle r={24} fill="#0b1222" stroke="#475569" />
        <g ref={spin}>
          <path d="M0 -20 Q7 -7 0 0 Q-7 -7 0 -20 M20 0 Q7 7 0 0 Q7 -7 20 0 M0 20 Q-7 7 0 0 Q7 7 0 20 M-20 0 Q-7 -7 0 0 Q-7 7 -20 0" fill="#94a3b8" />
        </g>
      </g>
      <circle cx={11} cy={11} r={5} fill={alarm ? '#ef4444' : on(v.on) ? '#22c55e' : '#475569'} />
      <text x={45} y={90} className="mn-tag-text" fill="#c2410c" fontSize={14} textAnchor="middle" style={{ fill: '#c2410c' }}>
        VRF
      </text>
    </g>
  );
};

const AcUnit: Renderer = ({ p, text }) => {
  const spin = useSpin<SVGGElement>(250);
  return (
    <g>
      <rect width={110} height={60} rx={4} fill="#2a2a1a" stroke="#a3a36b" />
      <g transform="translate(30 30)">
        <circle r={18} fill="#0b1222" stroke="#a3a36b" />
        <g ref={spin}>
          <path d="M0 -15 Q6 -5 0 0 Q-6 -5 0 -15 M15 0 Q5 6 0 0 Q5 -6 15 0 M0 15 Q-6 5 0 0 Q6 5 0 15 M-15 0 Q-5 -6 0 0 Q-5 6 -15 0" fill="#a3a36b" />
        </g>
      </g>
      <text x={80} y={27} className="mn-small" textAnchor="middle">
        AC
      </text>
      <text x={80} y={43} className="mn-small" textAnchor="middle">
        INV
      </text>
      <text x={55} y={80} className="mn-small" textAnchor="middle">
        {text(p.caption as Loc)}
      </text>
    </g>
  );
};

const Ffu: Renderer = ({ v, p }) => {
  const spin = useSpin<SVGGElement>(on(v.running) ? 500 : 0);
  return (
    <g>
      <rect x={-40} y={-10} width={80} height={20} fill="#14532d" stroke="#4ade80" />
      <circle r={8} fill="#0b1222" stroke="#4ade80" />
      <g ref={spin}>
        <path d="M0 -7 L2 0 L0 7 L-2 0 z M-7 0 L0 2 L7 0 L0 -2 z" fill="#4ade80" />
      </g>
      <text y={26} className="mn-small" textAnchor="middle" style={{ fill: '#60a5fa' }}>
        {str(p, 'label', '440 cfm')}
      </text>
    </g>
  );
};

const Room: Renderer = ({ v, p, alarm, text }) => {
  const w = num(p, 'w', 700), h = num(p, 'h', 260);
  return (
    <g>
      <rect width={w} height={h} fill="#0b1f17" stroke="#4ade80" strokeWidth={2} className={alarm ? 'mn-alarm' : undefined} />
      <text x={w / 2} y={h / 2 - 20} className="mn-label" fontSize={16} fill="#4ade80" textAnchor="middle" style={{ unicodeBidi: 'plaintext' }}>
        {text(p.title as Loc)}
      </text>
      <text x={w / 2} y={h / 2 + 2} className="mn-small" textAnchor="middle">
        {str(p, 'info')}
      </text>
      <text x={w / 2} y={h / 2 + 50} className="mn-value" fontSize={28} textAnchor="middle">
        {formatValue(v.t, 1, '°C')}
      </text>
      {'sp' in v && (
        <text x={w / 2} y={h / 2 + 74} className="mn-small" textAnchor="middle">
          {text({ i18n: 'mnemo.setpoint' })} {formatValue(v.sp, 1, '°C')}
        </text>
      )}
    </g>
  );
};

const Relief: Renderer = ({ v }) => (
  <g>
    <rect width={20} height={50} fill="#1e293b" stroke="#94a3b8" />
    <g transform={on(v.open) ? 'rotate(-25 10 25)' : undefined} stroke="#cbd5e1" strokeWidth={2} className="mn-transition">
      <line x1={2} y1={12} x2={18} y2={12} />
      <line x1={2} y1={25} x2={18} y2={25} />
      <line x1={2} y1={38} x2={18} y2={38} />
    </g>
  </g>
);

const Band: Renderer = ({ p }) => <rect width={num(p, 'w', 1400)} height={num(p, 'h', 14)} fill="#1e293b" />;

export const elementLibrary: Record<string, Renderer> = {
  box3d: Box3d,
  label: Label,
  value: Value,
  filter: Filter,
  coil: Coil,
  heater: Heater,
  fan: Fan,
  vsd: Vsd,
  tag: Tag,
  gauge: Gauge,
  duct: Duct,
  line: Line,
  flow: Flow,
  vrf: Vrf,
  acUnit: AcUnit,
  ffu: Ffu,
  room: Room,
  relief: Relief,
  band: Band,
};
