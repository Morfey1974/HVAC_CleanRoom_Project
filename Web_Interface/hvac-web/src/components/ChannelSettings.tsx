import { useTranslation } from 'react-i18next';
import type { ModuleChannel } from '../api/client';

type Props = { prefix: string; count: number; modes: string[]; value: ModuleChannel[]; disabled?: boolean; onChange: (v: ModuleChannel[]) => void };

/** Signal type of every channel, chosen explicitly: "not set" stays visible until each channel has one. */
export function ChannelSettings({ prefix, count, modes, value, disabled, onChange }: Props) {
  const { t } = useTranslation();
  const modeOf = (n: number) => value.find((c) => c.channel === n)?.mode ?? '';
  const set = (n: number, mode: string) =>
    onChange([...value.filter((c) => c.channel !== n), ...(mode ? [{ channel: n, mode }] : [])].sort((a, b) => a.channel - b.channel));

  return (
    <div className="channel-settings">
      {Array.from({ length: count }, (_, i) => i + 1).map((n) => (
        <label key={n} className={modeOf(n) ? '' : 'field-missing'}>
          <span className="ltr-value">
            {prefix}
            {n}
          </span>
          <select value={modeOf(n)} disabled={disabled} onChange={(e) => set(n, e.target.value)}>
            <option value="">{t('channels.notSet')}</option>
            {modes.map((m) => (
              <option key={m} value={m}>
                {t(`channels.modes.${m}`, { defaultValue: m })}
              </option>
            ))}
          </select>
        </label>
      ))}
    </div>
  );
}
