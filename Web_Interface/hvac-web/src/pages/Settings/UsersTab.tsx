import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type AppRole, type UserRow, type UserSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatDateTime, pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { Modal } from '../../components/Modal';

type Edit = { id: string | null; data: UserSave };

export function UsersTab() {
  const { t, i18n } = useTranslation();
  const { token, can } = useAuth();
  const canEdit = can('users.edit');
  const [rows, setRows] = useState<UserRow[] | null>(null);
  const [roles, setRoles] = useState<AppRole[]>([]);
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');

  const load = useCallback(() => {
    if (!token) return;
    Promise.all([api.users(token), api.roles(token)])
      .then(([u, r]) => {
        setRows(u);
        setRoles(r);
      })
      .catch((e: Error) => setError(e.message));
  }, [token]);
  useEffect(load, [load]);

  const openNew = () =>
    setEdit({
      id: null,
      data: { login: '', fullName: '', roleId: roles[0]?.id ?? '', password: '', isActive: true },
    });
  const openEdit = (u: UserRow) =>
    setEdit({ id: u.id, data: { login: u.login, fullName: u.fullName, roleId: u.roleId, password: '', isActive: u.isActive, reason: '' } });

  const save = async () => {
    if (!token || !edit) return;
    try {
      setError('');
      if (edit.id) await api.updateUser(token, edit.id, edit.data);
      else await api.createUser(token, edit.data);
      setEdit(null);
      load();
    } catch (e) {
      const m = (e as Error).message;
      setError(t(`settings.errors.${m}`, { defaultValue: m }));
    }
  };

  const setData = (patch: Partial<UserSave>) => edit && setEdit({ ...edit, data: { ...edit.data, ...patch } });

  return (
    <div>
      <div className="page-header">
        <p className="muted">{t('settings.usersHint')}</p>
        {canEdit && (
          <button type="button" className="btn btn-primary" onClick={openNew} disabled={!roles.length}>
            + {t('settings.addUser')}
          </button>
        )}
      </div>
      {error && <div className="error-banner">{error}</div>}
      <div className="card table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('settings.login')}</th>
              <th>{t('settings.fullName')}</th>
              <th>{t('settings.role')}</th>
              <th>{t('settings.lastLogin')}</th>
              <th>{t('settings.active')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rows === null && (
              <tr>
                <td colSpan={6} className="muted">
                  {t('common.loading')}
                </td>
              </tr>
            )}
            {rows?.map((u) => (
              <tr key={u.id}>
                <td className="ltr-value">{u.login}</td>
                <td>
                  <BidiText>{u.fullName}</BidiText>
                </td>
                <td>
                  <BidiText>{pickName(u.roleName, i18n.language).value}</BidiText>
                </td>
                <td className="ltr-value">{formatDateTime(u.lastLoginAt, i18n.language)}</td>
                <td>{u.isActive ? '✓' : ''}</td>
                <td className="row-actions">
                  {canEdit && (
                    <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openEdit(u)}>
                      {t('common.edit')}
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {edit && (
        <Modal sizeKey="user-edit" onClose={() => setEdit(null)}>
          <h2>{edit.id ? t('settings.editUser') : t('settings.addUser')}</h2>
          <div className="form-grid">
            <label>
              {t('settings.login')}
              <input dir="ltr" value={edit.data.login} onChange={(e) => setData({ login: e.target.value })} />
            </label>
            <label>
              {t('settings.fullName')}
              <input value={edit.data.fullName} onChange={(e) => setData({ fullName: e.target.value })} {...bidiAutoInput()} />
            </label>
            <label>
              {t('settings.role')}
              <select value={edit.data.roleId} onChange={(e) => setData({ roleId: e.target.value })}>
                {roles.map((r) => (
                  <option key={r.id} value={r.id}>
                    {pickName(r.name, i18n.language).value}
                  </option>
                ))}
              </select>
            </label>
            <label>
              {t('settings.password')}
              <input type="password" dir="ltr" value={edit.data.password ?? ''} onChange={(e) => setData({ password: e.target.value })} />
              <span className="field-hint">{edit.id ? t('settings.passwordHintEdit') : t('settings.passwordHint')}</span>
            </label>
            <label className="checkbox-row">
              <input type="checkbox" checked={edit.data.isActive} onChange={(e) => setData({ isActive: e.target.checked })} />
              {t('settings.active')}
            </label>
            {edit.id && (
              <label>
                {t('common.reason')}
                <input value={edit.data.reason ?? ''} onChange={(e) => setData({ reason: e.target.value })} {...bidiAutoInput()} />
              </label>
            )}
          </div>
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
