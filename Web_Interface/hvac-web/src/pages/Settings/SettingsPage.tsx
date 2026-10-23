import { useEffect, useState, type FormEvent } from 'react';
import { NavLink, Navigate, Route, Routes } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type Settings } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { RECORDS_STRICT } from '../../lib/recordsMode';
import { languages } from '../../i18n';
import { bidiAutoInput } from '../../components/BidiText';
import { AuditTable } from '../../components/AuditTable';
import { FirmwareTab } from './FirmwareTab';
import { Can3Tab } from './Can3Tab';
import { UsersTab } from './UsersTab';
import { RolesTab } from './RolesTab';

export function SettingsPage() {
  const { t } = useTranslation();
  const { can } = useAuth();
  const tabs = [
    { k: 'general', show: can('settings.view') },
    { k: 'users', show: can('users.view') },
    { k: 'roles', show: can('roles.view') },
    { k: 'audit', show: can('audit.view') },
    { k: 'firmware', show: can('firmware.view') },
    { k: 'can3', show: can('can3.use') },
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
        <Route index element={<Navigate to={tabs[0]?.k ?? 'general'} replace />} />
        {can('settings.view') && <Route path="general" element={<GeneralTab />} />}
        {can('users.view') && <Route path="users" element={<UsersTab />} />}
        {can('roles.view') && <Route path="roles" element={<RolesTab />} />}
        {can('audit.view') && <Route path="audit" element={<AuditTable />} />}
        {can('firmware.view') && <Route path="firmware" element={<FirmwareTab />} />}
        {can('can3.use') && <Route path="can3" element={<Can3Tab />} />}
      </Routes>
    </div>
  );
}

function GeneralTab() {
  const { t } = useTranslation();
  const { token, can } = useAuth();
  const [s, setS] = useState<Settings | null>(null);
  const [msg, setMsg] = useState('');
  const canEdit = can('settings.edit');

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
      <label className="settings-check">
        <input type="checkbox" checked={RECORDS_STRICT} disabled />
        {t('settings.recordsStrict')}
        <span className="field-hint">{t('settings.recordsStrictHint')}</span>
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
