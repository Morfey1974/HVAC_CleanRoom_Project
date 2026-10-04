import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type AlarmEvent } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useLive } from '../../context/LiveDataContext';
import { formatDateTime } from '../../lib/localized';
import { SiteOnly } from './MonitoringPage';

export function AlarmsPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { alarmsVersion } = useLive();
  const [activeOnly, setActiveOnly] = useState(true);
  const [rows, setRows] = useState<AlarmEvent[]>([]);
  const [error, setError] = useState('');

  const load = useCallback(() => {
    if (!token) return;
    api.alarms(token, activeOnly).then(setRows).catch((e: Error) => setError(e.message));
  }, [token, activeOnly]);

  useEffect(load, [load, alarmsVersion]);

  const ack = async (id: number) => {
    if (!token) return;
    await api.ackAlarm(token, id);
    load();
  };

  return (
    <div className="page">
      <h1>{t('projectNav.alarms')}</h1>
      <SiteOnly>
      <div className="card">
        <div className="tabs tabs--small">
          <button type="button" className={activeOnly ? 'tab active' : 'tab'} onClick={() => setActiveOnly(true)}>
            {t('alarms.active')}
          </button>
          <button type="button" className={!activeOnly ? 'tab active' : 'tab'} onClick={() => setActiveOnly(false)}>
            {t('alarms.history')}
          </button>
        </div>
        {error && <div className="error-banner">{error}</div>}
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>{t('alarms.text')}</th>
                <th>{t('alarms.started')}</th>
                <th>{t('alarms.cleared')}</th>
                <th>{t('alarms.ackBy')}</th>
                <th />
              </tr>
            </thead>
            <tbody>
              {rows.length === 0 && (
                <tr>
                  <td colSpan={5} className="muted">
                    {t('common.noData')}
                  </td>
                </tr>
              )}
              {rows.map((a) => (
                <tr key={a.id} className={a.clearedAt ? '' : 'row-alarm'}>
                  <td>{t(`alarms.codes.${a.code}`, { defaultValue: a.code })}</td>
                  <td className="ltr-value">{formatDateTime(a.startedAt, i18n.language)}</td>
                  <td className="ltr-value">{a.clearedAt ? formatDateTime(a.clearedAt, i18n.language) : t('alarms.stillActive')}</td>
                  <td>{a.ackBy ? `${a.ackBy} · ${formatDateTime(a.ackAt, i18n.language)}` : ''}</td>
                  <td>
                    {!a.ackAt && hasRole('Operator') && (
                      <button type="button" className="btn btn-small btn-primary" onClick={() => ack(a.id)}>
                        {t('alarms.ack')}
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
      </SiteOnly>
    </div>
  );
}
