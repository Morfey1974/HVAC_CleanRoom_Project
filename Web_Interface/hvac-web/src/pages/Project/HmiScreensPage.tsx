import { useCallback, useEffect, useMemo, useState } from 'react';
import { useTranslation } from 'react-i18next';
import {
  api,
  type HmiAhuItem,
  type HmiElement,
  type HmiRoomItem,
  type HmiScreen,
  type HmiScreenContent,
  type HmiScreenKind,
  type Room,
} from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useLive } from '../../context/LiveDataContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { AhuView } from '../../hmi/AhuView';
import { RoomTiles } from '../../hmi/RoomTiles';
import { formatSlot, hmiStates, slotsFor, type HmiState } from '../../hmi/arts';
import { DamperWiring } from '../Hmi/HmiLibraryPage';
import { HmiElementPreview } from '../Hmi/HmiLibraryPage';

const uid = () => Math.random().toString(36).slice(2, 10);

export const itemFromElement = (e: HmiElement, bind: Record<string, string> = {}): HmiAhuItem => ({
  uid: uid(),
  elementId: e.id,
  version: e.version,
  code: e.code,
  kind: e.kind,
  art: e.art,
  length: e.length,
  params: { ...e.params },
  name: { ...e.name },
  bind,
});

export function HmiScreensPage() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, project } = useProject();
  const { snapshot } = useLive();
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');
  const [screens, setScreens] = useState<HmiScreen[] | null>(null);
  const [library, setLibrary] = useState<HmiElement[]>([]);
  const [rooms, setRooms] = useState<Room[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [draft, setDraft] = useState<HmiScreen | null>(null);
  const [dirty, setDirty] = useState(false);
  const [selected, setSelected] = useState<string | null>(null);
  const [drafts, setDrafts] = useState(false);
  const [view, setView] = useState<'live' | HmiState>('live');
  const [error, setError] = useState('');

  const values = project?.isActiveOnSite ? snapshot?.values : undefined;
  const tags = useMemo(() => Object.keys(snapshot?.values ?? {}).sort(), [snapshot?.values]);

  const load = useCallback(
    (select?: string) => {
      if (!token) return;
      Promise.all([api.hmiScreens(token, projectId), api.hmiElements(token), api.rooms(token, projectId)])
        .then(([s, l, r]) => {
          setScreens(s);
          setLibrary(l);
          setRooms([...r].sort((a, b) => a.sortOrder - b.sortOrder));
          const pick = s.find((x) => x.id === select) ?? s[0] ?? null;
          setActiveId(pick?.id ?? null);
          setDraft(pick ? structuredClone(pick) : null);
          setDirty(false);
          setSelected(null);
        })
        .catch((e: Error) => setError(e.message));
    },
    [token, projectId],
  );
  useEffect(() => load(), [load]);

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
    } catch (e) {
      const m = (e as Error).message;
      setError(t(`hmi.errors.${m}`, { defaultValue: m }));
    }
  };

  const open = async (s: HmiScreen) => {
    if (dirty && (await confirm({ title: t('hmi.unsavedTitle'), message: t('hmi.unsaved'), confirmText: t('hmi.discard'), reason: 'none' })) === null) return;
    setActiveId(s.id);
    setDraft(structuredClone(s));
    setDirty(false);
    setSelected(null);
  };

  const add = (kind: HmiScreenKind) =>
    run(async () => {
      if (!token) return;
      const n = (screens?.length ?? 0) + 1;
      const name = kind === 'ahu' ? { ru: `Установка ${n}`, en: `Unit ${n}`, he: `יחידה ${n}` } : { ru: 'Комнаты', en: 'Rooms', he: 'חדרים' };
      const content: HmiScreenContent = kind === 'ahu' ? { items: [], flowTag: '' } : { rooms: rooms.map((r) => ({ roomId: r.id, bind: {} })) };
      const s = await api.createHmiScreen(token, projectId, { kind, name, sortOrder: n, content });
      load(s.id);
    });

  const save = () =>
    run(async () => {
      if (!token || !draft) return;
      await api.updateHmiScreen(token, projectId, draft.id, { kind: draft.kind, name: draft.name, sortOrder: draft.sortOrder, content: draft.content });
      load(draft.id);
    });

  const remove = () =>
    run(async () => {
      if (!token || !draft) return;
      const reason = await confirm({
        title: t('common.delete'),
        message: t('hmi.deleteScreen', { name: pickName(draft.name, i18n.language).value }),
        confirmText: t('common.delete'),
        danger: true,
        reason: 'optional',
      });
      if (reason === null) return;
      await api.deleteHmiScreen(token, projectId, draft.id, reason || undefined);
      load();
    });

  const patch = (p: Partial<HmiScreen>) => {
    if (!draft) return;
    setDraft({ ...draft, ...p });
    setDirty(true);
  };
  const setContent = (c: Partial<HmiScreenContent>) => draft && patch({ content: { ...draft.content, ...c } });
  const items = draft?.content.items ?? [];
  const setItems = (next: HmiAhuItem[]) => setContent({ items: next });
  const sel = items.find((i) => i.uid === selected) ?? null;
  const selIdx = sel ? items.indexOf(sel) : -1;
  const selLib = sel ? library.find((e) => e.id === sel.elementId) : undefined;

  const moveItem = (from: number, to: number) => {
    const next = [...items];
    const [x] = next.splice(from, 1);
    next.splice(to, 0, x);
    setItems(next);
  };

  const palette = library.filter((e) => e.group === 'section' && (drafts || e.status === 'approved'));

  if (!screens) return <div className="page"><p className="muted">{t('common.loading')}</p></div>;

  return (
    <div className="page">
      <div className="page-header">
        <h1>{t('projectNav.mnemo')}</h1>
        {canEdit && (
          <div className="row-actions">
            <button type="button" className="btn btn-ghost-inline" onClick={() => add('ahu')}>
              + {t('hmi.addAhu')}
            </button>
            <button type="button" className="btn btn-ghost-inline" onClick={() => add('rooms')}>
              + {t('hmi.addRooms')}
            </button>
          </div>
        )}
      </div>
      <p className="muted">{t('hmi.screensHint')}</p>
      {error && <div className="error-banner">{error}</div>}

      {screens.length === 0 && <div className="card muted">{t('hmi.noScreens')}</div>}
      {screens.length > 0 && (
        <div className="tabs">
          {screens.map((s) => (
            <button key={s.id} type="button" className={s.id === activeId ? 'tab active' : 'tab'} onClick={() => open(s)}>
              <BidiText>{pickName(s.id === draft?.id ? draft.name : s.name, i18n.language).value}</BidiText>
            </button>
          ))}
        </div>
      )}

      {draft && (
        <>
          <div className="card mt">
            <div className="form-grid-3">
              {(['he', 'ru', 'en'] as const).map((l) => (
                <label key={l}>
                  {t(`projects.fields.name_${l}`)}
                  <input disabled={!canEdit} value={draft.name[l]} onChange={(e) => patch({ name: { ...draft.name, [l]: e.target.value } })} {...bidiAutoInput()} />
                </label>
              ))}
            </div>
            {canEdit && (
              <div className="modal-actions">
                <button type="button" className="btn btn-ghost-inline" onClick={remove}>
                  {t('hmi.deleteScreenBtn')}
                </button>
                <button type="button" className="btn btn-primary" disabled={!dirty} onClick={save}>
                  {t('common.save')}
                </button>
              </div>
            )}
          </div>

          {draft.kind === 'ahu' && (
            <>
              {canEdit && (
                <div className="card mt">
                  <div className="props-head">
                    <b>{t('hmi.palette')}</b>
                    <label className="checkbox-row">
                      <input type="checkbox" checked={drafts} onChange={(e) => setDrafts(e.target.checked)} />
                      {t('hmi.showDrafts')}
                    </label>
                  </div>
                  <div className="hmi-palette">
                    {palette.map((e) => (
                      <button key={e.id} type="button" className="hmi-palette__item" title={t('hmi.clickToAdd')} onClick={() => setItems([...items, itemFromElement(e)])}>
                        <HmiElementPreview e={e} state="running" />
                        <span className="small">
                          <BidiText>{pickName(e.name, i18n.language).value}</BidiText>
                          {e.status === 'draft' && <span className="badge badge--warn">{t('hmi.status.draft')}</span>}
                        </span>
                      </button>
                    ))}
                  </div>
                  <span className="field-hint">{t('hmi.paletteHint')}</span>
                </div>
              )}

              <div className="card hmi-host mt">
                <div className="tabs tabs--inline tabs--small">
                  {(['live', ...hmiStates] as const).map((m) => (
                    <button key={m} type="button" className={view === m ? 'tab active' : 'tab'} onClick={() => setView(m)}>
                      {m === 'live' ? t('hmi.liveView') : t(`hmi.states.${m}`)}
                    </button>
                  ))}
                </div>
                {items.length === 0 ? (
                  <p className="muted">{t('hmi.emptyAhu')}</p>
                ) : (
                  <AhuView
                    items={items}
                    values={values}
                    flowTag={draft.content.flowTag}
                    demo={view === 'live' ? undefined : view}
                    selected={selected}
                    onSelect={canEdit ? setSelected : undefined}
                    onMove={canEdit ? moveItem : undefined}
                  />
                )}
              </div>

              {canEdit && (
                <div className="card mt">
                  <label>
                    {t('hmi.flowTag')}
                    <input dir="ltr" list="hmi-tags" value={draft.content.flowTag ?? ''} onChange={(e) => setContent({ flowTag: e.target.value })} />
                    <span className="field-hint">{t('hmi.flowTagHint')}</span>
                  </label>
                </div>
              )}

              {canEdit && sel && (
                <div className="card mt">
                  <div className="props-head">
                    <b>
                      <BidiText>{pickName(sel.name, i18n.language).value}</BidiText>{' '}
                      <span className="muted small ltr-value">
                        {sel.code} · v{sel.version}
                      </span>
                    </b>
                    <div className="row-actions">
                      <button type="button" className="btn btn-small btn-ghost-inline" disabled={selIdx <= 0} onClick={() => moveItem(selIdx, selIdx - 1)}>
                        ←
                      </button>
                      <button type="button" className="btn btn-small btn-ghost-inline" disabled={selIdx >= items.length - 1} onClick={() => moveItem(selIdx, selIdx + 1)}>
                        →
                      </button>
                      <button
                        type="button"
                        className="btn btn-small btn-ghost-inline"
                        onClick={() => {
                          setItems(items.filter((i) => i.uid !== sel.uid));
                          setSelected(null);
                        }}
                      >
                        {t('hmi.removeSection')}
                      </button>
                    </div>
                  </div>
                  {selLib && selLib.version > sel.version && (
                    <div className="warning-banner">
                      {t('hmi.newerInLibrary', { v: selLib.version })}{' '}
                      <button
                        type="button"
                        className="btn btn-small btn-ghost-inline"
                        onClick={() => setItems(items.map((i) => (i.uid === sel.uid ? { ...itemFromElement(selLib, i.bind), uid: i.uid } : i)))}
                      >
                        {t('hmi.updateFromLibrary')}
                      </button>
                    </div>
                  )}
                  {!selLib && <div className="warning-banner">{t('hmi.missingInLibrary')}</div>}
                  <DamperWiring art={sel.art} p={sel.params ?? {}} />
                  <div className="hmi-binds">
                    {slotsFor(sel.art, sel.params ?? {}).length === 0 && <span className="muted small">{t('hmi.noSlots')}</span>}
                    {slotsFor(sel.art, sel.params ?? {}).map((s) => (
                      <label key={s.key}>
                        {t(`hmi.slots.${s.key}`)}
                        <input
                          dir="ltr"
                          list="hmi-tags"
                          value={sel.bind[s.key] ?? ''}
                          onChange={(e) => setItems(items.map((i) => (i.uid === sel.uid ? { ...i, bind: { ...i.bind, [s.key]: e.target.value } } : i)))}
                        />
                        {sel.bind[s.key] && values && <span className="field-hint ltr-value">{formatSlot(values[sel.bind[s.key]], s)}</span>}
                      </label>
                    ))}
                  </div>
                  <span className="field-hint">{t('hmi.bindHint')}</span>
                </div>
              )}
            </>
          )}

          {draft.kind === 'rooms' && (
            <>
              <div className="card hmi-host mt">
                <RoomTiles rooms={rooms} items={draft.content.rooms ?? []} values={values} />
              </div>
              {canEdit && (
                <div className="card table-wrap mt">
                  <table className="data-table">
                    <thead>
                      <tr>
                        <th>{t('hmi.onScreen')}</th>
                        <th>{t('projectNav.rooms')}</th>
                        <th>{t('hmi.slots.t')}</th>
                        <th>{t('hmi.slots.rh')}</th>
                        <th>{t('hmi.slots.dp')}</th>
                      </tr>
                    </thead>
                    <tbody>
                      {rooms.length === 0 && (
                        <tr>
                          <td colSpan={5} className="muted">
                            {t('monitoring.noRooms')}
                          </td>
                        </tr>
                      )}
                      {rooms.map((r) => {
                        const list = draft.content.rooms ?? [];
                        const it = list.find((x) => x.roomId === r.id);
                        const setRoom = (next: HmiRoomItem | null) =>
                          setContent({
                            rooms: next
                              ? it
                                ? list.map((x) => (x.roomId === r.id ? next : x))
                                : [...list, next]
                              : list.filter((x) => x.roomId !== r.id),
                          });
                        return (
                          <tr key={r.id}>
                            <td>
                              <input type="checkbox" checked={!!it} onChange={(e) => setRoom(e.target.checked ? { roomId: r.id, bind: {} } : null)} />
                            </td>
                            <td>
                              <BidiText>{pickName(r.name, i18n.language).value}</BidiText>
                            </td>
                            {(['t', 'rh', 'dp'] as const).map((k) => (
                              <td key={k}>
                                <input
                                  dir="ltr"
                                  list="hmi-tags"
                                  disabled={!it}
                                  value={it?.bind[k] ?? ''}
                                  onChange={(e) => it && setRoom({ ...it, bind: { ...it.bind, [k]: e.target.value } })}
                                />
                              </td>
                            ))}
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                  <span className="field-hint">{t('hmi.bindHint')}</span>
                </div>
              )}
            </>
          )}
        </>
      )}

      <datalist id="hmi-tags">
        {tags.map((k) => (
          <option key={k} value={k} />
        ))}
      </datalist>
    </div>
  );
}
