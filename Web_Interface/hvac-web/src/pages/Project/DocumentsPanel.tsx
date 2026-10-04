import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, documentKinds, type DocumentKind, type ProjectDocument } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { formatDateTime, formatSize } from '../../lib/localized';
import { useConfirm } from '../../components/Dialog';

/** Project files; with `kind` shows and uploads only that kind. */
export function DocumentsPanel({ kind }: { kind?: DocumentKind }) {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, reload: reloadProject } = useProject();
  const [docs, setDocs] = useState<ProjectDocument[]>([]);
  const [uploadKind, setUploadKind] = useState<DocumentKind>(kind ?? 'pid');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');

  const load = useCallback(() => {
    if (token) api.documents(token, projectId).then(setDocs).catch((e: Error) => setError(e.message));
  }, [token, projectId]);
  useEffect(load, [load]);

  const upload = async (files: FileList | null) => {
    if (!token || !files?.length) return;
    setBusy(true);
    setError('');
    try {
      for (const f of Array.from(files)) await api.uploadDocument(token, projectId, uploadKind, f);
      load();
      reloadProject();
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  };

  const remove = async (d: ProjectDocument) => {
    if (!token) return;
    const reason = await confirm({
      title: t('dialog.deleteTitle'),
      message: t('documents.confirmDelete', { name: d.fileName }),
      confirmText: t('common.delete'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return;
    try {
      await api.deleteDocument(token, projectId, d.id, reason || undefined);
      load();
      reloadProject();
    } catch (e) {
      setError((e as Error).message);
    }
  };

  const rows = kind ? docs.filter((d) => d.kind === kind) : docs;

  return (
    <div className="card">
      {canEdit && (
        <div className="upload-bar">
          {!kind && (
            <select value={uploadKind} onChange={(e) => setUploadKind(e.target.value as DocumentKind)}>
              {documentKinds.map((k) => (
                <option key={k} value={k}>
                  {t(`documents.kinds.${k}`)}
                </option>
              ))}
            </select>
          )}
          <label className={busy ? 'btn btn-primary disabled' : 'btn btn-primary'}>
            {busy ? t('common.loading') : `+ ${t('documents.upload')}`}
            <input type="file" multiple hidden disabled={busy} onChange={(e) => { upload(e.target.files); e.target.value = ''; }} />
          </label>
        </div>
      )}
      {error && <div className="error-banner">{error}</div>}
      <div className="table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              {!kind && <th>{t('documents.kind')}</th>}
              <th>{t('documents.file')}</th>
              <th>{t('documents.size')}</th>
              <th>{t('documents.uploaded')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rows.length === 0 && (
              <tr>
                <td colSpan={5} className="muted">
                  {t('documents.empty')}
                </td>
              </tr>
            )}
            {rows.map((d) => (
              <tr key={d.id}>
                {!kind && (
                  <td>
                    <span className={`badge badge--${d.kind}`}>{t(`documents.kinds.${d.kind}`)}</span>
                  </td>
                )}
                <td className="ltr-value">
                  <button type="button" className="link-button" onClick={() => token && api.downloadDocument(token, projectId, d)}>
                    {d.fileName}
                  </button>
                </td>
                <td className="ltr-value">{formatSize(d.sizeBytes)}</td>
                <td className="ltr-value">
                  {formatDateTime(d.uploadedAt, i18n.language)} · {d.uploadedBy}
                </td>
                <td className="row-actions">
                  {canEdit && (
                    <button type="button" className="btn btn-small btn-danger" onClick={() => remove(d)}>
                      {t('common.delete')}
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
