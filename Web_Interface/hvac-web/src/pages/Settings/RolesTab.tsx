import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type AppRole, type AppRoleSave, type PermissionCatalog } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';

type Edit = { id: string | null; builtIn: boolean; data: AppRoleSave };

export function RolesTab() {
  const { t, i18n } = useTranslation();
  const { token, can } = useAuth();
  const canEdit = can('roles.edit');
  const confirm = useConfirm();
  const [rows, setRows] = useState<AppRole[] | null>(null);
  const [catalog, setCatalog] = useState<PermissionCatalog | null>(null);
  const [archived, setArchived] = useState(false);
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');

  const load = useCallback(() => {
    if (!token) return;
    Promise.all([api.roles(token, archived), api.permissionCatalog(token)])
      .then(([r, c]) => {
        setRows(r);
        setCatalog(c);
      })
      .catch((e: Error) => setError(e.message));
  }, [token, archived]);
  useEffect(load, [load]);

  const openNew = () =>
    setEdit({
      id: null,
      builtIn: false,
      data: { code: '', name: { ru: '', en: '', he: '' }, description: '', permissions: [] },
    });
  const openEdit = (r: AppRole) =>
    setEdit({
      id: r.id,
      builtIn: r.isBuiltIn,
      data: { code: r.code, name: { ...r.name }, description: r.description, permissions: [...r.permissions], reason: '' },
    });

  const save = async () => {
    if (!token || !edit) return;
    try {
      setError('');
      if (edit.id) await api.updateRole(token, edit.id, edit.data);
      else await api.createRole(token, edit.data);
      setEdit(null);
      load();
    } catch (e) {
      const m = (e as Error).message;
      setError(t(`settings.errors.${m}`, { defaultValue: m }));
    }
  };

  const archive = async (r: AppRole) => {
    if (!token) return;
    const title = r.isArchived ? t('projects.restore') : t('projects.toArchive');
    const reason = await confirm({
      title,
      message: t(r.isArchived ? 'library.restoreConfirm' : 'library.archiveConfirm', { name: pickName(r.name, i18n.language).value }),
      confirmText: title,
      danger: !r.isArchived,
      reason: r.isArchived ? 'none' : 'optional',
    });
    if (reason === null) return;
    try {
      await api.archiveRole(token, r.id, !r.isArchived, reason || undefined);
      load();
    } catch (e) {
      setError(t(`settings.errors.${(e as Error).message}`, { defaultValue: (e as Error).message }));
    }
  };

  const setData = (patch: Partial<AppRoleSave>) => edit && setEdit({ ...edit, data: { ...edit.data, ...patch } });
  const toggle = (key: string, on: boolean) => {
    if (!edit) return;
    const cur = edit.data.permissions;
    setData({ permissions: on ? [...cur, key] : cur.filter((k) => k !== key) });
  };
  const fillFrom = (r: AppRole) => setData({ permissions: [...r.permissions] });
  const allKeys = catalog?.groups.flatMap((g) => g.keys) ?? [];

  return (
    <div>
      <div className="page-header">
        <p className="muted">{t('settings.rolesHint')}</p>
        {canEdit && !archived && (
          <button type="button" className="btn btn-primary" onClick={openNew}>
            + {t('settings.addRole')}
          </button>
        )}
      </div>
      <label className="checkbox-row">
        <input type="checkbox" checked={archived} onChange={(e) => setArchived(e.target.checked)} />
        {t('library.showArchived')}
      </label>
      {error && <div className="error-banner">{error}</div>}
      <div className="card table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('library.code')}</th>
              <th>{t('library.name')}</th>
              <th>{t('settings.rights')}</th>
              <th>{t('settings.usersOnRole')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rows === null && (
              <tr>
                <td colSpan={5} className="muted">
                  {t('common.loading')}
                </td>
              </tr>
            )}
            {rows?.map((r) => (
              <tr key={r.id}>
                <td className="ltr-value">
                  <b>{r.code}</b>
                  {r.isBuiltIn && <div className="muted small">{t('settings.builtIn')}</div>}
                </td>
                <td>
                  <BidiText>{pickName(r.name, i18n.language).value}</BidiText>
                </td>
                <td className="ltr-value">{r.permissions.length}</td>
                <td className="ltr-value">{r.userCount || ''}</td>
                <td className="row-actions">
                  {canEdit && !r.isArchived && (
                    <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openEdit(r)}>
                      {t('common.edit')}
                    </button>
                  )}
                  {canEdit && !r.isBuiltIn && (
                    <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => archive(r)}>
                      {r.isArchived ? t('projects.restore') : t('projects.toArchive')}
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {edit && catalog && (
        <Modal sizeKey="role-edit" className="modal-xl" onClose={() => setEdit(null)}>
          <h2>{edit.id ? t('settings.editRole') : t('settings.addRole')}</h2>
          <div className="form-grid form-grid--wide">
            <div className="form-grid-3">
              {(['he', 'ru', 'en'] as const).map((l) => (
                <label key={l} className={edit.data.name[l].trim() ? '' : 'field-missing'}>
                  {t(`projects.fields.name_${l}`)}
                  <input value={edit.data.name[l]} onChange={(e) => setData({ name: { ...edit.data.name, [l]: e.target.value } })} {...bidiAutoInput()} />
                </label>
              ))}
            </div>
            <div className="form-grid-2">
              <label>
                {t('library.code')}
                <input dir="ltr" disabled={edit.builtIn} value={edit.data.code ?? ''} onChange={(e) => setData({ code: e.target.value.toUpperCase() })} />
                <span className="field-hint">{edit.builtIn ? t('settings.builtInCode') : t('settings.roleCodeHint')}</span>
              </label>
              <label>
                {t('projects.fields.description')}
                <input value={edit.data.description ?? ''} onChange={(e) => setData({ description: e.target.value })} {...bidiAutoInput()} />
              </label>
            </div>
            {rows && rows.length > 0 && (
              <label>
                {t('settings.fillFrom')}
                <select defaultValue="" onChange={(e) => e.target.value && fillFrom(rows.find((x) => x.id === e.target.value)!)}>
                  <option value="">{t('settings.fillFromPick')}</option>
                  {rows.map((r) => (
                    <option key={r.id} value={r.id}>
                      {pickName(r.name, i18n.language).value}
                    </option>
                  ))}
                </select>
              </label>
            )}
            <div className="props-head">
              <b>
                {t('settings.rights')} ({edit.data.permissions.length}/{allKeys.length})
              </b>
              <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => setData({ permissions: [...allKeys] })}>
                {t('settings.allRights')}
              </button>
              <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => setData({ permissions: [] })}>
                {t('settings.noRights')}
              </button>
            </div>
            {catalog.groups.map((g) => (
              <fieldset key={g.group} className="hmi-card" style={{ marginTop: 0 }}>
                <legend>
                  <b>{t(`permissions.groups.${g.group}`)}</b>
                </legend>
                <div className="checkbox-group">
                  {g.keys.map((k) => (
                    <label key={k} className="checkbox-row">
                      <input type="checkbox" checked={edit.data.permissions.includes(k)} onChange={(e) => toggle(k, e.target.checked)} />
                      {t(`permissions.keys.${k}`)}
                    </label>
                  ))}
                </div>
              </fieldset>
            ))}
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
