import { useEffect, useState, type FormEvent } from 'react';
import { NavLink, Navigate, Route, Routes } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type Settings, type UserRow } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatDateTime } from '../../lib/localized';
import { languages } from '../../i18n';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { AuditTable } from '../../components/AuditTable';

export function SettingsPage() {
  const { t } = useTranslation();
  const { hasRole } = useAuth();
  const tabs = [
    { k: 'general', show: hasRole('Viewer') },
    { k: 'users', show: hasRole('Admin') },
    { k: 'audit', show: hasRole('Engineer') },
  ].filter((x) => x.show);

  return (
    <div className="page">
      <h1>{t('nav.system')}</h1>
      <div className="tabs">
        {tabs.map(({ k }) => (
          <NavLink key={k} to={`/system/${k}`} className={({ isActive }) => (isActive ? 'tab active' : 'tab')}>
            {t(`settings.${k}`)}
          </NavLink>
        ))}
      </div>
      <Routes>
        <Route index element={<Navigate to="general" replace />} />
        <Route path="general" element={<GeneralTab />} />
        {hasRole('Admin') && <Route path="users" element={<UsersTab />} />}
        {hasRole('Engineer') && <Route path="audit" element={<AuditTable />} />}
      </Routes>
    </div>
  );
}

function GeneralTab() {
  const { t } = useTranslation();
  const { token, hasRole } = useAuth();
  const [s, setS] = useState<Settings | null>(null);
  const [msg, setMsg] = useState('');
  const canEdit = hasRole('Admin');

  useEffect(() => {
    if (token) api.settings(token).then(setS).catch(() => undefined);
  }, [token]);

  const save = async (e: FormEvent) => {
    e.preventDefault();
    if (!token || !s) return;
    setS(await api.saveSettings(token, s));
    setMsg(t('common.saved'));
    setTimeout(() => setMsg(''), 2500);
  };

  if (!s) return <div className="card muted">{t('common.loading')}</div>;
  return (
    <form className="card form-grid" onSubmit={save}>
      {msg && <div className="success-banner">{msg}</div>}
      <label>
        {t('settings.siteName')}
        <input value={s.siteName} disabled={!canEdit} onChange={(e) => setS({ ...s, siteName: e.target.value })} {...bidiAutoInput()} />
      </label>
      <label>
        {t('settings.startupLanguage')}
        <select value={s.startupLanguage} disabled={!canEdit} onChange={(e) => setS({ ...s, startupLanguage: e.target.value })}>
          {languages.map((l) => (
            <option key={l.code} value={l.code}>
              {l.label}
            </option>
          ))}
        </select>
        <span className="field-hint">{t('settings.startupLanguageHint')}</span>
      </label>
      <label>
        {t('settings.plcMode')}
        <select value={s.plcMode} disabled={!canEdit} onChange={(e) => setS({ ...s, plcMode: e.target.value })}>
          <option value="simulator">{t('settings.plcModes.simulator')}</option>
          <option value="plc">{t('settings.plcModes.plc')}</option>
        </select>
      </label>
      <label>
        {t('settings.plcAddress')}
        <input value={s.plcAddress} disabled={!canEdit} dir="ltr" onChange={(e) => setS({ ...s, plcAddress: e.target.value })} />
      </label>
      {canEdit && (
        <div>
          <button type="submit" className="btn btn-primary">
            {t('common.save')}
          </button>
        </div>
      )}
    </form>
  );
}

function UsersTab() {
  const { t, i18n } = useTranslation();
  const { token } = useAuth();
  const [rows, setRows] = useState<UserRow[]>([]);
  useEffect(() => {
    if (token) api.users(token).then(setRows).catch(() => undefined);
  }, [token]);
  return (
    <div className="card table-wrap">
      <table className="data-table">
        <thead>
          <tr>
            <th>{t('settings.login')}</th>
            <th>{t('settings.fullName')}</th>
            <th>{t('settings.role')}</th>
            <th>{t('settings.lastLogin')}</th>
            <th>{t('settings.active')}</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((u) => (
            <tr key={u.id}>
              <td className="ltr-value">{u.login}</td>
              <td>
                <BidiText>{u.fullName}</BidiText>
              </td>
              <td>{t(`roles.${u.role}`)}</td>
              <td className="ltr-value">{formatDateTime(u.lastLoginAt, i18n.language)}</td>
              <td>{u.isActive ? '✓' : ''}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
