import { useTranslation } from 'react-i18next';
import type { DocumentKind } from '../../api/client';
import { DocumentsPanel } from './DocumentsPanel';

/** P&ID / AOI analysis. For now: source files; automatic parsing comes with the AI stage. */
export function AnalysisPage({ kind }: { kind: Extract<DocumentKind, 'pid' | 'aoi'> }) {
  const { t } = useTranslation();
  return (
    <div className="page">
      <h1>{t(`projectNav.${kind}`)}</h1>
      <div className="card info-card">
        <p>{t(`analysis.${kind}.what`)}</p>
        <ol className="analysis-steps">
          {(t(`analysis.${kind}.steps`, { returnObjects: true }) as string[]).map((s) => (
            <li key={s}>{s}</li>
          ))}
        </ol>
        <p className="muted">{t('analysis.notYet')}</p>
      </div>
      <h2>{t('analysis.sources')}</h2>
      <DocumentsPanel kind={kind} />
    </div>
  );
}
