import type { ReactNode } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { useProject } from '../../context/ProjectContext';
import { ProtectedRoute } from '../../components/ProtectedRoute';
import { OverviewPage } from './OverviewPage';
import { DocumentsPage } from './DocumentsPage';
import { AnalysisPage } from './AnalysisPage';
import { RoomsPage } from './RoomsPage';
import { EquipmentPage } from './EquipmentPage';
import { MonitoringPage } from './MonitoringPage';
import { AlarmsPage } from './AlarmsPage';
import { JournalPage } from './JournalPage';
import { ReportsPage } from './ReportsPage';
import { StubPage } from './StubPage';
import { HardwarePage } from './HardwarePage';
import { SchemePage } from './SchemePage';
import { ExportPage } from './ExportPage';
import { HmiScreensPage } from './HmiScreensPage';
import { HmiPanelPreview } from './HmiPanelPreview';

const stubs: { path: string; perm: string }[] = [
  { path: 'signals', perm: 'signals.edit' },
  { path: 'binding', perm: 'binding.edit' },
  { path: 'regulators', perm: 'regulators.edit' },
  { path: 'commissioning', perm: 'commissioning.edit' },
];

const guard = (perm: string, el: ReactNode) => <ProtectedRoute perm={perm}>{el}</ProtectedRoute>;

export function ProjectRoutes() {
  const { t } = useTranslation();
  const { project, notFound } = useProject();

  if (notFound) return <div className="card error-banner">{t('projects.notFound')}</div>;
  if (!project) return <div className="card muted">{t('common.loading')}</div>;

  return (
    <Routes>
      <Route index element={<Navigate to="overview" replace />} />
      <Route path="overview" element={guard('projects.view', <OverviewPage />)} />
      <Route path="documents" element={guard('documents.view', <DocumentsPage />)} />
      <Route path="pid" element={guard('analysis.edit', <AnalysisPage kind="pid" />)} />
      <Route path="aoi" element={guard('analysis.edit', <AnalysisPage kind="aoi" />)} />
      <Route path="rooms" element={guard('rooms.view', <RoomsPage />)} />
      <Route path="equipment" element={guard('equipment.view', <EquipmentPage />)} />
      <Route path="hardware" element={guard('hardware.view', <HardwarePage />)} />
      <Route path="scheme" element={guard('scheme.view', <SchemePage />)} />
      <Route path="mnemo" element={guard('hmi.screens.view', <HmiScreensPage />)} />
      <Route path="hmi-preview" element={guard('hmi.screens.view', <HmiPanelPreview />)} />
      <Route path="hmi-preview/:screenId" element={guard('hmi.screens.view', <HmiPanelPreview />)} />
      <Route path="monitoring" element={guard('monitoring.view', <MonitoringPage />)} />
      <Route path="alarms" element={guard('alarms.view', <AlarmsPage />)} />
      <Route path="journal" element={guard('audit.view', <JournalPage />)} />
      <Route path="reports" element={guard('reports.view', <ReportsPage />)} />
      <Route path="export" element={guard('plc.config.view', <ExportPage />)} />
      {stubs.map((s) => (
        <Route key={s.path} path={s.path} element={guard(s.perm, <StubPage section={s.path} />)} />
      ))}
      <Route path="*" element={<Navigate to="overview" replace />} />
    </Routes>
  );
}
