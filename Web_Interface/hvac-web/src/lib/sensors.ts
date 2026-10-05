import type { TFunction } from 'i18next';
import type { Equipment, ProjectModule, SensorOutput } from '../api/client';

/** "1 · Humidity 0–100 %RH" */
export function outputLabel(o: SensorOutput, t: TFunction) {
  return `${o.no} · ${t(`quantities.${o.quantity}`)} ${o.min}–${o.max} ${o.unit}`.trim();
}

/** Live value key of a module channel; same format as the server (ChannelKey). */
export const channelKey = (moduleId: string, channel: number) => `${moduleId}.CH${channel}`;

export type BoundOutput = { equipment: Equipment; output: SensorOutput; module: ProjectModule; channel: number; key: string };

/** Every sensor output wired to a placed module channel. */
export function boundOutputs(modules: ProjectModule[], equipment: Equipment[]): BoundOutput[] {
  const byId = new Map(equipment.map((e) => [e.id, e]));
  const res: BoundOutput[] = [];
  for (const m of modules) {
    if (!m.moduleId) continue;
    for (const c of m.channels ?? []) {
      const e = c.equipmentId ? byId.get(c.equipmentId) : undefined;
      const o = e?.snapshot?.outputs?.find((x) => x.no === c.output);
      if (e && o) res.push({ equipment: e, output: o, module: m, channel: c.channel, key: channelKey(m.moduleId, c.channel) });
    }
  }
  return res;
}
