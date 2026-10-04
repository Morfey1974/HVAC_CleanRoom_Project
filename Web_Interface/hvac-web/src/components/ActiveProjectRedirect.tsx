import { useEffect, useState } from 'react';
import { Navigate, useParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api } from '../api/client';
import { useAuth } from '../context/AuthContext';

/** Opens a section (e.g. alarms) of the project that runs on the site PLC. */
export function ActiveProjectRedirect() {
  const { section = 'monitoring' } = useParams();
  const { token } = useAuth();
  const { t } = useTranslation();
  const [target, setTarget] = useState<string | null>(null);

  useEffect(() => {
    if (!token) return;
    api
      .projects(token)
      .then((list) => {
        const active = list.find((p) => p.isActiveOnSite);
        setTarget(active ? `/p/${active.id}/${section}` : '/projects');
      })
      .catch(() => setTarget('/projects'));
  }, [token, section]);

  return target ? <Navigate to={target} replace /> : <div className="card muted">{t('common.loading')}</div>;
}
