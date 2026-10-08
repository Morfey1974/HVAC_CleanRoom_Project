import { useCallback, useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, hmiGroups, type HmiDrive, type HmiElement, type HmiElementSave, type HmiGroup, type HmiParams } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatDate, pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';
import { arts, damperControls, damperFeedbacks, damperSupplies, hmiStates, sectionArts, type HmiState } from '../../hmi/arts';
import { AhuView } from '../../hmi/AhuView';
import { ArtPreview } from '../../hmi/RoomTiles';

type Edit = { id: string | null; version: number; used: number; data: HmiElementSave };

/** Picture of a library element: sections in a short casing, the rest on their own. */
export function HmiElementPreview({ e, state }: { e: Pick<HmiElement, 'art' | 'length' | 'params' | 'name'> & { group: HmiGroup }; state: HmiState }) {
  if (e.group === 'section' || sectionArts.includes(e.art))
    return (
      <AhuView
        demo={state}
        showNames={false}
        bare
        items={[{ uid: 'p', elementId: '', version: 0, code: '', kind: e.art, art: e.art, length: e.length, params: e.params, name: e.name, bind: {} }]}
      />
    );
  return (
    <div className="hmi-canvas">
      <ArtPreview art={e.art} length={e.length} params={e.params} state={state} />
    </div>
  );
}

/** Drive and actuator wiring of a damper in one line. */
export function DamperWiring({ art, p }: { art: string; p: HmiParams }) {
  const { t } = useTranslation();
  if (art !== 'damper') return null;
  if (p.drive !== 'motor') return <div className="muted small">{t('hmi.drives.manual')}</div>;
  return (
    <div className="muted small">
      {t('hmi.drives.motor')}: <span className="ltr-value">{p.supply ?? '24V AC/DC'}</span> · {t('hmi.control')} <span className="ltr-value">{p.control ?? '0-10V'}</span> ·{' '}
      {t('hmi.feedback')} <span className="ltr-value">{p.feedback === 'none' ? t('hmi.none') : (p.feedback ?? '0-10V')}</span>
    </div>
  );
}

export function HmiLibraryPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');
  const [archived, setArchived] = useState(false);
  const [items, setItems] = useState<HmiElement[] | null>(null);
  const [state, setState] = useState<HmiState>('running');
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');

  const load = useCallback(() => {
    if (token) api.hmiElements(token, archived).then(setItems).catch((e: Error) => setError(e.message));
  }, [token, archived]);
  useEffect(load, [load]);

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
      load();
    } catch (e) {
      const m = (e as Error).message;
      setError(t(`hmi.errors.${m}`, { defaultValue: m }));
    }
  };

  const openNew = (group: HmiGroup) =>
    setEdit({
      id: null,
      version: 0,
      used: 0,
      data: { code: '', group, kind: '', art: group === 'room' ? 'roomTile' : 'filter', name: { ru: '', en: '', he: '' }, description: '', length: 80, params: {}, status: 'draft' },
    });

  const openEdit = (e: HmiElement) =>
    setEdit({
      id: e.id,
      version: e.version,
      used: e.usedInScreens,
      data: { code: e.code, group: e.group, kind: e.kind, art: e.art, name: { ...e.name }, description: e.description, length: e.length, params: { ...e.params }, status: e.status, reason: '' },
    });

  const save = () =>
    run(async () => {
      if (!token || !edit) return;
      const d = { ...edit.data, code: edit.data.code.trim().toUpperCase(), kind: edit.data.kind.trim() || edit.data.art };
      if (edit.id) await api.updateHmiElement(token, edit.id, d);
      else await api.createHmiElement(token, d);
      setEdit(null);
    });

  const archive = (e: HmiElement) =>
    run(async () => {
      if (!token) return;
      const title = e.isArchived ? t('projects.restore') : t('projects.toArchive');
      const reason = await confirm({
        title,
        message: t(e.isArchived ? 'library.restoreConfirm' : 'library.archiveConfirm', { name: `${e.code} ${pickName(e.name, i18n.language).value}` }),
        confirmText: title,
        danger: !e.isArchived,
        reason: e.isArchived ? 'none' : 'optional',
      });
      if (reason === null) return;
      await api.archiveHmiElement(token, e.id, !e.isArchived, reason || undefined);
    });

  const setData = (patch: Partial<HmiElementSave>) => edit && setEdit({ ...edit, data: { ...edit.data, ...patch } });

  return (
    <div className="page">
      <div className="page-header">
        <h1>{t('nav.hmiLibrary')}</h1>
      </div>
      <p className="muted">{t('hmi.libraryHint')}</p>

      <div className="toolbar">
        <div className="tabs tabs--inline">
          {hmiStates.map((s) => (
            <button key={s} type="button" className={state === s ? 'tab active' : 'tab'} onClick={() => setState(s)}>
              {t(`hmi.states.${s}`)}
            </button>
          ))}
        </div>
        <label className="checkbox-row">
          <input type="checkbox" checked={archived} onChange={(e) => setArchived(e.target.checked)} />
          {t('library.showArchived')}
        </label>
      </div>

      {error && <div className="error-banner">{error}</div>}
      {items === null && <p className="muted">{t('common.loading')}</p>}

      {items &&
        hmiGroups.map((g) => {
          const list = items.filter((i) => i.group === g);
          if (!list.length && (archived || !canEdit)) return null;
          return (
            <section key={g} className="mt">
              <div className="props-head">
                <h2>{t(`hmi.groups.${g}`)}</h2>
                {canEdit && !archived && (
                  <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openNew(g)}>
                    + {t('hmi.addElement')}
                  </button>
                )}
              </div>
              <div className="hmi-cards">
                {list.length === 0 && <div className="card muted">{t('library.empty')}</div>}
                {list.map((e) => (
                  <div key={e.id} className={`card hmi-card ${e.status === 'draft' ? 'hmi-card--draft' : ''}`}>
                    <HmiElementPreview e={e} state={state} />
                    <div className="hmi-card__title">
                      <BidiText>{pickName(e.name, i18n.language).value}</BidiText>
                      <span className={`badge ${e.status === 'draft' ? 'badge--warn' : 'badge--site'}`}>{t(`hmi.status.${e.status}`)}</span>
                    </div>
                    <div className="muted small ltr-value">
                      {e.code} · v{e.version} · {formatDate(e.updatedAt, i18n.language)}
                    </div>
                    <DamperWiring art={e.art} p={e.params} />
                    {e.usedInScreens > 0 && <div className="muted small">{t('hmi.usedIn', { count: e.usedInScreens })}</div>}
                    {canEdit && (
                      <div className="row-actions">
                        {!e.isArchived && (
                          <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openEdit(e)}>
                            {t('common.edit')}
                          </button>
                        )}
                        <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => archive(e)}>
                          {e.isArchived ? t('projects.restore') : t('projects.toArchive')}
                        </button>
                      </div>
                    )}
                  </div>
                ))}
              </div>
            </section>
          );
        })}

      {edit && (
        <Modal sizeKey="hmi-element" className="modal-wide" onClose={() => setEdit(null)}>
          <h2>
            {t('hmi.element')} {edit.id && <span className="count-pill ltr-value">v{edit.version}</span>}
          </h2>
          {edit.id && edit.used > 0 && <div className="warning-banner">{t('hmi.usedWarning', { count: edit.used })}</div>}
          <HmiElementPreview e={{ ...edit.data, params: edit.data.params }} state={state} />
          <div className="form-grid form-grid--wide mt">
            <div className="form-grid-3">
              {(['he', 'ru', 'en'] as const).map((l) => (
                <label key={l} className={edit.data.name[l].trim() ? '' : 'field-missing'}>
                  {t(`projects.fields.name_${l}`)}
                  <input value={edit.data.name[l]} onChange={(e) => setData({ name: { ...edit.data.name, [l]: e.target.value } })} {...bidiAutoInput()} />
                </label>
              ))}
            </div>
            <div className="form-grid-3">
              <label>
                {t('library.code')}
                <input dir="ltr" placeholder="HMI-…" value={edit.data.code} onChange={(e) => setData({ code: e.target.value.toUpperCase() })} />
                <span className="field-hint">{t('hmi.codeHint')}</span>
              </label>
              <label>
                {t('hmi.group')}
                <select value={edit.data.group} onChange={(e) => setData({ group: e.target.value as HmiGroup })}>
                  {hmiGroups.map((g) => (
                    <option key={g} value={g}>
                      {t(`hmi.groups.${g}`)}
                    </option>
                  ))}
                </select>
              </label>
              <label>
                {t('hmi.art')}
                <select value={edit.data.art} onChange={(e) => setData({ art: e.target.value })}>
                  {arts.map((a) => (
                    <option key={a} value={a}>
                      {t(`hmi.arts.${a}`)}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            <div className="form-grid-3">
              <label>
                {t('hmi.length')}
                <input type="number" dir="ltr" min={20} max={600} value={edit.data.length} onChange={(e) => setData({ length: e.target.valueAsNumber || 0 })} />
                <span className="field-hint">{t('hmi.lengthHint')}</span>
              </label>
              <label>
                {t('hmi.label')}
                <input dir="ltr" value={edit.data.params.label ?? ''} onChange={(e) => setData({ params: { ...edit.data.params, label: e.target.value || undefined } })} />
              </label>
              {edit.data.art === 'heater' ? (
                <label>
                  {t('hmi.stages')}
                  <input
                    type="number"
                    dir="ltr"
                    min={1}
                    max={6}
                    value={edit.data.params.stages ?? 3}
                    onChange={(e) => setData({ params: { ...edit.data.params, stages: e.target.valueAsNumber || 1 } })}
                  />
                </label>
              ) : (
                <label>
                  {t('hmi.kind')}
                  <input dir="ltr" placeholder={edit.data.art} value={edit.data.kind} onChange={(e) => setData({ kind: e.target.value })} />
                  <span className="field-hint">{t('hmi.kindHint')}</span>
                </label>
              )}
            </div>
            {edit.data.art === 'damper' && (
              <div className="form-grid-3">
                <label>
                  {t('hmi.drive')}
                  <select
                    value={edit.data.params.drive ?? 'manual'}
                    onChange={(e) => {
                      const drive = e.target.value as HmiDrive;
                      setData({
                        params:
                          drive === 'motor'
                            ? { ...edit.data.params, drive, supply: edit.data.params.supply ?? '24V AC/DC', control: edit.data.params.control ?? '0-10V', feedback: edit.data.params.feedback ?? '0-10V' }
                            : { label: edit.data.params.label, drive },
                      });
                    }}
                  >
                    <option value="manual">{t('hmi.drives.manual')}</option>
                    <option value="motor">{t('hmi.drives.motor')}</option>
                  </select>
                </label>
                {edit.data.params.drive === 'motor' && (
                  <>
                    <label>
                      {t('hmi.supply')}
                      <select dir="ltr" value={edit.data.params.supply} onChange={(e) => setData({ params: { ...edit.data.params, supply: e.target.value } })}>
                        {damperSupplies.map((s) => (
                          <option key={s}>{s}</option>
                        ))}
                      </select>
                    </label>
                    <div className="form-grid-2">
                      <label>
                        {t('hmi.control')}
                        <select dir="ltr" value={edit.data.params.control} onChange={(e) => setData({ params: { ...edit.data.params, control: e.target.value } })}>
                          {damperControls.map((s) => (
                            <option key={s}>{s}</option>
                          ))}
                        </select>
                      </label>
                      <label>
                        {t('hmi.feedback')}
                        <select value={edit.data.params.feedback} onChange={(e) => setData({ params: { ...edit.data.params, feedback: e.target.value } })}>
                          {damperFeedbacks.map((s) => (
                            <option key={s} value={s}>
                              {s === 'none' ? t('hmi.none') : s}
                            </option>
                          ))}
                        </select>
                      </label>
                    </div>
                  </>
                )}
                <span className="field-hint">{t('hmi.driveHint')}</span>
              </div>
            )}
            <label>
              {t('hmi.statusLabel')}
              <select value={edit.data.status} onChange={(e) => setData({ status: e.target.value as HmiElementSave['status'] })}>
                <option value="draft">{t('hmi.status.draft')}</option>
                <option value="approved">{t('hmi.status.approved')}</option>
              </select>
              <span className="field-hint">{t('hmi.statusHint')}</span>
            </label>
            <label>
              {t('projects.fields.description')}
              <textarea rows={2} value={edit.data.description} onChange={(e) => setData({ description: e.target.value })} {...bidiAutoInput()} />
            </label>
            {edit.id && (
              <label>
                {t('common.reason')}
                <input value={edit.data.reason ?? ''} onChange={(e) => setData({ reason: e.target.value })} {...bidiAutoInput()} />
                <span className="field-hint">{t('library.versionHint')}</span>
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
