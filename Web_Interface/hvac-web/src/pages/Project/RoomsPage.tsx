import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type Room, type RoomSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';

const emptyRoom: RoomSave = {
  name: { ru: '', en: '', he: '' },
  isoClass: null,
  areaM2: null,
  heightM: null,
  tempSetpointC: null,
  tempToleranceC: null,
  rhSetpointPct: null,
  rhTolerancePct: null,
  pressureSetpointPa: null,
  pressureTolerancePa: null,
  sortOrder: 0,
  reason: '',
};

type NumKey =
  'isoClass' | 'areaM2' | 'heightM' | 'tempSetpointC' | 'tempToleranceC' | 'rhSetpointPct' | 'rhTolerancePct' | 'pressureSetpointPa' | 'pressureTolerancePa';
const numFields: { key: NumKey; label: string }[] = [
  { key: 'isoClass', label: 'rooms.iso' },
  { key: 'areaM2', label: 'rooms.area' },
  { key: 'heightM', label: 'rooms.height' },
  { key: 'tempSetpointC', label: 'rooms.tSp' },
  { key: 'tempToleranceC', label: 'rooms.tTol' },
  { key: 'rhSetpointPct', label: 'rooms.rhSp' },
  { key: 'rhTolerancePct', label: 'rooms.rhTol' },
  { key: 'pressureSetpointPa', label: 'rooms.pSp' },
  { key: 'pressureTolerancePa', label: 'rooms.pTol' },
];

const fmt = (v: number | null, unit = '') => (v === null ? '' : `${v}${unit}`);

export function RoomsPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, reload: reloadProject } = useProject();
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');
  const [rooms, setRooms] = useState<Room[]>([]);
  const [edit, setEdit] = useState<{ id: string | null; data: RoomSave } | null>(null);
  const [error, setError] = useState('');

  const load = useCallback(() => {
    if (token)
      api
        .rooms(token, projectId)
        .then(setRooms)
        .catch((e: Error) => setError(e.message));
  }, [token, projectId]);
  useEffect(load, [load]);

  const save = async (e: FormEvent) => {
    e.preventDefault();
    if (!token || !edit) return;
    const n = edit.data.name;
    if (!n.ru.trim() && !n.en.trim() && !n.he.trim()) {
      setError(t('rooms.nameRequired'));
      return;
    }
    try {
      if (edit.id) await api.updateRoom(token, projectId, edit.id, edit.data);
      else {
        await api.createRoom(token, projectId, edit.data);
        reloadProject();
      }
      setEdit(null);
      setError('');
      load();
    } catch (err) {
      setError((err as Error).message);
    }
  };

  const remove = async (r: Room) => {
    if (!token) return;
    const name = pickName(r.name, i18n.language).value;
    const reason = await confirm({
      title: t('dialog.deleteTitle'),
      message: t('rooms.confirmDelete', { name }),
      confirmText: t('common.delete'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return;
    try {
      await api.deleteRoom(token, projectId, r.id, reason || undefined);
      load();
      reloadProject();
    } catch (err) {
      setError((err as Error).message);
    }
  };

  const setName = (lang: 'ru' | 'en' | 'he', v: string) => edit && setEdit({ ...edit, data: { ...edit.data, name: { ...edit.data.name, [lang]: v } } });
  const setNum = (k: NumKey, v: string) => edit && setEdit({ ...edit, data: { ...edit.data, [k]: v === '' ? null : Number(v) } });

  return (
    <div className="card">
      <div className="page-header">
        <h2>
          {t('rooms.title')} <span className="count-pill">{t('rooms.count', { count: rooms.length })}</span>
        </h2>
        {canEdit && (
          <button type="button" className="btn btn-primary" onClick={() => setEdit({ id: null, data: { ...emptyRoom, sortOrder: rooms.length } })}>
            {t('rooms.add')}
          </button>
        )}
      </div>
      {error && <div className="error-banner">{error}</div>}

      <div className="table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('rooms.name')}</th>
              <th>{t('rooms.iso')}</th>
              <th>{t('rooms.area')}</th>
              <th>{t('rooms.tSp')}</th>
              <th>{t('rooms.rhSp')}</th>
              <th>{t('rooms.pSp')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rooms.length === 0 && (
              <tr>
                <td colSpan={7} className="muted">
                  {t('rooms.empty')}
                </td>
              </tr>
            )}
            {rooms.map((r) => {
              const name = pickName(r.name, i18n.language);
              return (
                <tr key={r.id}>
                  <td>
                    <BidiText>{name.value}</BidiText>
                    {name.fallback && <span className="missing-badge">{t('rooms.missingTranslation')}</span>}
                  </td>
                  <td className="ltr-value">{fmt(r.isoClass)}</td>
                  <td className="ltr-value">{fmt(r.areaM2)}</td>
                  <td className="ltr-value">
                    {fmt(r.tempSetpointC)}
                    {r.tempToleranceC !== null && ` ±${r.tempToleranceC}`}
                  </td>
                  <td className="ltr-value">
                    {fmt(r.rhSetpointPct)}
                    {r.rhTolerancePct !== null && ` ±${r.rhTolerancePct}`}
                  </td>
                  <td className="ltr-value">
                    {fmt(r.pressureSetpointPa)}
                    {r.pressureTolerancePa !== null && ` ±${r.pressureTolerancePa}`}
                  </td>
                  <td className="row-actions">
                    {canEdit && (
                      <>
                        <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => setEdit({ id: r.id, data: { ...r, reason: '' } })}>
                          {t('common.edit')}
                        </button>
                        <button type="button" className="btn btn-small btn-danger" onClick={() => remove(r)}>
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
        <Modal sizeKey="room" className="modal-wide" onClose={() => setEdit(null)}>
          <form onSubmit={save}>
            <h2>{t('rooms.editTitle')}</h2>
            <div className="form-grid">
              {(['he', 'ru', 'en'] as const).map((l) => (
                <label key={l} className={edit.data.name[l].trim() ? '' : 'field-missing'}>
                  {t(`rooms.name${l === 'ru' ? 'Ru' : l === 'en' ? 'En' : 'He'}`)}
                  <input value={edit.data.name[l]} onChange={(e) => setName(l, e.target.value)} {...bidiAutoInput()} />
                </label>
              ))}
              <div className="form-grid-2">
                {numFields.map((f) => (
                  <label key={f.key}>
                    {t(f.label)}
                    <input type="number" step="any" dir="ltr" value={edit.data[f.key] ?? ''} onChange={(e) => setNum(f.key, e.target.value)} />
                  </label>
                ))}
              </div>
              {edit.id && (
                <label>
                  {t('common.reason')}
                  <input
                    value={edit.data.reason ?? ''}
                    onChange={(e) => setEdit({ ...edit, data: { ...edit.data, reason: e.target.value } })}
                    {...bidiAutoInput()}
                  />
                  <span className="field-hint">{t('common.reasonHint')}</span>
                </label>
              )}
            </div>
            <div className="modal-actions">
              <button type="button" className="btn btn-ghost-inline" onClick={() => setEdit(null)}>
                {t('common.cancel')}
              </button>
              <button type="submit" className="btn btn-primary">
                {t('common.save')}
              </button>
            </div>
          </form>
        </Modal>
      )}
    </div>
  );
}
