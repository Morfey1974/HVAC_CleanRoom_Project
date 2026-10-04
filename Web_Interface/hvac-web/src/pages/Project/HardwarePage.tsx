import { Fragment, useCallback, useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, moduleLines, type LibraryItem, type ModuleKind, type ProjectModule, type ProjectModuleSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';
import { ChannelSettings } from '../../components/ChannelSettings';
import { HUB_PORTS, HUB_TYPE, RAIL_MAX, SERIAL_RE, WAGON_MAX, moduleIdOf, systemNameOf } from '../../lib/modules';

type Edit = { id: string | null; data: ProjectModuleSave };

const emptyModule: ProjectModuleSave = {
  libraryItemId: '',
  line: 1,
  rail: 0,
  place: 0,
  userName: { ru: '', en: '', he: '' },
  expectedSerial: '',
  revision: '',
  notes: '',
};

const isHub = (m: ProjectModule) => m.snapshot?.typeCode === HUB_TYPE;

export function HardwarePage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, reload: reloadProject } = useProject();
  const confirm = useConfirm();
  const [rows, setRows] = useState<ProjectModule[]>([]);
  const [library, setLibrary] = useState<LibraryItem[]>([]);
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');
  const [formError, setFormError] = useState('');
  const canEdit = hasRole('Engineer');
  const name = (n: { ru: string; en: string; he: string }) => pickName(n, i18n.language).value;
  const errText = (m: string) => t(`hardware.errors.${m}`, { defaultValue: m });

  const load = useCallback(() => {
    if (!token) return;
    api
      .modules(token, projectId)
      .then(setRows)
      .catch((e: Error) => setError(e.message));
  }, [token, projectId]);
  useEffect(load, [load]);

  useEffect(() => {
    if (token)
      api
        .library(token, 'module')
        .then(setLibrary)
        .catch(() => undefined);
  }, [token]);

  const others = useMemo(() => rows.filter((r) => r.id !== edit?.id), [rows, edit?.id]);
  const heads = useMemo(() => others.filter((r) => r.kind === 'head'), [others]);
  const placeable = useMemo(() => library.filter((l) => l.kind && l.kind !== 'plugin'), [library]);
  const item = edit ? library.find((l) => l.id === edit.data.libraryItemId) : undefined;
  const kind: ModuleKind | null = item?.kind ?? null;
  const railHeads = kind === 'display' ? heads.filter(isHub) : heads.filter((h) => !isHub(h));

  const freeRail = () => {
    const used = new Set(heads.map((h) => h.rail));
    for (let r = 1; r <= RAIL_MAX; r++) if (!used.has(r)) return r;
    return 0;
  };
  const freePlace = (rail: number, max: number) => {
    const used = new Set(others.filter((o) => o.rail === rail && o.place > 0).map((o) => o.place));
    for (let p = 1; p <= max; p++) if (!used.has(p)) return p;
    return 0;
  };

  const pickLibrary = (id: string) => {
    if (!edit) return;
    const it = library.find((l) => l.id === id);
    const data = { ...edit.data, libraryItemId: id, channels: [] };
    if (it?.kind === 'head') {
      Object.assign(data, { line: it.typeCode === HUB_TYPE ? 2 : 1, rail: freeRail(), place: 0 });
    } else if (it?.kind === 'wagon' || it?.kind === 'display') {
      const candidates = it.kind === 'display' ? heads.filter(isHub) : heads.filter((h) => !isHub(h));
      const head = candidates.find((h) => h.rail === data.rail) ?? candidates[0];
      const max = it.kind === 'display' ? HUB_PORTS : WAGON_MAX;
      Object.assign(data, head ? { line: head.line, rail: head.rail, place: freePlace(head.rail, max) } : { rail: 0, place: 0 });
    } else {
      Object.assign(data, { line: 0, rail: 0, place: 0 });
    }
    setFormError('');
    setEdit({ ...edit, data });
  };

  const pickRail = (rail: number) => {
    if (!edit) return;
    const head = heads.find((h) => h.rail === rail);
    const max = kind === 'display' ? HUB_PORTS : WAGON_MAX;
    setEdit({ ...edit, data: { ...edit.data, rail, line: head?.line ?? edit.data.line, place: freePlace(rail, max) } });
  };

  const set = (patch: Partial<ProjectModuleSave>) => edit && setEdit({ ...edit, data: { ...edit.data, ...patch } });

  const save = async () => {
    if (!token || !edit) return;
    const d = { ...edit.data, expectedSerial: edit.data.expectedSerial.trim() };
    try {
      if (!d.libraryItemId) throw new Error('choose_module');
      if (d.expectedSerial && !SERIAL_RE.test(d.expectedSerial)) throw new Error('serial_format');
      if (edit.id) await api.updateModule(token, projectId, edit.id, d);
      else await api.createModule(token, projectId, d);
      setEdit(null);
      load();
      reloadProject();
    } catch (e) {
      setFormError(errText((e as Error).message));
    }
  };

  const remove = async (m: ProjectModule) => {
    if (!token) return;
    const reason = await confirm({
      title: t('dialog.deleteTitle'),
      message: t('hardware.confirmDelete', { name: m.systemName, id: m.moduleId || '—' }),
      confirmText: t('common.delete'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return;
    try {
      setError('');
      await api.deleteModule(token, projectId, m.id, reason || undefined);
      load();
      reloadProject();
    } catch (e) {
      setError(errText((e as Error).message));
    }
  };

  const openEdit = (m: ProjectModule) => {
    setFormError('');
    setEdit({
      id: m.id,
      data: {
        libraryItemId: m.libraryItemId ?? '',
        line: m.line,
        rail: m.rail,
        place: m.place,
        userName: { ...m.userName },
        expectedSerial: m.expectedSerial,
        revision: m.revision,
        notes: m.notes,
        channels: m.channels.map((c) => ({ ...c })),
        reason: '',
      },
    });
  };

  // Channel modes come from the module's own copy of the library item; a newly chosen item uses the library.
  const editedRow = edit?.id ? rows.find((r) => r.id === edit.id) : undefined;
  const modeSource = editedRow && editedRow.libraryItemId === edit?.data.libraryItemId ? editedRow.snapshot : item;
  const channelModes = modeSource?.channelModes ?? [];
  const channelCount = modeSource?.channelCount ?? 0;

  const groups = useMemo(() => {
    const out: { key: string; title: string; items: ProjectModule[] }[] = [];
    const plc = rows.filter((r) => r.kind === 'plc');
    if (plc.length) out.push({ key: 'plc', title: t('hardware.groupPlc'), items: plc });
    const rails = [...new Set(rows.filter((r) => r.rail > 0).map((r) => r.rail))].sort((a, b) => a - b);
    for (const rail of rails) {
      const items = rows.filter((r) => r.rail === rail && r.kind !== 'plc' && r.kind !== 'noid');
      const head = items.find((r) => r.kind === 'head');
      const line = head?.line ?? items[0]?.line ?? 0;
      out.push({
        key: `r${rail}`,
        title: t('hardware.groupRail', { rail: String(rail).padStart(2, '0'), line: t(`hardware.lines.${line}`, { defaultValue: String(line) }) }),
        items,
      });
    }
    const noid = rows.filter((r) => r.kind === 'noid');
    if (noid.length) out.push({ key: 'noid', title: t('hardware.groupNoId'), items: noid });
    const unplaced = rows.filter((r) => !r.placed);
    if (unplaced.length) out.push({ key: 'unplaced', title: t('hardware.groupUnplaced'), items: unplaced });
    return out;
  }, [rows, t]);

  const previewId = kind ? moduleIdOf(kind, edit!.data.line, edit!.data.rail, edit!.data.place) : '';
  const previewName = kind && item ? systemNameOf(kind, item.systemPrefix ?? '?', edit!.data.rail, edit!.data.place) : '';

  return (
    <div className="page">
      <div className="page-header">
        <h1>
          {t('projectNav.hardware')} <span className="count-pill">{rows.length}</span>
        </h1>
        {canEdit && (
          <button
            type="button"
            className="btn btn-primary"
            onClick={() => {
              setFormError('');
              setEdit({ id: null, data: { ...emptyModule, userName: { ru: '', en: '', he: '' } } });
            }}
          >
            + {t('hardware.add')}
          </button>
        )}
      </div>
      <p className="muted">
        {t('hardware.hint')} <Link to="/library/module">{t('nav.library')}</Link>
      </p>
      {error && <div className="error-banner">{error}</div>}

      <div className="card table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('hardware.moduleId')}</th>
              <th>{t('hardware.systemName')}</th>
              <th>{t('library.code')}</th>
              <th>{t('hardware.userName')}</th>
              <th>{t('hardware.serial')}</th>
              <th>{t('hardware.revision')}</th>
              <th>{t('library.version')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {rows.length === 0 && (
              <tr>
                <td colSpan={8} className="muted">
                  {t('hardware.empty')}
                </td>
              </tr>
            )}
            {groups.map((g) => (
              <Fragment key={g.key}>
                <tr className="group-row">
                  <td colSpan={8}>{g.title}</td>
                </tr>
                {g.items.map((m) => {
                  const outdated = m.libraryLatestVersion !== null && m.libraryLatestVersion > m.libraryVersion;
                  return (
                    <tr key={m.id}>
                      <td className="ltr-value mono">{m.moduleId || '—'}</td>
                      <td className="ltr-value">
                        <b>{m.systemName}</b>
                        {(m.snapshot?.channelModes?.length ?? 0) > 0 && (
                          <div className="small">
                            {Array.from({ length: m.snapshot?.channelCount ?? 0 }, (_, i) => i + 1).map((n) => {
                              const mode = m.channels.find((c) => c.channel === n)?.mode;
                              return (
                                <span key={n} className={mode ? 'channel-chip' : 'channel-chip channel-chip--missing'}>
                                  {n}: {mode ? t(`channels.modes.${mode}`, { defaultValue: mode }) : t('channels.notSet')}
                                </span>
                              );
                            })}
                          </div>
                        )}
                      </td>
                      <td>
                        <span className="ltr-value">{m.snapshot?.code}</span>
                        {m.snapshot && (
                          <div className="muted small">
                            <BidiText>{name(m.snapshot.name)}</BidiText>
                          </div>
                        )}
                      </td>
                      <td>
                        <BidiText>{name(m.userName)}</BidiText>
                        {m.notes && <div className="muted small">{m.notes}</div>}
                      </td>
                      <td className="ltr-value mono">{m.expectedSerial}</td>
                      <td className="ltr-value">{m.revision}</td>
                      <td>
                        {m.libraryItemId && <span className="ltr-value">v{m.libraryVersion}</span>}
                        {outdated && <span className="badge badge--warn">{t('equipment.newVersion', { v: m.libraryLatestVersion })}</span>}
                      </td>
                      <td className="row-actions">
                        {canEdit && (
                          <>
                            <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openEdit(m)}>
                              {t('common.edit')}
                            </button>
                            <button type="button" className="btn btn-small btn-danger" onClick={() => remove(m)}>
                              {t('common.delete')}
                            </button>
                          </>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </Fragment>
            ))}
          </tbody>
        </table>
      </div>

      {edit && (
        <Modal sizeKey="hardware-module" className="modal-wide" onClose={() => setEdit(null)}>
          <h2>{edit.id ? t('common.edit') : t('hardware.add')}</h2>
          {formError && <div className="error-banner">{formError}</div>}
          <div className="form-grid form-grid--wide">
            <label>
              {t('hardware.module')}
              <select value={edit.data.libraryItemId} onChange={(e) => pickLibrary(e.target.value)}>
                <option value="">— {t('hardware.chooseModule')} —</option>
                {placeable.map((l) => (
                  <option key={l.id} value={l.id}>
                    {l.code} — {name(l.name)}
                  </option>
                ))}
              </select>
            </label>

            {kind === 'plc' && <p className="muted">{t('hardware.placePlc')}</p>}
            {kind === 'noid' && <p className="muted">{t('hardware.placeNoId')}</p>}

            {kind === 'head' && (
              <div className="form-grid-2">
                <label>
                  {t('hardware.line')}
                  <select value={edit.data.line} onChange={(e) => set({ line: Number(e.target.value) })}>
                    {moduleLines.map((l) => (
                      <option key={l} value={l}>
                        {l} — {t(`hardware.lines.${l}`)}
                      </option>
                    ))}
                  </select>
                </label>
                <label>
                  {t('hardware.rail')}
                  <input
                    type="number"
                    min={1}
                    max={RAIL_MAX}
                    dir="ltr"
                    value={edit.data.rail || ''}
                    onChange={(e) => set({ rail: Number(e.target.value) || 0 })}
                  />
                  <span className="field-hint">{t('hardware.railHint')}</span>
                </label>
              </div>
            )}

            {(kind === 'wagon' || kind === 'display') &&
              (railHeads.length === 0 ? (
                <div className="warning-banner">{t(kind === 'display' ? 'hardware.errors.display_needs_hub' : 'hardware.errors.no_head')}</div>
              ) : (
                <div className="form-grid-2">
                  <label>
                    {t('hardware.rail')}
                    <select value={edit.data.rail} onChange={(e) => pickRail(Number(e.target.value))}>
                      {!railHeads.some((h) => h.rail === edit.data.rail) && <option value={edit.data.rail}>—</option>}
                      {railHeads.map((h) => (
                        <option key={h.id} value={h.rail}>
                          {String(h.rail).padStart(2, '0')} — {h.systemName}
                          {name(h.userName) ? ` (${name(h.userName)})` : ''}
                        </option>
                      ))}
                    </select>
                  </label>
                  <label>
                    {kind === 'display' ? t('hardware.port') : t('hardware.place')}
                    <input
                      type="number"
                      min={1}
                      max={kind === 'display' ? HUB_PORTS : WAGON_MAX}
                      dir="ltr"
                      value={edit.data.place || ''}
                      onChange={(e) => set({ place: Number(e.target.value) || 0 })}
                    />
                    <span className="field-hint">{kind === 'display' ? t('hardware.portHint') : t('hardware.placeHint')}</span>
                  </label>
                </div>
              ))}

            {kind && (
              <div className="module-preview">
                <div>
                  <span className="muted small">{t('hardware.moduleId')}</span>
                  <b className="ltr-value mono">{previewId || '—'}</b>
                </div>
                <div>
                  <span className="muted small">{t('hardware.systemName')}</span>
                  <b className="ltr-value">{previewName}</b>
                </div>
                {item?.channelCount ? (
                  <div>
                    <span className="muted small">{t('hardware.channelExample')}</span>
                    <b className="ltr-value">{`${previewName}/1 … ${previewName}/${item.channelCount}`}</b>
                  </div>
                ) : null}
              </div>
            )}

            {channelModes.length > 0 && channelCount > 0 && (
              <div>
                <b className="small">{t('channels.title')}</b>
                <ChannelSettings
                  prefix={t('channels.channel')}
                  count={channelCount}
                  modes={channelModes}
                  value={edit.data.channels ?? []}
                  onChange={(channels) => set({ channels })}
                />
                <span className="field-hint">{t('channels.hint')}</span>
              </div>
            )}

            <div className="form-grid-3">
              {(['he', 'ru', 'en'] as const).map((l) => (
                <label key={l}>
                  {t('hardware.userName')} ({l.toUpperCase()})
                  <input
                    value={edit.data.userName[l]}
                    onChange={(e) => set({ userName: { ...edit.data.userName, [l]: e.target.value } })}
                    {...bidiAutoInput()}
                  />
                </label>
              ))}
            </div>
            <div className="form-grid-2">
              <label>
                {t('hardware.serial')}
                <input dir="ltr" placeholder="2641-0017" value={edit.data.expectedSerial} onChange={(e) => set({ expectedSerial: e.target.value })} />
                <span className="field-hint">{t('hardware.serialHint')}</span>
              </label>
              <label>
                {t('hardware.revision')}
                <input dir="ltr" placeholder="A" value={edit.data.revision} onChange={(e) => set({ revision: e.target.value.toUpperCase() })} />
              </label>
            </div>
            <label>
              {t('equipment.notes')}
              <input value={edit.data.notes} onChange={(e) => set({ notes: e.target.value })} {...bidiAutoInput()} />
            </label>
            {edit.id && (
              <label>
                {t('common.reason')}
                <input value={edit.data.reason ?? ''} onChange={(e) => set({ reason: e.target.value })} {...bidiAutoInput()} />
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
