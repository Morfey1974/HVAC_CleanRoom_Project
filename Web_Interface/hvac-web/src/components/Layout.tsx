import { NavLink, Outlet, useMatch } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { useAuth } from '../context/AuthContext';
import { useLive } from '../context/LiveDataContext';
import { ProjectProvider, useProject } from '../context/ProjectContext';
import { changeLanguage, isLangCode, languages } from '../i18n';
import { pickName } from '../lib/localized';
import { AlarmBell } from './AlarmBell';
import { BidiText } from './BidiText';
import logoDcm from '../assets/logo-dcm.png';

type NavItem = { to: string; key: string; perm: string };

const globalNav: NavItem[] = [
  { to: '/projects', key: 'projects', perm: 'projects.view' },
  { to: '/library', key: 'library', perm: 'library.view' },
  { to: '/hmi-library', key: 'hmiLibrary', perm: 'hmi.library.view' },
  { to: '/system', key: 'system', perm: 'settings.view' },
];

export const projectNavGroups: { group: string; items: NavItem[] }[] = [
  {
    group: 'project',
    items: [
      { to: 'overview', key: 'overview', perm: 'projects.view' },
      { to: 'documents', key: 'documents', perm: 'documents.view' },
    ],
  },
  {
    group: 'analysis',
    items: [
      { to: 'pid', key: 'pid', perm: 'analysis.edit' },
      { to: 'aoi', key: 'aoi', perm: 'analysis.edit' },
    ],
  },
  {
    group: 'config',
    items: [
      { to: 'rooms', key: 'rooms', perm: 'rooms.view' },
      { to: 'equipment', key: 'equipment', perm: 'equipment.view' },
      { to: 'signals', key: 'signals', perm: 'signals.edit' },
      { to: 'hardware', key: 'hardware', perm: 'hardware.view' },
      { to: 'scheme', key: 'scheme', perm: 'scheme.view' },
      { to: 'binding', key: 'binding', perm: 'binding.edit' },
      { to: 'regulators', key: 'regulators', perm: 'regulators.edit' },
      { to: 'mnemo', key: 'mnemo', perm: 'hmi.screens.view' },
    ],
  },
  {
    group: 'operation',
    items: [
      { to: 'monitoring', key: 'monitoring', perm: 'monitoring.view' },
      { to: 'alarms', key: 'alarms', perm: 'alarms.view' },
      { to: 'commissioning', key: 'commissioning', perm: 'commissioning.edit' },
    ],
  },
  {
    group: 'records',
    items: [
      { to: 'journal', key: 'journal', perm: 'audit.view' },
      { to: 'reports', key: 'reports', perm: 'reports.view' },
      { to: 'export', key: 'export', perm: 'plc.config.view' },
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
      {online && snapshot?.mode === 'plc' && <PlcClock time={snapshot.plcTime} ok={!!snapshot.plcClockOk} />}
    </span>
  );
}

function PlcClock({ time, ok }: { time?: string | null; ok: boolean }) {
  const { t, i18n } = useTranslation();
  const at = ok && time ? new Date(time) : null;
  if (!at || Number.isNaN(at.getTime()))
    return <span className="plc-clock plc-clock--bad" title={t('header.plcClockBad')}>--:--:--</span>;
  return (
    <span className="plc-clock" title={`${t('header.plcClock')}: ${at.toLocaleDateString(i18n.language)}`}>
      {at.toLocaleTimeString(i18n.language, { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false })}
    </span>
  );
}

const systemPerms = ['settings.view', 'users.view', 'roles.view', 'audit.view', 'firmware.view', 'can3.use'];

function GlobalNav() {
  const { t } = useTranslation();
  const { can } = useAuth();
  return (
    <nav className="app-nav">
      {globalNav
        .filter((i) => (i.key === 'system' ? systemPerms.some(can) : can(i.perm)))
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
  const { can } = useAuth();
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
        const items = g.items.filter((i) => can(i.perm));
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
        <div className="header-left">
          <div className="header-logo">
            <img src={logoDcm} alt={t('header.logoAlt')} className="header-logo__image" />
            <div className="header-logo__text">
              <span className="header-logo__name">DCM</span>
              <span className="header-logo__subtitle">{t('header.logoSubtitle')}</span>
            </div>
          </div>
        </div>
        <div className="header-brand">{t('appTitle')}</div>
        <div className="header-actions">
          <PlcStatus />
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
            {user?.fullName || user?.login}
            {user && (
              <>
                {' · '}
                <BidiText>{user.roleName ? pickName(user.roleName, i18n.language).value : user.role}</BidiText>
              </>
            )}
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
  const preview = useMatch('/p/:projectId/hmi-preview/:screenId') ?? useMatch('/p/:projectId/hmi-preview');
  const projectId = match?.params.projectId;
  if (projectId && preview) {
    return (
      <ProjectProvider projectId={projectId}>
        <Outlet />
      </ProjectProvider>
    );
  }
  return projectId ? (
    <ProjectProvider projectId={projectId}>
      <Shell inProject />
    </ProjectProvider>
  ) : (
    <Shell inProject={false} />
  );
}
