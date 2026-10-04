import type { ReactNode } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { useProject } from '../../context/ProjectContext';
import { ProtectedRoute } from '../../components/ProtectedRoute';
import type { Role } from '../../api/client';
import { OverviewPage } from './OverviewPage';
import { DocumentsPage } from './DocumentsPage';
import { AnalysisPage } from './AnalysisPage';
import { RoomsPage } from './RoomsPage';
import { EquipmentPage } from './EquipmentPage';
import { MonitoringPage } from './MonitoringPage';
import { AlarmsPage } from './AlarmsPage';
import { JournalPage } from './JournalPage';
import { StubPage } from './StubPage';
import { HardwarePage } from './HardwarePage';
import { SchemePage } from './SchemePage';

const stubs: { path: string; minRole: Role }[] = [
  { path: 'signals', minRole: 'Engineer' },
  { path: 'binding', minRole: 'Engineer' },
  { path: 'regulators', minRole: 'Engineer' },
  { path: 'mnemo', minRole: 'Engineer' },
  { path: 'commissioning', minRole: 'Engineer' },
  { path: 'export', minRole: 'Engineer' },
];

const guard = (minRole: Role, el: ReactNode) => <ProtectedRoute minRole={minRole}>{el}</ProtectedRoute>;

export function ProjectRoutes() {
  const { t } = useTranslation();
  const { project, notFound } = useProject();

  if (notFound) return <div className="card error-banner">{t('projects.notFound')}</div>;
  if (!project) return <div className="card muted">{t('common.loading')}</div>;

  return (
    <Routes>
      <Route index element={<Navigate to="overview" replace />} />
      <Route path="overview" element={<OverviewPage />} />
      <Route path="documents" element={<DocumentsPage />} />
      <Route path="pid" element={guard('Engineer', <AnalysisPage kind="pid" />)} />
      <Route path="aoi" element={guard('Engineer', <AnalysisPage kind="aoi" />)} />
      <Route path="rooms" element={<RoomsPage />} />
      <Route path="equipment" element={<EquipmentPage />} />
      <Route path="hardware" element={guard('Engineer', <HardwarePage />)} />
      <Route path="scheme" element={guard('Engineer', <SchemePage />)} />
      <Route path="monitoring" element={<MonitoringPage />} />
      <Route path="alarms" element={<AlarmsPage />} />
      <Route path="journal" element={guard('Engineer', <JournalPage />)} />
      {stubs.map((s) => (
        <Route key={s.path} path={s.path} element={guard(s.minRole, <StubPage section={s.path} />)} />
      ))}
      <Route path="*" element={<Navigate to="overview" replace />} />
    </Routes>
  );
}
