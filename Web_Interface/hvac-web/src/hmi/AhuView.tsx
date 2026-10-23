import { useEffect, useRef, useState, type MutableRefObject, type PointerEvent as ReactPointerEvent } from 'react';
import { useTranslation } from 'react-i18next';
import type { HmiAhuItem, HmiScreenContent } from '../api/client';
import { pickName } from '../lib/localized';
import { artLibrary, Box, slotsFor, demoValues, DX, DY, formatSlot, HmiDefs, P, pts, stateOf, type HmiState } from './arts';
import { isPipeArt, pipeRot, pipeSize, snapPipe } from './pipeGeom';

export const CASE_H = 150;
const PAD_L = 176;
const PAD_R = 160;
const TOP = DY + 34;
const BOTTOM = 76;
const ZOOM = 1.4;

export function ahuScreenProps(c: HmiScreenContent) {
  return {
    items: c.items ?? [],
    ducts: c.ducts ?? [],
    pipes: c.pipes ?? [],
    instruments: c.instruments ?? [],
    flowTag: c.flowTag,
  };
}

type Props = {
  items: HmiAhuItem[];
  ducts?: HmiAhuItem[];
  pipes?: HmiAhuItem[];
  instruments?: HmiAhuItem[];
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
  /** Fill the parent (10" panel preview) instead of a fixed zoom. */
  fit?: boolean;
  /** picture = casing only (for the snapshot); overlay = numbers and arrows on top. */
  layer?: 'full' | 'picture' | 'overlay';
  /** Draw the flow arrows. Off when baking a still for the panel. */
  showFlow?: boolean;
  /** Hide spinning blades so the panel can turn the fan itself. */
  still?: boolean;
  /** Pipe from the palette, follows the mouse until drop. */
  placing?: HmiAhuItem | null;
  placingHold?: MutableRefObject<HmiAhuItem | null>;
  onDropPipe?: (item: HmiAhuItem) => void;
  onCancelPlace?: () => void;
  onPipeMove?: (uid: string, x: number, y: number) => void;
};

type Drag = { uid: string; from: number; startX: number; dx: number };

/** Air handling unit seen at an angle: glass casing, sections glued in airflow order, units drawn inside. */
export function AhuView({
  items,
  ducts = [],
  pipes = [],
  instruments = [],
  values,
  flowTag,
  demo,
  selected,
  onSelect,
  onMove,
  showNames = false,
  bare = false,
  fit = false,
  layer = 'full',
  showFlow = true,
  still = false,
  placing = null,
  placingHold,
  onDropPipe,
  onCancelPlace,
  onPipeMove,
}: Props) {
  const { i18n } = useTranslation();
  const [drag, setDrag] = useState<Drag | null>(null);
  const [pipeDrag, setPipeDrag] = useState<{ uid: string; gx: number; gy: number; x: number; y: number } | null>(null);
  const [ghost, setGhost] = useState<{ x: number; y: number } | null>(null);
  const svgRef = useRef<SVGSVGElement>(null);
  const H = CASE_H;

  const xs: number[] = [];
  let T = 0;
  for (const it of items) {
    xs.push(T);
    T += it.length;
  }
  T = Math.max(T, 60);
  const hood = !bare && items[0]?.art === 'inlet';
  const ductCollar = !bare && items[items.length - 1]?.art === 'outlet';
  const inDucts = ducts.filter((d) => d.side !== 'out');
  const outDucts = ducts.filter((d) => d.side === 'out');
  const inLen = inDucts.reduce((s, d) => s + d.length, 0);
  const outLen = outDucts.reduce((s, d) => s + d.length, 0);
  const inStart = -(hood ? 38 : 0) - inLen;
  const outStart = T + (ductCollar ? 46 : 0);
  const laid = layoutPipes(pipes, items, xs, H);
  const livePipes = laid.map((it) => (pipeDrag && it.uid === pipeDrag.uid ? { ...it, x: pipeDrag.x, y: pipeDrag.y } : it));
  const padL = bare ? 8 : Math.max(PAD_L, 48 + (hood ? 38 : 0) + inLen);
  const padR = (bare ? 8 : Math.max(PAD_R, 48 + (ductCollar ? 46 : 0) + outLen)) + DX;
  const top = TOP;
  let extraR = 0;
  let extraB = 0;
  for (const it of laid) {
    if (pipeDrag?.uid === it.uid) continue;
    const s = pipeSize(it.art, it.length, pipeRot(it));
    extraR = Math.max(extraR, (it.x ?? 0) + s.w - T);
    extraB = Math.max(extraB, (it.y ?? 0) + s.h - H - BOTTOM + 16);
  }
  const width = padL + T + padR + extraR;
  const height = top + H + BOTTOM + extraB;
  const air = demo ? (demo === 'stopped' ? 0 : 80) : flowTag && values ? (values[flowTag] ?? 0) : 0;
  const flowTone = (() => {
    let coolOn = false;
    let heatOn = false;
    for (const it of items) {
      const v: Record<string, number | undefined> = {};
      if (demo) Object.assign(v, demoValues(it.art, demo, it.params));
      else for (const [slot, tag] of Object.entries(it.bind ?? {})) if (tag && values) v[slot] = values[tag];
      if (it.art === 'coil' && (v.on ?? 0) > 0) coolOn = true;
      if (it.art === 'heater' && (v.stages ?? 0) > 0) heatOn = true;
    }
    const out: 'cool' | 'warm' = heatOn && !coolOn ? 'warm' : 'cool';
    return { in: (out === 'cool' ? 'warm' : 'cool') as 'cool' | 'warm', out };
  })();

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

  const localPoint = (clientX: number, clientY: number) => {
    const svg = svgRef.current;
    if (!svg) return { x: 0, y: 0 };
    const pt = svg.createSVGPoint();
    pt.x = clientX;
    pt.y = clientY;
    const ctm = svg.getScreenCTM();
    if (!ctm) return { x: 0, y: 0 };
    const p = pt.matrixTransform(ctm.inverse());
    return { x: p.x - padL, y: p.y - top };
  };

  const liveRef = useRef(livePipes);
  liveRef.current = livePipes;
  const placingRef = useRef(placing);
  placingRef.current = placing;
  const ghostRef = useRef(ghost);
  ghostRef.current = ghost;
  const pipeDragRef = useRef(pipeDrag);
  pipeDragRef.current = pipeDrag;
  const localRef = useRef(localPoint);
  localRef.current = localPoint;
  const dropRef = useRef(onDropPipe);
  dropRef.current = onDropPipe;
  const cancelRef = useRef(onCancelPlace);
  cancelRef.current = onCancelPlace;
  const movePipeRef = useRef(onPipeMove);
  movePipeRef.current = onPipeMove;
  const holdRef = useRef(placingHold);
  holdRef.current = placingHold;
  const editPipes = !!(onDropPipe || onPipeMove);

  useEffect(() => {
    if (!editPipes) return;
    const overSvg = (e: PointerEvent) => {
      const r = svgRef.current?.getBoundingClientRect();
      return !!r && e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom;
    };
    const carryOf = () => placingRef.current ?? holdRef.current?.current ?? null;
    const move = (e: PointerEvent) => {
      const loc = localRef.current(e.clientX, e.clientY);
      const carry = carryOf();
      const d = pipeDragRef.current;
      if (carry) {
        if (!overSvg(e)) {
          setGhost(null);
          return;
        }
        const s = pipeSize(carry.art, carry.length, pipeRot(carry));
        const raw = { ...carry, x: loc.x - s.w / 2, y: loc.y - s.h / 2 };
        setGhost(snapPipe(raw, liveRef.current));
        return;
      }
      if (d) {
        const it = liveRef.current.find((p) => p.uid === d.uid);
        if (!it) return;
        const sn = snapPipe({ ...it, x: loc.x - d.gx, y: loc.y - d.gy }, liveRef.current.filter((p) => p.uid !== d.uid));
        setPipeDrag({ ...d, x: sn.x, y: sn.y });
      }
    };
    const up = (e: PointerEvent) => {
      const carry = carryOf();
      const d = pipeDragRef.current;
      if (carry) {
        const loc = localRef.current(e.clientX, e.clientY);
        const s = pipeSize(carry.art, carry.length, pipeRot(carry));
        const g = ghostRef.current ?? snapPipe({ ...carry, x: loc.x - s.w / 2, y: loc.y - s.h / 2 }, liveRef.current);
        if (overSvg(e)) {
          const rot = pipeRot(carry);
          dropRef.current?.({ ...carry, x: g.x, y: g.y, rot, params: { ...carry.params, rot } });
        } else cancelRef.current?.();
        if (holdRef.current) holdRef.current.current = null;
        setGhost(null);
      }
      if (d) {
        movePipeRef.current?.(d.uid, d.x, d.y);
        setPipeDrag(null);
      }
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    return () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
  }, [editPipes]);

  const tap = (uid: string) => (e: ReactPointerEvent<SVGGElement>) => {
    e.stopPropagation();
    onSelect?.(uid);
  };
  const down = (uid: string, from: number) => (e: ReactPointerEvent<SVGGElement>) => {
    e.stopPropagation();
    onSelect?.(uid);
    if (!onMove) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    setDrag({ uid, from, startX: e.clientX, dx: 0 });
  };
  const move = (e: ReactPointerEvent<SVGGElement>) => drag && setDrag({ ...drag, dx: (e.clientX - drag.startX) * scale() });
  const up = () => {
    if (!drag) return;
    const to = dropIndex(drag);
    setDrag(null);
    if (Math.abs(drag.dx) > 4 && to !== drag.from) onMove?.(drag.from, to);
  };
  const downPipe = (it: HmiAhuItem) => (e: ReactPointerEvent<SVGGElement>) => {
    e.stopPropagation();
    onSelect?.(it.uid);
    if (!onPipeMove) return;
    const loc = localPoint(e.clientX, e.clientY);
    setPipeDrag({ uid: it.uid, gx: loc.x - (it.x ?? 0), gy: loc.y - (it.y ?? 0), x: it.x ?? 0, y: it.y ?? 0 });
  };

  let marker: number | null = null;
  if (drag && Math.abs(drag.dx) > 4) {
    const to = dropIndex(drag);
    marker = items.filter((_, i) => i !== drag.from).slice(0, to).reduce((s, it) => s + it.length, 0);
  }

  const order = items.map((it, i) => [it, i] as const).sort((a, b) => Number(a[0].uid === drag?.uid) - Number(b[0].uid === drag?.uid));
  const picture = layer !== 'overlay';
  const live = layer !== 'picture';
  const canvasClass = [
    fit ? 'hmi-canvas hmi-canvas--fit' : 'hmi-canvas',
    layer === 'overlay' ? 'hmi-canvas--overlay' : '',
    still ? 'hmi-canvas--still' : '',
  ]
    .filter(Boolean)
    .join(' ');

  return (
    <div className={canvasClass}>
      <svg
        ref={svgRef}
        viewBox={`0 0 ${width} ${height}`}
        width={fit ? undefined : width * ZOOM}
        overflow="visible"
        className={fit ? 'hmi-svg hmi-svg--fit' : 'hmi-svg'}
        preserveAspectRatio="xMidYMid meet"
        onPointerDown={() => {
          if (!placing && !placingHold?.current) onSelect?.(null);
        }}
      >
        <HmiDefs />
        <g transform={`translate(${padL} ${top})`}>
          {picture && (
          <>
          <polygon points={pts([[-30, H + 14, 0], [T + 30, H + 14, 0], [T + 30, H + 14, 1.2], [-30, H + 14, 1.2]])} fill="rgba(80,90,100,0.18)" />
          <Box x0={-3} x1={T + 3} y0={H} y1={H + 14} z0={0} z1={1} front="#6b7280" top="#9ca3af" side="#4b5563" stroke="#374151" />
          {hood && (
            <g>
              <Box x0={-38} x1={0} y0={16} y1={H - 16} z0={0.12} z1={0.88} front="#c5cdd6" top="#e8edf2" side="#8b949e" stroke="#64748b" />
              {Array.from({ length: 6 }, (_, i) => {
                const y = 26 + i * ((H - 52) / 5);
                const [x1, y1] = P(-34, y, 0.12);
                const [x2, y2] = P(-4, y + 6, 0.12);
                return <line key={i} x1={x1} y1={y1} x2={x2} y2={y2} stroke="#64748b" strokeWidth={2.5} />;
              })}
            </g>
          )}

          <polygon points={pts([[0, 0, 1], [T, 0, 1], [T, H, 1], [0, H, 1]])} fill="#2a313a" stroke="#1f252c" />
          <polygon points={pts([[0, H, 0], [T, H, 0], [T, H, 1], [0, H, 1]])} fill="#4a5562" stroke="#2a313a" />
          {xs.slice(1).map((x) => (
            <g key={x} stroke="#2f3842" strokeWidth={3}>
              <line x1={P(x, 0, 1)[0]} y1={P(x, 0, 1)[1]} x2={P(x, H, 1)[0]} y2={P(x, H, 1)[1]} />
              <line x1={P(x, H, 0)[0]} y1={P(x, H, 0)[1]} x2={P(x, H, 1)[0]} y2={P(x, H, 1)[1]} />
            </g>
          ))}
          </>
          )}

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
                {picture && (
                  <>
                    <polygon points={pts([[0, H, 0], [0, 0, 0], [0, 0, 1], [w, 0, 1], [w, H, 1], [w, H, 0]])} fill="transparent" />
                    {R && <R w={w} h={H} v={v} p={it.params ?? {}} state={state} air={air} />}
                    <rect x={3} y={4} width={w - 6} height={H - 8} fill="none" stroke="#c5cdd6" strokeWidth={2.5} />
                    <rect x={w - 12} y={H * 0.28} width={5} height={14} rx={1} fill="#9ca3af" />
                    <rect x={w - 12} y={H * 0.6} width={5} height={14} rx={1} fill="#9ca3af" />
                    {state === 'alarm' && <polygon points={front} fill="rgba(239,68,68,0.12)" stroke="#ef4444" strokeWidth={3} className="mn-alarm" />}
                    {selected === it.uid && <polygon points={front} fill="rgba(34,211,238,0.08)" stroke="#0891b2" strokeWidth={3} strokeDasharray="7 4" />}
                    {showNames && (
                      <text x={w / 2 + DX / 2} y={-DY - 10} className="hmi-name" textAnchor="middle">
                        {short(pickName(it.name, i18n.language).value, w)}
                      </text>
                    )}
                  </>
                )}
                {live &&
                  shown.map((s, k) => (
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

          {picture && (
          <g pointerEvents="none">
          <polygon points={pts([[0, 0, 0], [T, 0, 0], [T, 0, 1], [0, 0, 1]])} fill="rgba(197,205,214,0.28)" stroke="#9aa3ad" strokeWidth={2} />
          <polygon points={pts([[T, 0, 0], [T, 0, 1], [T, H, 1], [T, H, 0]])} fill="rgba(156,163,175,0.4)" stroke="#9aa3ad" strokeWidth={2} />
          <rect x={0} y={0} width={T} height={H} fill="none" stroke="#c5cdd6" strokeWidth={4} />
          {xs.slice(1).map((x) => (
            <g key={x} stroke="#c5cdd6" strokeWidth={3}>
              <line x1={x} y1={0} x2={x} y2={H} />
              <line x1={x} y1={0} x2={P(x, 0, 1)[0]} y2={P(x, 0, 1)[1]} />
            </g>
          ))}
          </g>
          )}

          {picture && ductCollar && (
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

          {picture &&
            (() => {
              const draw = (it: HmiAhuItem, x: number, y: number, w: number, h: number, sc = 1, onDown?: (e: ReactPointerEvent<SVGGElement>) => void, pe = true) => {
                const v: Record<string, number | undefined> = {};
                if (demo) Object.assign(v, demoValues(it.art, demo, it.params));
                else for (const [slot, tag] of Object.entries(it.bind ?? {})) if (tag && values) v[slot] = values[tag];
                const state = demo ?? stateOf(it.art, v, air);
                const R = artLibrary[it.art];
                const pipe = isPipeArt(it.art);
                return (
                  <g
                    key={it.uid}
                    transform={`translate(${x} ${y}) scale(${sc})`}
                    className={[
                      onSelect ? 'hmi-section hmi-section--edit' : 'hmi-section',
                      pipe ? 'hmi-pipe' : '',
                      pipe && pipeDrag?.uid === it.uid ? 'hmi-pipe--drag' : '',
                    ].filter(Boolean).join(' ')}
                    pointerEvents={pe ? undefined : 'none'}
                    opacity={pe ? 1 : 0.55}
                    onPointerDown={pe ? (onDown ?? tap(it.uid)) : undefined}
                  >
                    {R && <R w={w} h={h} v={v} p={it.params ?? {}} state={state} air={air} />}
                    {selected === it.uid && (
                      <rect x={1} y={1} width={w - 2} height={h - 2} fill="none" stroke="#0891b2" strokeWidth={3} strokeDasharray="7 4" />
                    )}
                  </g>
                );
              };
              const boxes = new Map<string, { x: number; y: number; w: number; h: number }>();
              items.forEach((it, i) => boxes.set(it.uid, { x: xs[i], y: 0, w: it.length, h: H }));
              const bits = [];
              let dx = inStart;
              for (const it of inDucts) {
                boxes.set(it.uid, { x: dx, y: 0, w: it.length, h: H });
                bits.push(draw(it, dx, 0, it.length, H));
                dx += it.length;
              }
              let ox = outStart;
              for (const it of outDucts) {
                boxes.set(it.uid, { x: ox, y: 0, w: it.length, h: H });
                bits.push(draw(it, ox, 0, it.length, H));
                ox += it.length;
              }
              for (const it of livePipes) {
                const s = pipeSize(it.art, it.length, pipeRot(it));
                const x = it.x ?? 0;
                const y = it.y ?? 0;
                boxes.set(it.uid, { x, y, w: s.w, h: s.h });
                bits.push(draw(it, x, y, s.w, s.h, 1, onPipeMove ? downPipe(it) : tap(it.uid)));
              }
              if (ghost && placing) {
                const s = pipeSize(placing.art, placing.length, pipeRot(placing));
                bits.push(draw({ ...placing, uid: 'ghost' }, ghost.x, ghost.y, s.w, s.h, 1, undefined, false));
              }
              const instOn = new Map<string, number>();
              for (const it of instruments) {
                const host = boxes.get(it.hostUid ?? '');
                if (!host) continue;
                const n = instOn.get(it.hostUid ?? '') ?? 0;
                instOn.set(it.hostUid ?? '', n + 1);
                const sc = 0.42;
                const w = 70;
                bits.push(draw(it, host.x + host.w * 0.55 + n * 30 - 8, host.y - 6, w, H, sc));
              }
              return bits;
            })()}

          {live && showFlow && !bare && (
            <FlowOverlay
              length={T}
              h={H}
              on={air > 0}
              hood={hood}
              duct={ductCollar}
              inn={flowTone.in}
              out={flowTone.out}
              xIn={inDucts.length ? inStart - 90 : undefined}
              xOut={outDucts.length ? outStart + outLen + 12 : undefined}
            />
          )}

          {live &&
            items.map((it, i) =>
              it.art === 'filter' && it.params?.label ? (
                <g key={`fl-${it.uid}`} pointerEvents="none" className="hmi-filter-chip" transform={`translate(${xs[i] + it.length / 2} 18)`}>
                  <rect x={-20} y={-10} width={40} height={18} rx={4} fill="#fff" stroke="#64748b" />
                  <text y={4} textAnchor="middle" className="hmi-chip-text">
                    {it.params.label}
                  </text>
                </g>
              ) : null,
            )}

          {picture && marker !== null && <line x1={marker} y1={-DY - 4} x2={marker} y2={H + 16} stroke="#0891b2" strokeWidth={4} />}
        </g>
      </svg>
    </div>
  );
}

const short = (s: string, len: number) => {
  const max = Math.max(4, Math.floor(len / 6));
  return s.length > max ? `${s.slice(0, max - 1)}…` : s;
};

function layoutPipes(pipes: HmiAhuItem[], items: HmiAhuItem[], xs: number[], H: number): HmiAhuItem[] {
  const nOn = new Map<string, number>();
  return pipes.map((it) => {
    if (it.x != null && it.y != null) return it;
    const i = items.findIndex((s) => s.uid === it.hostUid);
    if (i < 0) return { ...it, x: 0, y: H + 20 };
    const n = nOn.get(it.hostUid ?? '') ?? 0;
    nOn.set(it.hostUid ?? '', n + 1);
    return { ...it, x: xs[i], y: H + 12 + n * 28 };
  });
}

/** Arrows outside the casing, left of intake and right of the isometric supply box. */
function FlowOverlay({
  length,
  h,
  on,
  hood,
  duct,
  inn,
  out,
  xIn: xInOverride,
  xOut: xOutOverride,
}: {
  length: number;
  h: number;
  on: boolean;
  hood: boolean;
  duct: boolean;
  inn: 'cool' | 'warm';
  out: 'cool' | 'warm';
  xIn?: number;
  xOut?: number;
}) {
  const xIn = xInOverride ?? (hood ? -132 : -86);
  const [xExit, yExit] = P(length + 46, h * 0.5, 0.5);
  const xOut = xOutOverride ?? (duct ? xExit + 16 : length + 32);
  const y = duct ? yExit : h * 0.5 - DY * 0.5;
  return (
    <g pointerEvents="none">
      <FlowArrow x={xIn} y={y} on={on} tone={inn} />
      <FlowArrow x={xOut} y={y} on={on} tone={out} />
    </g>
  );
}

function FlowArrow({ x, y, on, tone }: { x: number; y: number; on: boolean; tone: 'cool' | 'warm' }) {
  const fill = tone === 'warm' ? 'url(#hmiFlowWarm)' : 'url(#hmiFlowCool)';
  const d = 'M0,-8 L44,-8 L44,-18 L74,0 L44,18 L44,8 L0,8 Z';
  return (
    <g transform={`translate(${x} ${y})`} opacity={on ? 1 : 0.22}>
      {[0, 1].map((i) => (
        <path
          key={i}
          d={d}
          fill={fill}
          className={on ? 'hmi-flow-arrow' : undefined}
          style={{ animationDelay: `${i * 0.7}s` }}
        />
      ))}
    </g>
  );
}
