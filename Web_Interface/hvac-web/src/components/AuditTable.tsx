import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type AuditEntry } from '../api/client';
import { useAuth } from '../context/AuthContext';
import { formatDateTime } from '../lib/localized';
import { BidiText } from './BidiText';

const PAGE = 50;

/** Append-only action log; with projectId shows only that project's entries. */
export function AuditTable({ projectId }: { projectId?: string }) {
  const { t, i18n } = useTranslation();
  const { token } = useAuth();
  const [page, setPage] = useState(0);
  const [data, setData] = useState<{ items: AuditEntry[]; total: number }>({ items: [], total: 0 });

  const load = useCallback(() => {
    if (token) api.audit(token, page * PAGE, PAGE, projectId).then(setData).catch(() => undefined);
  }, [token, page, projectId]);
  useEffect(load, [load]);

  const pages = Math.max(1, Math.ceil(data.total / PAGE));
  return (
    <div className="card">
      <div className="table-wrap">
        <table className="data-table data-table--compact">
          <thead>
            <tr>
              <th>{t('audit.at')}</th>
              <th>{t('audit.user')}</th>
              <th>{t('audit.action')}</th>
              <th>{t('audit.target')}</th>
              <th>{t('audit.oldValue')}</th>
              <th>{t('audit.newValue')}</th>
              <th>{t('audit.reason')}</th>
              <th>{t('audit.source')}</th>
            </tr>
          </thead>
          <tbody>
            {data.items.map((a) => (
              <tr key={a.id}>
                <td className="ltr-value nowrap">{formatDateTime(a.at, i18n.language)}</td>
                <td className="ltr-value">{a.userLogin}</td>
                <td>{t(`audit.actions.${a.action}`, { defaultValue: a.action })}</td>
                <td>
                  <BidiText>{a.target}</BidiText>
                </td>
                <td className="audit-json">
                  <BidiText>{a.oldValue}</BidiText>
                </td>
                <td className="audit-json">
                  <BidiText>{a.newValue}</BidiText>
                </td>
                <td>
                  <BidiText>{a.reason}</BidiText>
                </td>
                <td className="ltr-value">{a.source}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div className="pager">
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={page === 0} onClick={() => setPage(page - 1)}>
          ‹
        </button>
        <span className="ltr-value">
          {page + 1} / {pages}
        </span>
        <button type="button" className="btn btn-small btn-ghost-inline" disabled={page + 1 >= pages} onClick={() => setPage(page + 1)}>
          ›
        </button>
      </div>
    </div>
  );
}
