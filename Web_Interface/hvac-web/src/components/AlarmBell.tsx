import { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Link } from 'react-router-dom';
import { useLive } from '../context/LiveDataContext';

/** Silent red bell with active alarm count; click shows the active list. */
export function AlarmBell() {
  const { t } = useTranslation();
  const { snapshot } = useLive();
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const alarms = snapshot?.alarms ?? [];

  useEffect(() => {
    if (!open) return;
    const onDoc = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, [open]);

  return (
    <div className="alarm-bell-wrap" ref={ref}>
      <button
        type="button"
        className={alarms.length ? 'alarm-bell alarm-bell--active' : 'alarm-bell'}
        onClick={() => setOpen((o) => !o)}
        title={t('header.alarms')}
        aria-label={t('header.alarms')}
      >
        <svg viewBox="0 0 24 24" width="22" height="22" aria-hidden="true">
          <path
            fill="currentColor"
            d="M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1l-2-2Z"
          />
        </svg>
        {alarms.length > 0 && <span className="alarm-bell__count">{alarms.length}</span>}
      </button>
      {open && (
        <div className="alarm-popover card">
          <strong>{t('header.alarms')}</strong>
          {alarms.length === 0 ? (
            <p className="muted">{t('header.noAlarms')}</p>
          ) : (
            <ul className="alarm-popover__list">
              {alarms.map((a) => (
                <li key={a.code}>{t(`alarms.codes.${a.code}`, { defaultValue: a.code })}</li>
              ))}
            </ul>
          )}
          <Link to="/active/alarms" onClick={() => setOpen(false)}>
            {t('header.allAlarms')}
          </Link>
        </div>
      )}
    </div>
  );
}
