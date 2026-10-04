import type { ModuleKind, ProjectModule } from '../api/client';

/** Numbering rules from plan 8.3 (mirrors the server-side ModuleRules). */
export const RAIL_MAX = 30;
export const WAGON_MAX = 31;
export const HUB_PORTS = 9;
export const HUB_TYPE = 0x70;

export const hex = (n: number | null | undefined) => (n == null ? '' : `0x${n.toString(16).toUpperCase().padStart(2, '0')}`);

export function parseHex(s: string): number | null {
  const v = s.trim();
  if (!v) return null;
  const n = /^0x/i.test(v) ? parseInt(v.slice(2), 16) : parseInt(v, 16);
  return Number.isFinite(n) && n >= 0 && n <= 0xff ? n : NaN;
}

export function kindOf(typeCode: number | null | undefined): ModuleKind | null {
  if (typeCode == null) return null;
  if (typeCode === 0x01) return 'plc';
  if (typeCode === 0x02 || (typeCode >= 0x80 && typeCode <= 0x8f)) return 'noid';
  if (typeCode === 0x10 || typeCode === HUB_TYPE) return 'head';
  if (typeCode === 0x71) return 'display';
  if (typeCode >= 0x90 && typeCode <= 0x9f) return 'plugin';
  return 'wagon';
}

const two = (n: number) => String(n).padStart(2, '0');

export function moduleIdOf(kind: ModuleKind, line: number, rail: number, place: number) {
  if (kind === 'noid' || kind === 'plugin') return '';
  if (kind === 'plc') return '0.00.00';
  return `${line}.${two(rail)}.${two(place)}`;
}

export function systemNameOf(kind: ModuleKind, prefix: string, rail: number, place: number) {
  if (kind === 'plc' || kind === 'noid' || kind === 'plugin') return prefix;
  if (kind === 'head') return `${prefix}-${two(rail)}`;
  return `${prefix}-${two(rail)}.${two(place)}`;
}

/** Channel signal types known to the module firmware (mirrors ModuleRules.SignalModes). */
export const SIGNAL_MODES = ['0-10V', '2-10V', '0-5V', '0-20mA', '4-20mA', 'PT100', 'PT1000', 'NTC10K'] as const;

/** Channels of a module that still need a signal type chosen. */
export function channelsNotSet(m: ProjectModule): number[] {
  const count = m.snapshot?.channelCount ?? 0;
  if (!m.snapshot?.channelModes?.length || !count) return [];
  const set = new Set(m.channels.map((c) => c.channel));
  return Array.from({ length: count }, (_, i) => i + 1).filter((n) => !set.has(n));
}

export const SERIAL_RE = /^[0-9]{2}(0[1-9]|[1-4][0-9]|5[0-3])-[0-9]{4}$/;
export const ARTICLE_RE = /^HC-[A-Z]+[0-9]*(-[A-Z0-9]+)?$/;
