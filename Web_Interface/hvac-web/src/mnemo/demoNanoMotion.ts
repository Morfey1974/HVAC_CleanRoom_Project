import type { ElementInstance, Mnemonic } from './types';

/** Demo object from P&ID NanoMotion 26000017-HVAC-W: package AHU 600 cfm + ISO 7 room. */
const flowAir = { speed: 'AHU1.FLOW.PCT', active: 'AHU1.FS.FLOW' };
const el = (id: string, type: string, x: number, y: number, rest: Partial<ElementInstance> = {}): ElementInstance => ({
  id,
  type,
  x,
  y,
  ...rest,
});

const ffus: ElementInstance[] = Array.from({ length: 6 }, (_, i) => [
  el(`ffu${i}`, 'ffu', 440 + i * 110, 498, {
    bind: { running: 'ROOM1.FFU.RUN' },
    card: { title: { i18n: 'mnemo.el.ffu' }, channel: 'DO3' },
  }),
  el(`ffuFlow${i}`, 'flow', 0, 0, {
    props: { d: `M${440 + i * 110} 530 V585`, color: '#4ade80', width: 4 },
    bind: { active: 'ROOM1.FFU.RUN' },
  }),
]).flat();

export const demoNanoMotion: Mnemonic = {
  id: 'nanomotion-ahu1',
  name: { ru: 'Приточная установка и комната', en: 'Supply unit and room', he: 'יחידת אספקה וחדר' },
  width: 1400,
  height: 800,
  elements: [
    el('roof', 'band', 0, 420),
    el('roofLbl', 'label', 20, 414, { props: { text: { i18n: 'mnemo.roof' }, size: 11, color: 'var(--mn-muted)' } }),

    el('vrfPipes', 'line', 0, 0, { props: { d: 'M330 120 V170 M360 120 V170', color: '#fb923c', width: 3 } }),
    el('vrf', 'vrf', 300, 20, {
      bind: { on: 'AHU1.VRF.ON' },
      alarm: 'vrf',
      card: { title: { i18n: 'mnemo.el.vrf' }, channel: 'DO1 / DI5', valueTag: 'AHU1.VRF.CAP', unit: '%', decimals: 0 },
    }),

    el('ahu', 'box3d', 120, 170, { props: { w: 860, h: 170, depth: 30, walls: [165, 270, 395, 660], title: { i18n: 'mnemo.ahu' } } }),
    el('flowFresh', 'flow', 0, 0, { props: { d: 'M10 262 H130', color: '#fb923c' }, bind: flowAir }),
    el('flowAhu', 'flow', 0, 0, { props: { d: 'M140 262 H970', opacity: 0.4 }, bind: flowAir }),

    el('f1', 'filter', 150, 195, {
      props: { label: '12%' },
      bind: { dirt: 'AHU1.FLT.DIRT' },
      alarm: 'filters',
      card: { title: { i18n: 'mnemo.el.f1' }, valueTag: 'AHU1.FLT1.DP', unit: 'Pa', decimals: 0 },
    }),
    el('f2', 'filter', 215, 195, {
      props: { label: '30%' },
      bind: { dirt: 'AHU1.FLT.DIRT' },
      alarm: 'filters',
      card: { title: { i18n: 'mnemo.el.f2' }, valueTag: 'AHU1.FLT1.DP', unit: 'Pa', decimals: 0 },
    }),
    el('pi1Line', 'line', 0, 0, { props: { d: 'M160 170 V110 H245 V170' } }),
    el('pi1', 'gauge', 202, 100, {
      props: { max: 300 },
      bind: { value: 'AHU1.FLT1.DP' },
      card: { title: { i18n: 'mnemo.el.pi1' }, channel: 'local', valueTag: 'AHU1.FLT1.DP', unit: 'Pa', decimals: 0 },
    }),

    el('coil', 'coil', 310, 195, {
      bind: { on: 'AHU1.VRF.ON', mode: 'AHU1.VRF.MODE', cap: 'AHU1.VRF.CAP' },
      card: { title: { i18n: 'mnemo.el.coil' }, valueTag: 'AHU1.VRF.CAP', unit: '%', decimals: 0 },
    }),
    el('heater', 'heater', 410, 205, {
      props: { stages: 3, gap: 50, label: '3×1.2 kW' },
      bind: { on: 'AHU1.HTR.STAGES' },
      card: { title: { i18n: 'mnemo.el.heater' }, channel: 'RL1…RL3', valueTag: 'AHU1.HTR.STAGES', decimals: 0 },
    }),
    el('ts', 'tag', 452, 365, {
      props: { code: 'TS', stem: -30 },
      alarm: 'ts',
      card: { title: { i18n: 'mnemo.el.ts' }, channel: 'DI2', valueTag: 'AHU1.TS.TRIP', decimals: 0 },
    }),

    el('fan', 'fan', 620, 255, {
      bind: { speed: 'AHU1.FAN.SPD', running: 'AHU1.FAN.RUN' },
      card: { title: { i18n: 'mnemo.el.fan' }, channel: 'DO2', valueTag: 'AHU1.FAN.SPD', unit: '%', decimals: 0 },
    }),
    el('vsdLink', 'line', 0, 0, { props: { d: 'M725 360 V272', color: '#ef4444', dash: '4 3' } }),
    el('vsd', 'vsd', 690, 360, {
      bind: { hz: 'AHU1.VSD.HZ', fault: 'AHU1.VSD.FAULT' },
      alarm: 'vsd',
      card: { title: { i18n: 'mnemo.el.vsd' }, channel: 'AO1 / DI3', valueTag: 'AHU1.VSD.HZ', unit: 'Hz' },
    }),
    el('fs', 'tag', 770, 365, {
      props: { code: 'FS', stem: -45 },
      alarm: 'fs',
      card: { title: { i18n: 'mnemo.el.fs' }, channel: 'DI1', valueTag: 'AHU1.FS.FLOW', decimals: 0 },
    }),

    el('f3', 'filter', 850, 195, {
      props: { label: '95%', w: 55 },
      bind: { dirt: 'AHU1.FLT.DIRT' },
      alarm: 'filters',
      card: { title: { i18n: 'mnemo.el.f3' }, valueTag: 'AHU1.FLT2.DP', unit: 'Pa', decimals: 0 },
    }),
    el('pi2Line', 'line', 0, 0, { props: { d: 'M830 170 V110 H925 V170' } }),
    el('pi2', 'gauge', 877, 100, {
      props: { max: 450 },
      bind: { value: 'AHU1.FLT2.DP' },
      card: { title: { i18n: 'mnemo.el.pi2' }, channel: 'local', valueTag: 'AHU1.FLT2.DP', unit: 'Pa', decimals: 0 },
    }),

    el('freshLbl', 'label', 10, 240, { props: { text: { i18n: 'mnemo.fresh' }, size: 11, color: '#fb923c' } }),
    el('outT', 'value', 14, 290, { props: { unit: '°C', anchor: 'start' }, bind: { value: 'OUT.T' } }),

    el('duct1', 'duct', 980, 240, { props: { w: 210, h: 44 } }),
    el('duct2', 'duct', 1150, 240, { props: { w: 44, h: 235 } }),
    el('duct3', 'duct', 380, 455, { props: { w: 814, h: 30 } }),
    el('flowSupply', 'flow', 0, 0, { props: { d: 'M985 262 H1172 V470 H400' }, bind: flowAir }),
    el('dpt', 'tag', 1040, 190, {
      props: { code: 'DPT', stem: 33, unit: 'Pa', decimals: 0 },
      bind: { value: 'AHU1.SUP.P' },
      card: { title: { i18n: 'mnemo.el.dpt' }, channel: 'AI2', valueTag: 'AHU1.SUP.P', unit: 'Pa', decimals: 0 },
    }),
    el('tt1', 'tag', 1110, 190, {
      props: { code: 'TT', stem: 33, unit: '°C' },
      bind: { value: 'AHU1.SUP.T' },
      card: { title: { i18n: 'mnemo.el.tt1' }, channel: 'AI1', valueTag: 'AHU1.SUP.T', unit: '°C' },
    }),
    el('cfm', 'value', 1080, 305, { props: { unit: 'cfm', decimals: 0 }, bind: { value: 'AHU1.SUP.CFM' } }),

    el('room', 'room', 380, 490, {
      props: {
        w: 770,
        h: 270,
        title: { ru: 'Комната расширения', en: 'Extension room', he: 'חדר הרחבה' },
        info: 'A 35 m² · h 2.7 m · V 95 m³ · ISO 7 · ACH 50',
      },
      bind: { t: 'ROOM1.T', sp: 'ROOM1.SP' },
      alarm: 'room',
      card: { title: { ru: 'Комната расширения, ISO 7', en: 'Extension room, ISO 7', he: 'חדר הרחבה, ISO 7' }, valueTag: 'ROOM1.T', unit: '°C' },
    }),
    ...ffus,
    el('tt2', 'tag', 1100, 560, {
      props: { code: 'TT' },
      alarm: 'tt2',
      card: { title: { i18n: 'mnemo.el.tt2' }, channel: 'AI3', valueTag: 'ROOM1.T', unit: '°C' },
    }),
    el('relief', 'relief', 1140, 700, {
      bind: { open: 'AHU1.FS.FLOW' },
      card: { title: { i18n: 'mnemo.el.relief' } },
    }),
    el('flowRelief', 'flow', 0, 0, { props: { d: 'M1165 725 H1230', color: '#4ade80', width: 4 }, bind: flowAir }),
    el('ac', 'acUnit', 1220, 500, { props: { caption: { i18n: 'mnemo.acExisting' } }, card: { title: { i18n: 'mnemo.el.ac' } } }),
    el('flowAc', 'flow', 0, 0, { props: { d: 'M1220 520 H1160', color: '#a3a36b', width: 4 } }),
  ],
};
