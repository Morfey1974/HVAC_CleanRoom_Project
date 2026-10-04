import { useTranslation } from 'react-i18next';
import { useProject } from '../../context/ProjectContext';
import { AuditTable } from '../../components/AuditTable';

export function JournalPage() {
  const { t } = useTranslation();
  const { projectId } = useProject();
  return (
    <div className="page">
      <h1>{t('projectNav.journal')}</h1>
      <AuditTable projectId={projectId} />
    </div>
  );
}
