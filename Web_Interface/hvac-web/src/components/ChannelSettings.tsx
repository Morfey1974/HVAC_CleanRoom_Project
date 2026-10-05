import { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type Equipment, type ModuleChannel, type SensorOutput } from '../api/client';
import { useAuth } from '../context/AuthContext';
import { useProject } from '../context/ProjectContext';
import { outputLabel } from '../lib/sensors';

type Props = { prefix: string; count: number; modes: string[]; value: ModuleChannel[]; disabled?: boolean; onChange: (v: ModuleChannel[]) => void };

/**
 * Per channel: signal type (chosen explicitly, "not set" stays visible) and the project sensor output wired to it.
 * Sensors are project equipment whose library item has outputs.
 */
export function ChannelSettings({ prefix, count, modes, value, disabled, onChange }: Props) {
  const { t } = useTranslation();
  const { token } = useAuth();
  const { projectId } = useProject();
  const [sensors, setSensors] = useState<Equipment[]>([]);

  useEffect(() => {
    if (token)
      api
        .equipment(token, projectId)
        .then((rows) => setSensors(rows.filter((e) => (e.snapshot?.outputs?.length ?? 0) > 0)))
        .catch(() => undefined);
  }, [token, projectId]);

  const get = (n: number): ModuleChannel => value.find((c) => c.channel === n) ?? { channel: n, mode: '' };
  const set = (n: number, patch: Partial<ModuleChannel>) => {
    const next = { ...get(n), ...patch };
    const keep = next.mode || next.equipmentId;
    onChange([...value.filter((c) => c.channel !== n), ...(keep ? [next] : [])].sort((a, b) => a.channel - b.channel));
  };

  const outputsOf = (equipmentId?: string | null): SensorOutput[] => sensors.find((s) => s.id === equipmentId)?.snapshot?.outputs ?? [];

  const pickSensor = (n: number, equipmentId: string) => {
    const outs = outputsOf(equipmentId);
    if (!equipmentId) return set(n, { equipmentId: null, output: null });
    const output = outs.length === 1 ? outs[0].no : null;
    set(n, { equipmentId, output, ...modeFor(n, outs.find((o) => o.no === output)) });
  };

  /** Empty signal type takes the sensor output signal when the module supports it. */
  const modeFor = (n: number, o?: SensorOutput) => (!get(n).mode && o && modes.includes(o.signal) ? { mode: o.signal } : {});

  return (
    <div className="channel-settings">
      {Array.from({ length: count }, (_, i) => i + 1).map((n) => {
        const c = get(n);
        const outs = outputsOf(c.equipmentId);
        const out = outs.find((o) => o.no === c.output);
        const lost = !!c.equipmentId && sensors.length > 0 && !sensors.some((s) => s.id === c.equipmentId);
        const mismatch = out && c.mode && out.signal !== c.mode;
        return (
          <div key={n} className="channel-row">
            <label className={c.mode ? '' : 'field-missing'}>
              <span className="ltr-value">
                {prefix}
                {n}
              </span>
              <select value={c.mode} disabled={disabled} onChange={(e) => set(n, { mode: e.target.value })}>
                <option value="">{t('channels.notSet')}</option>
                {modes.map((m) => (
                  <option key={m} value={m}>
                    {t(`channels.modes.${m}`, { defaultValue: m })}
                  </option>
                ))}
              </select>
            </label>
            <label>
              <span>{t('channels.sensor')}</span>
              <select value={c.equipmentId ?? ''} disabled={disabled} onChange={(e) => pickSensor(n, e.target.value)}>
                <option value="">{t('channels.noSensor')}</option>
                {sensors.map((s) => (
                  <option key={s.id} value={s.id}>
                    {s.tag || s.snapshot?.code}
                  </option>
                ))}
              </select>
            </label>
            {c.equipmentId && (
              <label className={c.output ? '' : 'field-missing'}>
                <span>{t('channels.output')}</span>
                <select
                  value={c.output ?? ''}
                  disabled={disabled}
                  onChange={(e) => {
                    const no = e.target.value ? Number(e.target.value) : null;
                    set(n, { output: no, ...modeFor(n, outs.find((o) => o.no === no)) });
                  }}
                >
                  <option value="">{t('channels.notSet')}</option>
                  {outs.map((o) => (
                    <option key={o.no} value={o.no}>
                      {outputLabel(o, t)}
                    </option>
                  ))}
                </select>
              </label>
            )}
            {lost && <span className="field-hint field-hint--warn">{t('channels.sensorMissing')}</span>}
            {mismatch && (
              <span className="field-hint field-hint--warn">
                {t('channels.signalMismatch', { signal: t(`channels.modes.${out.signal}`, { defaultValue: out.signal }) })}
              </span>
            )}
          </div>
        );
      })}
    </div>
  );
}
