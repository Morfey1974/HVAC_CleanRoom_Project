import { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type DocumentKind, type ProjectSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatSize } from '../../lib/localized';
import { ProjectForm, emptyProject, hasProjectName } from './ProjectForm';

const steps = ['general', 'pid', 'aoi', 'confirm'] as const;
type Step = (typeof steps)[number];

function FilePick({ files, onChange, accept }: { files: File[]; onChange: (f: File[]) => void; accept: string }) {
  const { t } = useTranslation();
  return (
    <div className="file-pick">
      <label className="drop-zone">
        <input
          type="file"
          multiple
          accept={accept}
          onChange={(e) => {
            onChange([...files, ...Array.from(e.target.files ?? [])]);
            e.target.value = '';
          }}
        />
        <span>{t('documents.pick')}</span>
        <span className="field-hint ltr-value">{accept}</span>
      </label>
      {files.length > 0 && (
        <ul className="file-list">
          {files.map((f, i) => (
            <li key={`${f.name}-${i}`}>
              <span className="ltr-value">{f.name}</span>
              <span className="muted ltr-value">{formatSize(f.size)}</span>
              <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => onChange(files.filter((_, j) => j !== i))}>
                ✕
              </button>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

export function ProjectWizard() {
  const { t } = useTranslation();
  const { token } = useAuth();
  const navigate = useNavigate();
  const [step, setStep] = useState<Step>('general');
  const [data, setData] = useState<ProjectSave>(emptyProject);
  const [pid, setPid] = useState<File[]>([]);
  const [aoi, setAoi] = useState<File[]>([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const idx = steps.indexOf(step);

  const next = () => {
    if (step === 'general' && !hasProjectName(data)) {
      setError(t('projects.nameRequired'));
      return;
    }
    setError('');
    setStep(steps[idx + 1]);
  };

  const create = async () => {
    if (!token) return;
    setBusy(true);
    setError('');
    try {
      const p = await api.createProject(token, data);
      const uploads: [DocumentKind, File][] = [...pid.map((f) => ['pid', f] as [DocumentKind, File]), ...aoi.map((f) => ['aoi', f] as [DocumentKind, File])];
      for (const [kind, f] of uploads) await api.uploadDocument(token, p.id, kind, f);
      navigate(`/p/${p.id}/overview`);
    } catch (e) {
      setError((e as Error).message);
      setBusy(false);
    }
  };

  return (
    <div className="page wizard">
      <div className="page-header">
        <h1>{t('projects.create')}</h1>
        <Link to="/projects" className="btn btn-ghost-inline">
          {t('common.cancel')}
        </Link>
      </div>

      <ol className="stepper">
        {steps.map((s, i) => (
          <li key={s} className={i === idx ? 'active' : i < idx ? 'done' : ''}>
            <span className="stepper__num">{i < idx ? '✓' : i + 1}</span>
            {t(`wizard.steps.${s}`)}
          </li>
        ))}
      </ol>

      <div className="card">
        {error && <div className="error-banner">{error}</div>}

        {step === 'general' && <ProjectForm value={data} onChange={setData} />}

        {step === 'pid' && (
          <>
            <p>{t('wizard.pidHint')}</p>
            <FilePick files={pid} onChange={setPid} accept=".dwg,.dxf,.pdf,.png,.jpg" />
          </>
        )}

        {step === 'aoi' && (
          <>
            <p>{t('wizard.aoiHint')}</p>
            <FilePick files={aoi} onChange={setAoi} accept=".xlsx,.xls,.csv,.pdf" />
          </>
        )}

        {step === 'confirm' && (
          <dl className="summary">
            <dt>{t('projects.fields.name')}</dt>
            <dd>{[data.name.he, data.name.ru, data.name.en].filter(Boolean).join(' / ')}</dd>
            {data.number && (
              <>
                <dt>{t('projects.fields.number')}</dt>
                <dd>{data.number}</dd>
              </>
            )}
            {data.customer && (
              <>
                <dt>{t('projects.fields.customer')}</dt>
                <dd>{data.customer}</dd>
              </>
            )}
            <dt>P&amp;ID</dt>
            <dd>{pid.length ? pid.map((f) => f.name).join(', ') : t('wizard.skipped')}</dd>
            <dt>AOI</dt>
            <dd>{aoi.length ? aoi.map((f) => f.name).join(', ') : t('wizard.skipped')}</dd>
          </dl>
        )}

        <div className="modal-actions wizard__actions">
          {idx > 0 && (
            <button type="button" className="btn btn-ghost-inline" disabled={busy} onClick={() => setStep(steps[idx - 1])}>
              {t('wizard.back')}
            </button>
          )}
          {(step === 'pid' || step === 'aoi') && (
            <button
              type="button"
              className="btn btn-ghost-inline"
              onClick={() => {
                (step === 'pid' ? setPid : setAoi)([]);
                next();
              }}
            >
              {t('wizard.skip')}
            </button>
          )}
          {step !== 'confirm' ? (
            <button type="button" className="btn btn-primary" onClick={next}>
              {t('wizard.next')}
            </button>
          ) : (
            <button type="button" className="btn btn-primary" disabled={busy} onClick={create}>
              {busy ? t('common.loading') : t('wizard.create')}
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
