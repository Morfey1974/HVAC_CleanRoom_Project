import { useTranslation } from 'react-i18next';
import { DocumentsPanel } from './DocumentsPanel';

export function DocumentsPage() {
  const { t } = useTranslation();
  return (
    <div className="page">
      <h1>{t('projectNav.documents')}</h1>
      <DocumentsPanel />
    </div>
  );
}
