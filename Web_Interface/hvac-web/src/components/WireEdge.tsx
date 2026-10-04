import { useRef } from 'react';
import { BaseEdge, EdgeLabelRenderer, Position, getBezierPath, getStraightPath, useReactFlow, type Edge, type EdgeProps } from '@xyflow/react';
import type { LinkStyle, SchemePoint } from '../api/client';

export type WireData = {
  style: LinkStyle;
  points: SchemePoint[];
  color: string;
  editable: boolean;
  onRoute: (id: string, points: SchemePoint[], commit: boolean) => void;
};
export type WireEdgeType = Edge<WireData, 'wire'>;

type Pt = SchemePoint;

const STUB = 18;
const GRID = 10;
/** Snap distance in screen pixels. */
const ALIGN = 10;
const RADIUS = 8;
const BUMP = 30;

const DIR: Record<Position, Pt> = {
  [Position.Top]: { x: 0, y: -1 },
  [Position.Bottom]: { x: 0, y: 1 },
  [Position.Left]: { x: -1, y: 0 },
  [Position.Right]: { x: 1, y: 0 },
};

const stub = (p: Pt, pos: Position): Pt => ({ x: p.x + DIR[pos].x * STUB, y: p.y + DIR[pos].y * STUB });
const same = (a: Pt, b: Pt) => Math.abs(a.x - b.x) < 0.5 && Math.abs(a.y - b.y) < 0.5;
const snapGrid = (v: number) => Math.round(v / GRID) * GRID;

/** Right-angle route through the points: an elbow is added wherever two neighbours are not aligned. */
function orthogonal(chain: Pt[]): Pt[] {
  const out = [chain[0]];
  for (let i = 1; i < chain.length; i++) {
    const a = out[out.length - 1];
    const b = chain[i];
    if (Math.abs(a.x - b.x) > 0.5 && Math.abs(a.y - b.y) > 0.5) {
      const prev = out[out.length - 2];
      const cameHorizontal = prev ? Math.abs(prev.y - a.y) < 0.5 : false;
      out.push(cameHorizontal ? { x: a.x, y: b.y } : { x: b.x, y: a.y });
    }
    out.push(b);
  }
  return out;
}

/**
 * Removes corners that no longer bend the line: repeated points and points lying on a straight run.
 * The port points and their stubs (two at each end) always stay.
 */
function simplify(pts: Pt[], straightTolerance: number): Pt[] {
  const out = [...pts];
  let changed = true;
  while (changed) {
    changed = false;
    for (let i = 2; i < out.length - 2; i++) {
      const [a, b, c] = [out[i - 1], out[i], out[i + 1]];
      const len = Math.hypot(c.x - a.x, c.y - a.y);
      const off = len < 0.5 ? 0 : Math.abs((c.x - a.x) * (a.y - b.y) - (a.x - b.x) * (c.y - a.y)) / len;
      // Also drops a point where the line doubles back on itself.
      if (same(a, b) || same(b, c) || off <= straightTolerance) {
        out.splice(i, 1);
        changed = true;
        break;
      }
    }
  }
  return out;
}

/** Default right-angle route between the two stubs, like the automatic one. */
function defaultCorners(s1: Pt, t1: Pt, sourcePos: Position): Pt[] {
  if (sourcePos === Position.Top || sourcePos === Position.Bottom) {
    const y = snapGrid((s1.y + t1.y) / 2);
    return [
      { x: s1.x, y },
      { x: t1.x, y },
    ];
  }
  const x = snapGrid((s1.x + t1.x) / 2);
  return [
    { x, y: s1.y },
    { x, y: t1.y },
  ];
}

function rounded(pts: Pt[], r: number): string {
  let d = `M ${pts[0].x} ${pts[0].y}`;
  for (let i = 1; i < pts.length - 1; i++) {
    const [a, b, c] = [pts[i - 1], pts[i], pts[i + 1]];
    const l1 = Math.hypot(b.x - a.x, b.y - a.y);
    const l2 = Math.hypot(c.x - b.x, c.y - b.y);
    const k = Math.min(r, l1 / 2, l2 / 2);
    if (k < 0.5) {
      d += ` L ${b.x} ${b.y}`;
      continue;
    }
    const p1 = { x: b.x + ((a.x - b.x) / l1) * k, y: b.y + ((a.y - b.y) / l1) * k };
    const p2 = { x: b.x + ((c.x - b.x) / l2) * k, y: b.y + ((c.y - b.y) / l2) * k };
    d += ` L ${p1.x} ${p1.y} Q ${b.x} ${b.y} ${p2.x} ${p2.y}`;
  }
  const last = pts[pts.length - 1];
  return `${d} L ${last.x} ${last.y}`;
}

function curve(pts: Pt[]): string {
  let d = `M ${pts[0].x} ${pts[0].y}`;
  for (let i = 0; i < pts.length - 1; i++) {
    const p0 = pts[i - 1] ?? pts[i];
    const [p1, p2] = [pts[i], pts[i + 1]];
    const p3 = pts[i + 2] ?? p2;
    d += ` C ${p1.x + (p2.x - p0.x) / 6} ${p1.y + (p2.y - p0.y) / 6} ${p2.x - (p3.x - p1.x) / 6} ${p2.y - (p3.y - p1.y) / 6} ${p2.x} ${p2.y}`;
  }
  return d;
}

function segmentDistance(p: Pt, a: Pt, b: Pt) {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const t = dx || dy ? Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy))) : 0;
  return Math.hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}

/** Snaps a coordinate to the grid, then to the same coordinate of any other point of the wire (so levels merge). */
function snapAxis(raw: number, others: number[], align: number) {
  let best = snapGrid(raw);
  let dist = align;
  for (const o of others) {
    const d = Math.abs(raw - o);
    if (d < dist) [best, dist] = [o, d];
  }
  return best;
}

/**
 * Moves segment i..i+1 of a right-angle route across itself to `value`.
 * A port stub cannot move, so a copy of it is inserted and moved instead.
 */
function moveSegment(route: Pt[], i: number, value: number): Pt[] {
  const w = route.map((p) => ({ ...p }));
  const horizontal = Math.abs(w[i].y - w[i + 1].y) < 0.5;
  let a = i;
  let b = i + 1;
  if (a <= 1) {
    w.splice(a + 1, 0, { ...w[a] });
    a++;
    b++;
  }
  if (b >= w.length - 2) w.splice(b, 0, { ...w[b] });
  for (const k of [a, b]) {
    if (horizontal) w[k].y = value;
    else w[k].x = value;
  }
  return w;
}

type Drag = { base: Pt[]; kind: 'segment' | 'corner' | 'free'; index: number; points: Pt[]; from: Pt; origin: Pt; align: number };

/**
 * Wire between two connection points.
 * Right-angle wire: every corner is a square (drag, double-click removes), the middle of every segment
 * moves the whole segment; levels snap to each other and merge into one straight line.
 * Straight / smooth wire: free bends plus "+" to add a bend. Double-click on the wire adds a detour there.
 */
export function WireEdge({ id, sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition, selected, data }: EdgeProps<WireEdgeType>) {
  const flow = useReactFlow();
  const drag = useRef<Drag | null>(null);
  const style = data?.style ?? 'step';
  const points = data?.points ?? [];
  const S = { x: sourceX, y: sourceY };
  const T = { x: targetX, y: targetY };
  const S1 = stub(S, sourcePosition);
  const T1 = stub(T, targetPosition);
  const step = style === 'step';

  // Full right-angle route: port, stub, corners, stub, port.
  const route = step ? simplify(orthogonal([S, S1, ...(points.length ? points : defaultCorners(S1, T1, sourcePosition)), T1, T]), 0.5) : [];
  const cornersOf = (r: Pt[]) => simplify(orthogonal(r), 0.5).slice(2, -2);

  let path: string;
  let mid: Pt = { x: (S.x + T.x) / 2, y: (S.y + T.y) / 2 };
  if (step) path = rounded(route, RADIUS);
  else if (points.length) path = style === 'curve' ? curve([S, ...points, T]) : rounded([S, S1, ...points, T1, T], 4);
  else {
    const args = { sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition };
    const [p, lx, ly] = style === 'curve' ? getBezierPath(args) : getStraightPath(args);
    path = p;
    mid = { x: lx, y: ly };
  }

  const control = [S, ...points, T];
  const toFlow = (e: { clientX: number; clientY: number }) => flow.screenToFlowPosition({ x: e.clientX, y: e.clientY });

  const compute = (d: Drag, pointer: Pt): Pt[] => {
    // Follows the mouse movement, so grabbing a handle off-centre does not make it jump.
    const p = { x: d.origin.x + pointer.x - d.from.x, y: d.origin.y + pointer.y - d.from.y };
    if (d.kind === 'segment') {
      const [a, b] = [d.base[d.index], d.base[d.index + 1]];
      const horizontal = Math.abs(a.y - b.y) < 0.5;
      const others = d.base.filter((_, k) => k !== d.index && k !== d.index + 1).map((q) => (horizontal ? q.y : q.x));
      return moveSegment(d.base, d.index, snapAxis(horizontal ? p.y : p.x, others, d.align)).slice(2, -2);
    }
    if (d.kind === 'corner') {
      // A corner joins one horizontal and one vertical segment: move both.
      const c = d.index;
      const others = d.base.filter((_, k) => Math.abs(k - c) > 1);
      const nx = snapAxis(p.x, others.map((q) => q.x), d.align);
      const ny = snapAxis(p.y, others.map((q) => q.y), d.align);
      const prevHorizontal = Math.abs(d.base[c - 1].y - d.base[c].y) < 0.5;
      // The later segment first: an inserted stub copy then does not shift the earlier one.
      const w = moveSegment(d.base, c, prevHorizontal ? nx : ny);
      return moveSegment(w, c - 1, prevHorizontal ? ny : nx).slice(2, -2);
    }
    const list = [...d.base];
    const neighbours = [S, S1, T1, T, ...list.filter((_, k) => k !== d.index)];
    list[d.index] = { x: snapAxis(p.x, neighbours.map((q) => q.x), d.align), y: snapAxis(p.y, neighbours.map((q) => q.y), d.align) };
    return list;
  };

  const finish = (pts: Pt[]) => (step ? cornersOf([S, S1, ...pts, T1, T]) : simplify([S, S1, ...pts, T1, T], 3).slice(2, -2));

  // Tracked on the window: the handle under the mouse is re-created while the route changes.
  const start = (e: React.PointerEvent, kind: Drag['kind'], index: number, base: Pt[]) => {
    if (!data) return;
    e.stopPropagation();
    e.preventDefault();
    const from = toFlow(e);
    const origin =
      kind === 'segment'
        ? { x: (base[index].x + base[index + 1].x) / 2, y: (base[index].y + base[index + 1].y) / 2 }
        : kind === 'corner'
          ? base[index]
          : (base[index] ?? from);
    drag.current = { base, kind, index, points, from, origin, align: ALIGN / flow.getZoom() };
    const move = (ev: PointerEvent) => {
      const d = drag.current;
      if (!d) return;
      d.points = compute(d, toFlow(ev));
      data.onRoute(id, d.points, false);
    };
    const end = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', end);
      window.removeEventListener('pointercancel', end);
      const d = drag.current;
      drag.current = null;
      if (d) data.onRoute(id, finish(d.points), true);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', end);
    window.addEventListener('pointercancel', end);
  };

  const startInsert = (e: React.PointerEvent, index: number) => {
    const list = [...points];
    list.splice(index, 0, toFlow(e));
    data?.onRoute(id, list, false);
    start(e, 'free', index, list);
  };

  const removeCorner = (index: number) => {
    if (!data) return;
    if (step) data.onRoute(id, cornersOf(route.filter((_, k) => k !== index)), true);
    else data.onRoute(id, points.filter((_, k) => k !== index), true);
  };

  /** Double-click on the wire: a detour to go around a block (right angle) or a new bend. */
  const addAt = (e: React.MouseEvent) => {
    if (!data?.editable) return;
    e.stopPropagation();
    const p = toFlow(e);
    if (step) {
      let best = 1;
      let dist = Infinity;
      for (let i = 1; i < route.length - 2; i++) {
        const dd = segmentDistance(p, route[i], route[i + 1]);
        if (dd < dist) [best, dist] = [i, dd];
      }
      const [a, b] = [route[best], route[best + 1]];
      const horizontal = Math.abs(a.y - b.y) < 0.5;
      const bump = horizontal
        ? [{ x: snapGrid(p.x) - BUMP, y: a.y }, { x: snapGrid(p.x) - BUMP, y: a.y + BUMP }, { x: snapGrid(p.x) + BUMP, y: a.y + BUMP }, { x: snapGrid(p.x) + BUMP, y: a.y }]
        : [{ x: a.x, y: snapGrid(p.y) - BUMP }, { x: a.x + BUMP, y: snapGrid(p.y) - BUMP }, { x: a.x + BUMP, y: snapGrid(p.y) + BUMP }, { x: a.x, y: snapGrid(p.y) + BUMP }];
      if (Math.sign(b.x - a.x) < 0 || Math.sign(b.y - a.y) < 0) bump.reverse();
      const next = [...route.slice(0, best + 1), ...bump, ...route.slice(best + 1)];
      data.onRoute(id, cornersOf(next), true);
      return;
    }
    let best = 0;
    let dist = Infinity;
    for (let i = 0; i < control.length - 1; i++) {
      const dd = segmentDistance(p, control[i], control[i + 1]);
      if (dd < dist) [best, dist] = [i, dd];
    }
    const list = [...points];
    list.splice(best, 0, { x: snapGrid(p.x), y: snapGrid(p.y) });
    data.onRoute(id, list, true);
  };

  const handle = (key: string, p: Pt, cls: string, props: React.HTMLAttributes<HTMLDivElement>) => (
    <div
      key={key}
      className={`wire-handle ${cls} nodrag nopan`}
      style={{ transform: `translate(-50%, -50%) translate(${p.x}px, ${p.y}px)`, borderColor: data?.color }}
      {...props}
    />
  );

  const editing = selected && data?.editable;
  return (
    <>
      <BaseEdge id={id} path={path} interactionWidth={0} style={{ stroke: data?.color ?? '#64748b', strokeWidth: selected ? 3.5 : 2.5 }} />
      <path d={path} className="react-flow__edge-interaction" fill="none" stroke="transparent" strokeWidth={16} onDoubleClick={addAt} />
      {editing && step && (
        <EdgeLabelRenderer>
          {route.slice(2, -2).map((p, k) =>
            handle(`c${k}`, p, 'wire-handle--bend', {
              onPointerDown: (e) => start(e, 'corner', k + 2, route),
              onDoubleClick: (e) => {
                e.stopPropagation();
                removeCorner(k + 2);
              },
            }),
          )}
          {route.slice(1, -2).map((a, k) => {
            const i = k + 1;
            const b = route[i + 1];
            if (Math.hypot(b.x - a.x, b.y - a.y) < 16) return null;
            const horizontal = Math.abs(a.y - b.y) < 0.5;
            return handle(`s${i}`, { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }, `wire-handle--segment ${horizontal ? 'wire-handle--h' : 'wire-handle--v'}`, {
              onPointerDown: (e) => start(e, 'segment', i, route),
            });
          })}
        </EdgeLabelRenderer>
      )}
      {editing && !step && (
        <EdgeLabelRenderer>
          {points.map((p, k) =>
            handle(`b${k}`, p, 'wire-handle--bend', {
              onPointerDown: (e) => start(e, 'free', k, points),
              onDoubleClick: (e) => {
                e.stopPropagation();
                removeCorner(k);
              },
            }),
          )}
          {points.length === 0
            ? handle('m0', mid, 'wire-handle--add', { onPointerDown: (e) => startInsert(e, 0) })
            : control.slice(0, -1).map((a, k) => {
                const b = control[k + 1];
                return handle(`m${k}`, { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }, 'wire-handle--add', { onPointerDown: (e) => startInsert(e, k) });
              })}
        </EdgeLabelRenderer>
      )}
    </>
  );
}
