import type { HmiScreen, LocalizedText } from '../api/client';
import type { HmiState } from './arts';

/** EastRising 10.1" / LT7683. */
export const HMI_PANEL_W = 1024;
export const HMI_PANEL_H = 600;

export type HmiPreviewDemo = 'live' | HmiState;

export type HmiPreviewPayload = { screen: HmiScreen; demo: HmiPreviewDemo };

export const hmiPreviewKey = (projectId: string, screenId: string) => `hmi-preview:${projectId}:${screenId}`;

export type HmiPackScreen = { id: string; kind: HmiScreen['kind']; name: LocalizedText; png: string; screen: HmiScreen };
export type HmiPack = { projectId: string; at: string; screens: HmiPackScreen[] };
export const hmiPackKey = (projectId: string) => `hmi-pack:${projectId}`;

export function saveHmiPack(pack: HmiPack) {
  sessionStorage.setItem(hmiPackKey(pack.projectId), JSON.stringify(pack));
}

export function readHmiPack(projectId: string): HmiPack | null {
  try {
    const raw = sessionStorage.getItem(hmiPackKey(projectId));
    if (!raw) return null;
    const p = JSON.parse(raw) as HmiPack;
    return p?.projectId === projectId && Array.isArray(p.screens) ? p : null;
  } catch {
    return null;
  }
}

function copyAuthTo(w: Window) {
  try {
    const auth = sessionStorage.getItem('hvac_auth');
    if (auth) w.sessionStorage.setItem('hvac_auth', auth);
  } catch {
    /* popup cannot write */
  }
}

function popupFeatures() {
  const al = window.screen.availLeft || 0;
  const at = window.screen.availTop || 0;
  const left = al + Math.round((window.screen.availWidth - HMI_PANEL_W) / 2);
  const top = at + Math.round((window.screen.availHeight - HMI_PANEL_H) / 2);
  return [
    'popup=yes',
    `width=${HMI_PANEL_W}`,
    `height=${HMI_PANEL_H}`,
    `left=${left}`,
    `top=${top}`,
    'menubar=no',
    'toolbar=no',
    'location=no',
    'status=no',
    'scrollbars=no',
    'resizable=yes',
  ].join(',');
}

function openPanelWindow(url: string, name: string, extra?: (w: Window) => void) {
  const w = window.open(url, name, popupFeatures());
  if (w) {
    copyAuthTo(w);
    try {
      extra?.(w);
      centerHmiPreviewWindow(w);
    } catch {
      /* some browsers block writing into the new window */
    }
  } else {
    window.open(url, '_blank');
  }
}

export function openHmiPack(projectId: string) {
  openPanelWindow(`/p/${projectId}/hmi-preview?pack=1`, `hmi-pack-${projectId}`, (w) => {
    const raw = sessionStorage.getItem(hmiPackKey(projectId));
    if (raw) w.sessionStorage.setItem(hmiPackKey(projectId), raw);
  });
}

export function centerHmiPreviewWindow(target: Window = window) {
  const dx = target.outerWidth - target.innerWidth;
  const dy = target.outerHeight - target.innerHeight;
  if (dx >= 0 && dy >= 0 && dx < 80 && dy < 120) {
    target.resizeTo(HMI_PANEL_W + dx, HMI_PANEL_H + dy);
  }
  const al = target.screen.availLeft || 0;
  const at = target.screen.availTop || 0;
  const left = al + Math.round((target.screen.availWidth - target.outerWidth) / 2);
  const top = at + Math.round((target.screen.availHeight - target.outerHeight) / 2);
  target.moveTo(left, top);
}

export function openHmiPreview(projectId: string, screen: HmiScreen, demo: HmiPreviewDemo = 'live') {
  sessionStorage.setItem(hmiPreviewKey(projectId, screen.id), JSON.stringify({ screen, demo } satisfies HmiPreviewPayload));
  const q = demo !== 'live' ? `?demo=${demo}` : '';
  openPanelWindow(`/p/${projectId}/hmi-preview/${screen.id}${q}`, `hmi-preview-${screen.id}`, (w) => {
    w.sessionStorage.setItem(hmiPreviewKey(projectId, screen.id), sessionStorage.getItem(hmiPreviewKey(projectId, screen.id)) ?? '');
  });
}
