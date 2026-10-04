import { NavLink, Outlet, useMatch } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { useAuth } from '../context/AuthContext';
import { useLive } from '../context/LiveDataContext';
import { ProjectProvider, useProject } from '../context/ProjectContext';
import { changeLanguage, isLangCode, languages } from '../i18n';
import { pickName } from '../lib/localized';
import { AlarmBell } from './AlarmBell';
import { BidiText } from './BidiText';
import type { Role } from '../api/client';

type NavItem = { to: string; key: string; minRole: Role };

const globalNav: NavItem[] = [
  { to: '/projects', key: 'projects', minRole: 'Viewer' },
  { to: '/library', key: 'library', minRole: 'Viewer' },
  { to: '/system', key: 'system', minRole: 'Viewer' },
];

export const projectNavGroups: { group: string; items: NavItem[] }[] = [
  {
    group: 'project',
    items: [
      { to: 'overview', key: 'overview', minRole: 'Viewer' },
      { to: 'documents', key: 'documents', minRole: 'Viewer' },
    ],
  },
  {
    group: 'analysis',
    items: [
      { to: 'pid', key: 'pid', minRole: 'Engineer' },
      { to: 'aoi', key: 'aoi', minRole: 'Engineer' },
    ],
  },
  {
    group: 'config',
    items: [
      { to: 'rooms', key: 'rooms', minRole: 'Viewer' },
      { to: 'equipment', key: 'equipment', minRole: 'Viewer' },
      { to: 'signals', key: 'signals', minRole: 'Engineer' },
      { to: 'hardware', key: 'hardware', minRole: 'Engineer' },
      { to: 'scheme', key: 'scheme', minRole: 'Engineer' },
      { to: 'binding', key: 'binding', minRole: 'Engineer' },
      { to: 'regulators', key: 'regulators', minRole: 'Engineer' },
      { to: 'mnemo', key: 'mnemo', minRole: 'Engineer' },
    ],
  },
  {
    group: 'operation',
    items: [
      { to: 'monitoring', key: 'monitoring', minRole: 'Viewer' },
      { to: 'alarms', key: 'alarms', minRole: 'Viewer' },
      { to: 'commissioning', key: 'commissioning', minRole: 'Engineer' },
    ],
  },
  {
    group: 'records',
    items: [
      { to: 'journal', key: 'journal', minRole: 'Engineer' },
      { to: 'export', key: 'export', minRole: 'Engineer' },
    ],
  },
];

const linkClass = ({ isActive }: { isActive: boolean }) => (isActive ? 'nav-link active' : 'nav-link');

function PlcStatus() {
  const { t } = useTranslation();
  const { snapshot, linkUp } = useLive();
  const online = linkUp && !!snapshot?.connected;
  return (
    <span className={online ? 'plc-status plc-status--on' : 'plc-status plc-status--off'}>
      <span className="plc-status__dot" />
      {online ? t('header.plcOnline') : t('header.plcOffline')}
      {online && snapshot?.mode === 'simulator' && <span className="plc-status__mode">{t('header.simulator')}</span>}
    </span>
  );
}

function GlobalNav() {
  const { t } = useTranslation();
  const { hasRole } = useAuth();
  return (
    <nav className="app-nav">
      {globalNav
        .filter((i) => hasRole(i.minRole))
        .map((i) => (
          <NavLink key={i.to} to={i.to} className={linkClass}>
            {t(`nav.${i.key}`)}
          </NavLink>
        ))}
    </nav>
  );
}

function ProjectNav() {
  const { t, i18n } = useTranslation();
  const { hasRole } = useAuth();
  const { projectId, project } = useProject();
  return (
    <nav className="app-nav app-nav--project">
      <NavLink to="/projects" end className="nav-back">
        ← {t('nav.projects')}
      </NavLink>
      <div className="nav-project">
        <BidiText>{project ? pickName(project.name, i18n.language).value : '…'}</BidiText>
        {project?.isActiveOnSite && <span className="badge badge--site">{t('projects.activeOnSite')}</span>}
      </div>
      {projectNavGroups.map((g) => {
        const items = g.items.filter((i) => hasRole(i.minRole));
        if (!items.length) return null;
        return (
          <div key={g.group} className="nav-group">
            <div className="nav-group__title">{t(`projectNav.groups.${g.group}`)}</div>
            {items.map((i) => (
              <NavLink key={i.to} to={`/p/${projectId}/${i.to}`} className={linkClass}>
                {t(`projectNav.${i.key}`)}
              </NavLink>
            ))}
          </div>
        );
      })}
    </nav>
  );
}

function Shell({ inProject }: { inProject: boolean }) {
  const { t, i18n } = useTranslation();
  const { user, logout } = useAuth();

  return (
    <div className="app-shell">
      <header className="app-header">
        <div className="header-brand">{t('appTitle')}</div>
        <PlcStatus />
        <div className="header-actions">
          <AlarmBell />
          <select
            className="lang-select"
            value={i18n.language}
            onChange={(e) => isLangCode(e.target.value) && changeLanguage(e.target.value)}
            aria-label={t('language')}
          >
            {languages.map((l) => (
              <option key={l.code} value={l.code}>
                {l.label}
              </option>
            ))}
          </select>
          <span className="user-label">
            {user?.fullName || user?.login} · {user && t(`roles.${user.role}`)}
          </span>
          <button type="button" className="btn btn-ghost" onClick={logout}>
            {t('header.logout')}
          </button>
        </div>
      </header>
      <div className="app-body">
        {inProject ? <ProjectNav /> : <GlobalNav />}
        <main className="app-main">
          <Outlet />
        </main>
      </div>
    </div>
  );
}

export function Layout() {
  const match = useMatch('/p/:projectId/*');
  const projectId = match?.params.projectId;
  return projectId ? (
    <ProjectProvider projectId={projectId}>
      <Shell inProject />
    </ProjectProvider>
  ) : (
    <Shell inProject={false} />
  );
}
