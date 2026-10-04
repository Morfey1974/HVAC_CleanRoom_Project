import { useCallback, useEffect, useMemo, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type Project } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatDate, pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { confirmArchive } from './ProjectForm';

export function ProjectsPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const navigate = useNavigate();
  const [archived, setArchived] = useState(false);
  const [list, setList] = useState<Project[] | null>(null);
  const [search, setSearch] = useState('');
  const [error, setError] = useState('');
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');

  const load = useCallback(() => {
    if (token) api.projects(token, archived).then(setList).catch((e: Error) => setError(e.message));
  }, [token, archived]);
  useEffect(load, [load]);

  const filtered = useMemo(() => {
    const s = search.trim().toLowerCase();
    if (!list || !s) return list ?? [];
    return list.filter((p) =>
      [p.name.ru, p.name.en, p.name.he, p.number, p.customer, p.address].some((v) => v.toLowerCase().includes(s)),
    );
  }, [list, search]);

  const act = async (fn: () => Promise<unknown>) => {
    try {
      await fn();
      load();
    } catch (e) {
      setError((e as Error).message);
    }
  };

  const copy = (p: Project) =>
    act(async () => {
      if (!token) return;
      const c = await api.copyProject(token, p.id);
      navigate(`/p/${c.id}/overview`);
    });

  const archive = async (p: Project, value: boolean) => {
    if (!token) return;
    const reason = await confirmArchive(confirm, t, !value, pickName(p.name, i18n.language).value);
    if (reason === null) return;
    act(() => api.archiveProject(token, p.id, value, reason || undefined));
  };

  const copyAsk = async (p: Project) => {
    const ok = await confirm({
      title: t('projects.copy'),
      message: t('projects.copyConfirm', { name: pickName(p.name, i18n.language).value }),
      confirmText: t('projects.copy'),
    });
    if (ok !== null) copy(p);
  };

  return (
    <div className="page">
      <div className="page-header">
        <h1>
          {t('projects.title')} {list && <span className="count-pill">{filtered.length}</span>}
        </h1>
        {canEdit && (
          <Link to="/projects/new" className="btn btn-primary">
            + {t('projects.create')}
          </Link>
        )}
      </div>

      <div className="toolbar">
        <div className="tabs tabs--inline">
          <button type="button" className={!archived ? 'tab active' : 'tab'} onClick={() => setArchived(false)}>
            {t('projects.current')}
          </button>
          <button type="button" className={archived ? 'tab active' : 'tab'} onClick={() => setArchived(true)}>
            {t('projects.archive')}
          </button>
        </div>
        <input placeholder={t('common.search')} value={search} onChange={(e) => setSearch(e.target.value)} {...bidiAutoInput('search-input')} />
      </div>

      {error && <div className="error-banner">{error}</div>}
      {list === null && <div className="card muted">{t('common.loading')}</div>}
      {list !== null && filtered.length === 0 && (
        <div className="card empty-state">
          <p>{archived ? t('projects.emptyArchive') : t('projects.empty')}</p>
          {!archived && canEdit && (
            <Link to="/projects/new" className="btn btn-primary">
              + {t('projects.create')}
            </Link>
          )}
        </div>
      )}

      <div className="project-grid">
        {filtered.map((p) => {
          const name = pickName(p.name, i18n.language);
          return (
            <div key={p.id} className={p.isActiveOnSite ? 'card project-card project-card--site' : 'card project-card'}>
              <Link to={`/p/${p.id}/overview`} className="project-card__main">
                <div className="project-card__head">
                  <BidiText as="h3">{name.value}</BidiText>
                  {p.isActiveOnSite && <span className="badge badge--site">{t('projects.activeOnSite')}</span>}
                </div>
                <div className="project-card__meta">
                  {p.number && <span className="ltr-value">{p.number}</span>}
                  {p.customer && <BidiText>{p.customer}</BidiText>}
                  {p.isoClass && <span>ISO {p.isoClass}</span>}
                </div>
                <div className="project-card__stats">
                  <span>
                    {t('projectNav.rooms')}: <b>{p.roomCount}</b>
                  </span>
                  <span>
                    {t('projectNav.equipment')}: <b>{p.equipmentCount}</b>
                  </span>
                  <span>
                    {t('projectNav.hardware')}: <b>{p.moduleCount}</b>
                  </span>
                  <span>
                    {t('projectNav.documents')}: <b>{p.documentCount}</b>
                  </span>
                </div>
                <div className="project-card__date muted">
                  {t('projects.updated')}: <span className="ltr-value">{formatDate(p.updatedAt, i18n.language)}</span>
                </div>
              </Link>
              {canEdit && (
                <div className="project-card__actions">
                  {!p.isArchived && (
                    <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => copyAsk(p)}>
                      {t('projects.copy')}
                    </button>
                  )}
                  <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => archive(p, !p.isArchived)}>
                    {p.isArchived ? t('projects.restore') : t('projects.toArchive')}
                  </button>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
