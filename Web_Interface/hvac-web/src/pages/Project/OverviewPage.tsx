import { useEffect, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type ProjectDocument, type ProjectSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { formatDateTime, pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { ProjectForm, confirmArchive, hasProjectName, toProjectSave } from '../Projects/ProjectForm';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';

type Readiness = { key: string; to: string; state: 'done' | 'todo' | 'later'; detail?: string };

export function OverviewPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { project, projectId, setProject, reload } = useProject();
  const navigate = useNavigate();
  const [docs, setDocs] = useState<ProjectDocument[]>([]);
  const [edit, setEdit] = useState<ProjectSave | null>(null);
  const [error, setError] = useState('');
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');

  useEffect(() => {
    if (token)
      api
        .documents(token, projectId)
        .then(setDocs)
        .catch(() => undefined);
  }, [token, projectId]);

  if (!project) return null;
  const name = pickName(project.name, i18n.language);
  const pidCount = docs.filter((d) => d.kind === 'pid').length;
  const aoiCount = docs.filter((d) => d.kind === 'aoi').length;

  const readiness: Readiness[] = [
    { key: 'pid', to: 'documents', state: pidCount ? 'done' : 'todo', detail: pidCount ? `${pidCount}` : undefined },
    { key: 'aoi', to: 'documents', state: aoiCount ? 'done' : 'todo', detail: aoiCount ? `${aoiCount}` : undefined },
    { key: 'rooms', to: 'rooms', state: project.roomCount ? 'done' : 'todo', detail: project.roomCount ? `${project.roomCount}` : undefined },
    {
      key: 'equipment',
      to: 'equipment',
      state: project.equipmentCount ? 'done' : 'todo',
      detail: project.equipmentCount ? `${project.equipmentCount}` : undefined,
    },
    { key: 'signals', to: 'signals', state: 'later' },
    { key: 'hardware', to: 'hardware', state: project.moduleCount ? 'done' : 'todo', detail: project.moduleCount ? `${project.moduleCount}` : undefined },
    { key: 'binding', to: 'binding', state: 'later' },
    { key: 'regulators', to: 'regulators', state: 'later' },
    { key: 'mnemo', to: 'mnemo', state: 'later' },
    { key: 'commissioning', to: 'commissioning', state: 'later' },
  ];

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
    } catch (e) {
      setError((e as Error).message);
    }
  };

  const save = () =>
    run(async () => {
      if (!token || !edit) return;
      if (!hasProjectName(edit)) throw new Error(t('projects.nameRequired'));
      setProject(await api.updateProject(token, projectId, edit));
      setEdit(null);
    });

  const activate = () =>
    run(async () => {
      if (!token) return;
      const reason = await confirm({
        title: t('overview.activate'),
        message: t('overview.activateConfirm'),
        confirmText: t('overview.activate'),
        reason: 'optional',
      });
      if (reason === null) return;
      setProject(await api.activateProject(token, projectId, reason || undefined));
    });

  const copy = () =>
    run(async () => {
      if (!token) return;
      const c = await api.copyProject(token, projectId);
      navigate(`/p/${c.id}/overview`);
    });

  const archive = () =>
    run(async () => {
      if (!token) return;
      const reason = await confirmArchive(confirm, t, project.isArchived, pickName(project.name, i18n.language).value);
      if (reason === null) return;
      setProject(await api.archiveProject(token, projectId, !project.isArchived, reason || undefined));
      reload();
    });

  const field = (label: string, value: string | number | null) =>
    value || value === 0 ? (
      <>
        <dt>{label}</dt>
        <dd>
          <BidiText>{String(value)}</BidiText>
        </dd>
      </>
    ) : null;

  return (
    <div className="page">
      <div className="page-header">
        <h1>
          <BidiText>{name.value}</BidiText>
        </h1>
        <div className="row-actions">
          {canEdit && (
            <button type="button" className="btn btn-primary" onClick={() => setEdit(toProjectSave(project))}>
              {t('common.edit')}
            </button>
          )}
          {canEdit && !project.isArchived && (
            <button type="button" className="btn btn-ghost-inline" onClick={copy}>
              {t('projects.copy')}
            </button>
          )}
          {canEdit && (
            <button type="button" className="btn btn-ghost-inline" onClick={archive}>
              {project.isArchived ? t('projects.restore') : t('projects.toArchive')}
            </button>
          )}
        </div>
      </div>
      {error && <div className="error-banner">{error}</div>}
      {project.isArchived && <div className="warning-banner">{t('overview.archived')}</div>}

      <div className="overview-grid">
        <div className="card">
          <h2>{t('overview.general')}</h2>
          <dl className="summary">
            {field(t('projects.fields.name_he'), project.name.he)}
            {field(t('projects.fields.name_ru'), project.name.ru)}
            {field(t('projects.fields.name_en'), project.name.en)}
            {field(t('projects.fields.number'), project.number)}
            {field(t('projects.fields.customer'), project.customer)}
            {field(t('projects.fields.address'), project.address)}
            {field(t('projects.fields.responsible'), project.responsible)}
            {field(t('projects.fields.isoClass'), project.isoClass ? `ISO ${project.isoClass}` : null)}
            {field(t('projects.fields.description'), project.description)}
            <dt>{t('overview.created')}</dt>
            <dd className="ltr-value">
              {formatDateTime(project.createdAt, i18n.language)} · {project.createdBy}
            </dd>
          </dl>
        </div>

        <div className="card">
          <h2>{t('overview.site')}</h2>
          {project.isActiveOnSite ? (
            <p>
              <span className="badge badge--site">{t('projects.activeOnSite')}</span> {t('overview.siteActive')}
            </p>
          ) : (
            <>
              <p className="muted">{t('overview.siteInactive')}</p>
              {hasRole('Admin') && !project.isArchived && (
                <button type="button" className="btn btn-primary" onClick={activate}>
                  {t('overview.activate')}
                </button>
              )}
            </>
          )}

          <h2 className="mt">{t('overview.readiness')}</h2>
          <ul className="readiness">
            {readiness.map((r) => (
              <li key={r.key} className={`readiness__item readiness__item--${r.state}`}>
                <span className="readiness__mark">{r.state === 'done' ? '✓' : r.state === 'todo' ? '○' : '…'}</span>
                <Link to={`/p/${projectId}/${r.to}`}>{t(`overview.steps.${r.key}`)}</Link>
                {r.detail && <span className="count-pill">{r.detail}</span>}
                {r.state === 'later' && <span className="muted small">{t('overview.later')}</span>}
              </li>
            ))}
          </ul>
        </div>
      </div>

      {edit && (
        <Modal sizeKey="project-edit" className="modal-wide" onClose={() => setEdit(null)}>
          <h2>{t('overview.editTitle')}</h2>
          <ProjectForm value={edit} onChange={setEdit} />
          <label className="form-grid mt">
            {t('common.reason')}
            <input value={edit.reason ?? ''} onChange={(e) => setEdit({ ...edit, reason: e.target.value })} {...bidiAutoInput()} />
            <span className="field-hint">{t('common.reasonHint')}</span>
          </label>
          <div className="modal-actions">
            <button type="button" className="btn btn-ghost-inline" onClick={() => setEdit(null)}>
              {t('common.cancel')}
            </button>
            <button type="button" className="btn btn-primary" onClick={save}>
              {t('common.save')}
            </button>
          </div>
        </Modal>
      )}
    </div>
  );
}
