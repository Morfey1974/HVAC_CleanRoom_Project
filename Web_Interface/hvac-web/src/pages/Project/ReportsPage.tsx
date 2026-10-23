import { useTranslation } from 'react-i18next';
import { RECORDS_STRICT } from '../../lib/recordsMode';

const blocks = ['period', 'journal', 'trends', 'alarms'] as const;

export function ReportsPage() {
  const { t } = useTranslation();
  return (
    <div className="page">
      <h1>{t('projectNav.reports')}</h1>
      <p className="muted">{t('reports.inDevelopment')}</p>
      {!RECORDS_STRICT && <p className="muted">{t('reports.strictOff')}</p>}
      <div className="reports-grid">
        {blocks.map((k) => (
          <section key={k} className="card">
            <h2>{t(`reports.blocks.${k}`)}</h2>
            <p className="muted">{t(`reports.hints.${k}`)}</p>
            {k === 'period' && (
              <div className="reports-period">
                <label>
                  {t('reports.from')}
                  <input type="date" disabled />
                </label>
                <label>
                  {t('reports.to')}
                  <input type="date" disabled />
                </label>
              </div>
            )}
            <button type="button" className="btn" disabled>
              {t('reports.export')}
            </button>
          </section>
        ))}
      </div>
    </div>
  );
}
