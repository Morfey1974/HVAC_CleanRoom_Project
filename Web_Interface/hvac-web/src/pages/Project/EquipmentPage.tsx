import { useCallback, useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, libraryCategories, type Equipment, type EquipmentSave, type LibraryItem, type Room } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';

type Edit = { id: string | null; data: EquipmentSave };
const emptyEq: EquipmentSave = { libraryItemId: null, tag: '', quantity: 1, roomId: null, notes: '' };

export function EquipmentPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, reload: reloadProject } = useProject();
  const [rows, setRows] = useState<Equipment[]>([]);
  const [rooms, setRooms] = useState<Room[]>([]);
  const [library, setLibrary] = useState<LibraryItem[]>([]);
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');
  const name = (n: { ru: string; en: string; he: string }) => pickName(n, i18n.language).value;

  const load = useCallback(() => {
    if (!token) return;
    api
      .equipment(token, projectId)
      .then(setRows)
      .catch((e: Error) => setError(e.message));
    api
      .rooms(token, projectId)
      .then(setRooms)
      .catch(() => undefined);
  }, [token, projectId]);
  useEffect(load, [load]);

  useEffect(() => {
    if (token && edit && !library.length)
      api
        .library(token)
        .then(setLibrary)
        .catch(() => undefined);
  }, [token, edit, library.length]);

  const roomName = useMemo(() => new Map(rooms.map((r) => [r.id, pickName(r.name, i18n.language).value])), [rooms, i18n.language]);

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
      load();
      reloadProject();
    } catch (e) {
      setError(t(`equipment.errors.${(e as Error).message}`, { defaultValue: (e as Error).message }));
    }
  };

  const save = () =>
    run(async () => {
      if (!token || !edit) return;
      if (!edit.data.tag.trim()) throw new Error('tag_required');
      if (edit.id) await api.updateEquipment(token, projectId, edit.id, edit.data);
      else await api.createEquipment(token, projectId, edit.data);
      setEdit(null);
    });

  const upgrade = (e: Equipment) =>
    run(async () => {
      if (!token) return;
      const reason = await confirm({
        title: t('equipment.upgrade'),
        message: t('equipment.upgradeConfirm', { tag: e.tag, from: e.libraryVersion, to: e.libraryLatestVersion }),
        confirmText: t('equipment.upgrade'),
        reason: 'optional',
      });
      if (reason === null) return;
      await api.updateEquipmentFromLibrary(token, projectId, e.id, reason || undefined);
    });

  const remove = (e: Equipment) =>
    run(async () => {
      if (!token) return;
      const reason = await confirm({
        title: t('dialog.deleteTitle'),
        message: t('equipment.confirmDelete', { tag: e.tag }),
        confirmText: t('common.delete'),
        danger: true,
        reason: 'optional',
      });
      if (reason === null) return;
      await api.deleteEquipment(token, projectId, e.id, reason || undefined);
    });

  const pickLibrary = (id: string) => {
    if (!edit) return;
    const item = library.find((l) => l.id === id);
    const tag = edit.data.tag || item?.code || '';
    setEdit({ ...edit, data: { ...edit.data, libraryItemId: id || null, tag } });
  };

  return (
    <div className="page">
      <div className="page-header">
        <h1>
          {t('projectNav.equipment')} <span className="count-pill">{rows.length}</span>
        </h1>
        {canEdit && (
          <button type="button" className="btn btn-primary" onClick={() => setEdit({ id: null, data: { ...emptyEq } })}>
            + {t('equipment.add')}
          </button>
        )}
      </div>
      <p className="muted">
        {t('equipment.hint')} <Link to="/library">{t('nav.library')}</Link>
      </p>
      {error && <div className="error-banner">{error}</div>}

      <div className="card table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('equipment.tag')}</th>
              <th>{t('equipment.item')}</th>
              <th>{t('library.category')}</th>
              <th>{t('equipment.room')}</th>
              <th>{t('equipment.qty')}</th>
              <th>{t('library.version')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rows.length === 0 && (
              <tr>
                <td colSpan={7} className="muted">
                  {t('equipment.empty')}
                </td>
              </tr>
            )}
            {rows.map((e) => {
              const outdated = e.libraryLatestVersion !== null && e.libraryLatestVersion > e.libraryVersion;
              return (
                <tr key={e.id}>
                  <td className="ltr-value">
                    <b>{e.tag}</b>
                  </td>
                  <td>
                    {e.snapshot ? (
                      <>
                        <BidiText>{name(e.snapshot.name)}</BidiText>
                        <div className="muted small ltr-value">{[e.snapshot.code, e.snapshot.manufacturer, e.snapshot.model].filter(Boolean).join(' · ')}</div>
                      </>
                    ) : (
                      <span className="muted">{t('equipment.noLibrary')}</span>
                    )}
                    {e.notes && <div className="muted small">{e.notes}</div>}
                  </td>
                  <td>{e.snapshot && t(`library.categories.${e.snapshot.category}`)}</td>
                  <td>{e.roomId ? <BidiText>{roomName.get(e.roomId)}</BidiText> : ''}</td>
                  <td className="ltr-value">{e.quantity}</td>
                  <td>
                    {e.libraryItemId && <span className="ltr-value">v{e.libraryVersion}</span>}
                    {outdated && (
                      <span className="badge badge--warn" title={t('equipment.outdatedHint')}>
                        {t('equipment.newVersion', { v: e.libraryLatestVersion })}
                      </span>
                    )}
                  </td>
                  <td className="row-actions">
                    {canEdit && outdated && (
                      <button type="button" className="btn btn-small btn-primary" onClick={() => upgrade(e)}>
                        {t('equipment.upgrade')}
                      </button>
                    )}
                    {canEdit && (
                      <>
                        <button
                          type="button"
                          className="btn btn-small btn-ghost-inline"
                          onClick={() =>
                            setEdit({ id: e.id, data: { libraryItemId: e.libraryItemId, tag: e.tag, quantity: e.quantity, roomId: e.roomId, notes: e.notes } })
                          }
                        >
                          {t('common.edit')}
                        </button>
                        <button type="button" className="btn btn-small btn-danger" onClick={() => remove(e)}>
                          {t('common.delete')}
                        </button>
                      </>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {edit && (
        <Modal sizeKey="equipment" className="modal-wide" onClose={() => setEdit(null)}>
          <h2>{edit.id ? t('common.edit') : t('equipment.add')}</h2>
          <div className="form-grid form-grid--wide">
            <label>
              {t('equipment.item')}
              <select value={edit.data.libraryItemId ?? ''} disabled={!!edit.id} onChange={(e) => pickLibrary(e.target.value)}>
                <option value="">— {t('equipment.chooseLibrary')} —</option>
                {libraryCategories
                  .filter((c) => c !== 'module')
                  .map((c) => {
                    const items = library.filter((l) => l.category === c);
                    return items.length ? (
                      <optgroup key={c} label={t(`library.categories.${c}`)}>
                        {items.map((l) => (
                          <option key={l.id} value={l.id}>
                            {l.code} — {name(l.name)}
                          </option>
                        ))}
                      </optgroup>
                    ) : null;
                  })}
              </select>
              {edit.id && <span className="field-hint">{t('equipment.itemLocked')}</span>}
            </label>
            <div className="form-grid-2">
              <label>
                {t('equipment.tag')}
                <input dir="ltr" value={edit.data.tag} onChange={(e) => setEdit({ ...edit, data: { ...edit.data, tag: e.target.value } })} />
              </label>
              <label>
                {t('equipment.qty')}
                <input
                  type="number"
                  min={1}
                  dir="ltr"
                  value={edit.data.quantity}
                  onChange={(e) => setEdit({ ...edit, data: { ...edit.data, quantity: Number(e.target.value) || 1 } })}
                />
              </label>
              <label>
                {t('equipment.room')}
                <select value={edit.data.roomId ?? ''} onChange={(e) => setEdit({ ...edit, data: { ...edit.data, roomId: e.target.value || null } })}>
                  <option value="">—</option>
                  {rooms.map((r) => (
                    <option key={r.id} value={r.id}>
                      {name(r.name)}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            <label>
              {t('equipment.notes')}
              <input value={edit.data.notes} onChange={(e) => setEdit({ ...edit, data: { ...edit.data, notes: e.target.value } })} {...bidiAutoInput()} />
            </label>
            {edit.id && (
              <label>
                {t('common.reason')}
                <input
                  value={edit.data.reason ?? ''}
                  onChange={(e) => setEdit({ ...edit, data: { ...edit.data, reason: e.target.value } })}
                  {...bidiAutoInput()}
                />
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
