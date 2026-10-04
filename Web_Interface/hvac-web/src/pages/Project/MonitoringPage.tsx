import { useMemo, type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { MnemoView } from '../../mnemo/MnemoView';
import { demoNanoMotion } from '../../mnemo/demoNanoMotion';
import { useLive } from '../../context/LiveDataContext';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { SimulatorPanel } from './SimulatorPanel';

/** Live data exists only for the project that runs on the site PLC. */
export function SiteOnly({ children }: { children: ReactNode }) {
  const { t } = useTranslation();
  const { project, projectId } = useProject();
  if (project?.isActiveOnSite) return <>{children}</>;
  return (
    <div className="card info-card">
      <p>{t('overview.siteInactive')}</p>
      <Link to={`/p/${projectId}/overview`}>{t('projectNav.overview')}</Link>
    </div>
  );
}

export function MonitoringPage() {
  const { t } = useTranslation();
  const { snapshot } = useLive();
  const { hasRole } = useAuth();
  const alarmElements = useMemo(() => new Set((snapshot?.alarms ?? []).map((a) => a.element)), [snapshot]);
  const showSim = snapshot?.mode === 'simulator' && hasRole('Engineer');

  return (
    <div className="page">
      <h1>{t('projectNav.monitoring')}</h1>
      <SiteOnly>
        <div className="monitor-layout">
          <div className="card mn-card-host">
            {snapshot ? (
              <MnemoView mnemonic={demoNanoMotion} values={snapshot.values} alarmElements={alarmElements} />
            ) : (
              <p className="muted">{t('common.loading')}</p>
            )}
          </div>
          {showSim && <SimulatorPanel />}
        </div>
      </SiteOnly>
    </div>
  );
}
