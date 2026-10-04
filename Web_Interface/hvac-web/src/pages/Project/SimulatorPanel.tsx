import { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { useLive } from '../../context/LiveDataContext';

const sliders: { key: 'fanSp' | 'stages' | 'vrfCap' | 'outT' | 'dirt' | 'sp'; min: number; max: number; step?: number; unit: string }[] = [
  { key: 'fanSp', min: 0, max: 100, unit: '%' },
  { key: 'stages', min: 0, max: 3, unit: '' },
  { key: 'vrfCap', min: 0, max: 100, unit: '%' },
  { key: 'outT', min: -5, max: 40, unit: '°' },
  { key: 'dirt', min: 0, max: 100, unit: '%' },
  { key: 'sp', min: 18, max: 26, step: 0.5, unit: '°' },
];
const faults = ['fVsd', 'fBelt', 'fTt', 'fVrf'] as const;

export function SimulatorPanel() {
  const { t } = useTranslation();
  const { simInputs, setSimInput } = useLive();
  const set = (k: string, v: number) => void setSimInput(k, v);

  return (
    <aside className="card sim-panel">
      <h3>{t('monitoring.simTitle')}</h3>
      <p className="muted sim-panel__hint">{t('monitoring.simHint')}</p>
      <label className="checkbox-row">
        <input type="checkbox" checked={(simInputs.run ?? 0) > 0} onChange={(e) => set('run', e.target.checked ? 1 : 0)} />
        {t('monitoring.sim.run')}
      </label>
      {sliders.slice(0, 2).map((s) => (
        <Slider {...s} key={s.key} value={simInputs[s.key] ?? 0} onChange={(v) => set(s.key, v)} label={t(`monitoring.sim.${s.key}`)} />
      ))}
      <label className="sim-panel__field">
        {t('monitoring.sim.vrfMode')}
        <select value={simInputs.vrfMode ?? 0} onChange={(e) => set('vrfMode', Number(e.target.value))}>
          <option value={0}>{t('monitoring.vrfModes.off')}</option>
          <option value={1}>{t('monitoring.vrfModes.cool')}</option>
          <option value={2}>{t('monitoring.vrfModes.heat')}</option>
        </select>
      </label>
      {sliders.slice(2).map((s) => (
        <Slider {...s} key={s.key} value={simInputs[s.key] ?? 0} onChange={(v) => set(s.key, v)} label={t(`monitoring.sim.${s.key}`)} />
      ))}
      <h3>{t('monitoring.faults')}</h3>
      {faults.map((f) => (
        <label key={f} className="checkbox-row">
          <input type="checkbox" checked={(simInputs[f] ?? 0) > 0} onChange={(e) => set(f, e.target.checked ? 1 : 0)} />
          {t(`monitoring.sim.${f}`)}
        </label>
      ))}
    </aside>
  );
}

/** Sends the value on release only: every sent change is written to the action log. */
function Slider(props: { label: string; value: number; min: number; max: number; step?: number; unit: string; onChange: (v: number) => void }) {
  const [draft, setDraft] = useState<number | null>(null);
  const shown = draft ?? props.value;
  const commit = () => {
    if (draft !== null && draft !== props.value) props.onChange(draft);
    setDraft(null);
  };
  return (
    <label className="sim-panel__field">
      <span className="sim-panel__row">
        <span>{props.label}</span>
        <span className="ltr-value">
          {shown}
          {props.unit}
        </span>
      </span>
      <input
        type="range"
        min={props.min}
        max={props.max}
        step={props.step ?? 1}
        value={shown}
        onChange={(e) => setDraft(Number(e.target.value))}
        onPointerUp={commit}
        onKeyUp={commit}
        onBlur={commit}
      />
    </label>
  );
}
