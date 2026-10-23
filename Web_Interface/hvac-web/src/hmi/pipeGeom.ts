import type { HmiAhuItem } from '../api/client';

export const PIPE_D = 16;
export const ELBOW = 48;
export const SNAP = 36;
export type PipeDir = 'e' | 'w' | 'n' | 's';
export type PipePort = { x: number; y: number; dir: PipeDir };

const opp: Record<PipeDir, PipeDir> = { e: 'w', w: 'e', n: 's', s: 'n' };

export const pipeArts = ['pipeH', 'pipeV', 'pipeElbow', 'pipe3way', 'pipeValve', 'pipeSupply', 'pipeReturn', 'pipeStraight', 'pipeCondensate'];
export const isPipeArt = (art: string) => pipeArts.includes(art);
export const isFitting = (art: string) => art === 'pipeElbow' || art === 'pipe3way' || art === 'pipeValve';

export function pipeRot(it: { params?: { rot?: number }; rot?: number }) {
  const r = it.rot ?? it.params?.rot ?? 0;
  return ((Math.round(r / 90) * 90) % 360 + 360) % 360;
}

export function isUpright(rot: number) {
  return rot === 90 || rot === 270;
}

export function pipeSize(art: string, length: number, rot = 0): { w: number; h: number } {
  const L = Math.max(24, length);
  if (isFitting(art)) return { w: ELBOW, h: ELBOW };
  if (art === 'pipeV' || isUpright(rot)) return { w: PIPE_D + 8, h: L };
  return { w: L, h: PIPE_D + 8 };
}

export function portsOf(it: Pick<HmiAhuItem, 'art' | 'length' | 'params'> & { x?: number; y?: number; rot?: number }): PipePort[] {
  const x = it.x ?? 0;
  const y = it.y ?? 0;
  const rot = pipeRot(it);
  const { w, h } = pipeSize(it.art, it.length, rot);
  const cy = y + h / 2;
  const cx = x + w / 2;
  if (it.art === 'pipeElbow') return elbowPorts(x, y, w, h, rot);
  if (it.art === 'pipe3way' || it.art === 'pipeValve') return threeWayPorts(x, y, w, h, rot);
  if (it.art === 'pipeV' || isUpright(rot))
    return [
      { x: cx, y, dir: 'n' },
      { x: cx, y: y + h, dir: 's' },
    ];
  return [
    { x, y: cy, dir: 'w' },
    { x: x + w, y: cy, dir: 'e' },
  ];
}

function elbowPorts(x: number, y: number, w: number, h: number, rot: number): PipePort[] {
  const m = y + h / 2;
  const c = x + w / 2;
  if (rot === 90) return [{ x: c, y: y + h, dir: 's' }, { x: x + w, y: m, dir: 'e' }];
  if (rot === 180) return [{ x: x + w, y: m, dir: 'e' }, { x: c, y, dir: 'n' }];
  if (rot === 270) return [{ x: c, y, dir: 'n' }, { x, y: m, dir: 'w' }];
  return [{ x, y: m, dir: 'w' }, { x: c, y: y + h, dir: 's' }];
}

function threeWayPorts(x: number, y: number, w: number, h: number, rot: number): PipePort[] {
  const m = y + h / 2;
  const c = x + w / 2;
  if (rot === 90) return [{ x: c, y, dir: 'n' }, { x: c, y: y + h, dir: 's' }, { x: x + w, y: m, dir: 'e' }];
  if (rot === 180) return [{ x, y: m, dir: 'w' }, { x: x + w, y: m, dir: 'e' }, { x: c, y, dir: 'n' }];
  if (rot === 270) return [{ x: c, y, dir: 'n' }, { x: c, y: y + h, dir: 's' }, { x, y: m, dir: 'w' }];
  return [{ x, y: m, dir: 'w' }, { x: x + w, y: m, dir: 'e' }, { x: c, y: y + h, dir: 's' }];
}

export function snapPipe(moving: HmiAhuItem, others: HmiAhuItem[]): { x: number; y: number } {
  const x0 = moving.x ?? 0;
  const y0 = moving.y ?? 0;
  const mine = portsOf({ ...moving, x: x0, y: y0 });
  let best = { x: x0, y: y0, d: SNAP };
  for (const o of others) {
    if (o.uid === moving.uid) continue;
    for (const p of portsOf(o)) {
      for (const m of mine) {
        if (opp[m.dir] !== p.dir) continue;
        const dx = p.x - m.x;
        const dy = p.y - m.y;
        const d = Math.hypot(dx, dy);
        if (d < best.d) best = { x: x0 + dx, y: y0 + dy, d };
      }
    }
  }
  return { x: best.x, y: best.y };
}

/** Turn 90° around the centre so the piece stays in place. */
export function rotatePipeItem(it: HmiAhuItem): HmiAhuItem {
  const rot0 = pipeRot(it);
  const s0 = pipeSize(it.art, it.length, rot0);
  const rot = (rot0 + 90) % 360;
  const s1 = pipeSize(it.art, it.length, rot);
  const cx = (it.x ?? 0) + s0.w / 2;
  const cy = (it.y ?? 0) + s0.h / 2;
  return { ...it, rot, x: cx - s1.w / 2, y: cy - s1.h / 2, params: { ...it.params, rot } };
}
