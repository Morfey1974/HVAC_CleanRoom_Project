import { useCallback, useEffect, useMemo, useState } from 'react';
import { NavLink, Navigate, useParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, libraryCategories, type LibraryCategory, type LibraryItem, type LibraryItemSave } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { formatDate, pickName } from '../../lib/localized';
import { BidiText, bidiAutoInput } from '../../components/BidiText';
import { useConfirm } from '../../components/Dialog';
import { Modal } from '../../components/Modal';
import { ARTICLE_RE, SIGNAL_MODES, hex, kindOf, parseHex } from '../../lib/modules';
import { BlockEditor } from '../../components/BlockEditor';
import { ModuleBlock, emptyGraphic } from '../../components/ModuleBlock';

type Tab = 'data' | 'block';
type Edit = { id: string | null; version: number; used: number; typeHex: string; tab: Tab; data: LibraryItemSave };

const isCategory = (v: string | undefined): v is LibraryCategory => libraryCategories.includes(v as LibraryCategory);

export function LibraryPage() {
  const { category } = useParams();
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const [archived, setArchived] = useState(false);
  const [items, setItems] = useState<LibraryItem[] | null>(null);
  const [search, setSearch] = useState('');
  const [edit, setEdit] = useState<Edit | null>(null);
  const [error, setError] = useState('');
  const confirm = useConfirm();
  const canEdit = hasRole('Engineer');
  const cat = isCategory(category) ? category : null;

  const load = useCallback(() => {
    if (token && cat)
      api
        .library(token, cat, archived)
        .then(setItems)
        .catch((e: Error) => setError(e.message));
  }, [token, cat, archived]);
  useEffect(load, [load]);

  const filtered = useMemo(() => {
    const s = search.trim().toLowerCase();
    if (!items || !s) return items ?? [];
    return items.filter((i) => [i.code, i.name.ru, i.name.en, i.name.he, i.manufacturer, i.model].some((v) => v.toLowerCase().includes(s)));
  }, [items, search]);

  if (!cat) return <Navigate to="/library/module" replace />;

  const openNew = () =>
    setEdit({
      id: null,
      version: 0,
      used: 0,
      typeHex: '',
      tab: 'data',
      data: {
        category: cat,
        code: cat === 'module' ? 'HC-' : '',
        name: { ru: '', en: '', he: '' },
        manufacturer: '',
        model: '',
        description: '',
        props: [],
        typeCode: null,
        systemPrefix: null,
        channelCount: null,
        graphic: cat === 'module' ? emptyGraphic() : null,
        channelModes: [],
      },
    });

  const openEdit = (i: LibraryItem) =>
    setEdit({
      id: i.id,
      version: i.version,
      used: i.usedInProjects,
      typeHex: hex(i.typeCode),
      tab: 'data',
      data: {
        category: i.category,
        code: i.code,
        name: { ...i.name },
        manufacturer: i.manufacturer,
        model: i.model,
        description: i.description,
        props: i.props.map((p) => ({ ...p })),
        typeCode: i.typeCode,
        systemPrefix: i.systemPrefix,
        channelCount: i.channelCount,
        graphic: i.graphic ? structuredClone(i.graphic) : i.category === 'module' ? emptyGraphic() : null,
        channelModes: [...(i.channelModes ?? [])],
        reason: '',
      },
    });

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
      load();
    } catch (e) {
      const m = (e as Error).message;
      setError(t(`library.errors.${m}`, { defaultValue: m }));
    }
  };

  const save = () =>
    run(async () => {
      if (!token || !edit) return;
      const d = { ...edit.data, code: edit.data.code.trim().toUpperCase() };
      if (!d.code && !d.name.ru.trim() && !d.name.en.trim() && !d.name.he.trim()) throw new Error('name_required');
      if (d.category === 'module') {
        const tc = parseHex(edit.typeHex);
        if (tc === null) throw new Error('type_code_required');
        if (Number.isNaN(tc)) throw new Error('type_code_format');
        if (!ARTICLE_RE.test(d.code)) throw new Error('article_format');
        d.typeCode = tc;
        d.systemPrefix = (d.systemPrefix ?? '').trim().toUpperCase();
      } else {
        d.typeCode = null;
        d.systemPrefix = null;
        d.channelCount = null;
        d.graphic = null;
        d.channelModes = [];
      }
      if (edit.id) await api.updateLibraryItem(token, edit.id, d);
      else await api.createLibraryItem(token, d);
      setEdit(null);
    });

  const archive = (i: LibraryItem) =>
    run(async () => {
      if (!token) return;
      const title = i.isArchived ? t('projects.restore') : t('projects.toArchive');
      const reason = await confirm({
        title,
        message: t(i.isArchived ? 'library.restoreConfirm' : 'library.archiveConfirm', { name: `${i.code} ${pickName(i.name, i18n.language).value}`.trim() }),
        confirmText: title,
        danger: !i.isArchived,
        reason: i.isArchived ? 'none' : 'optional',
      });
      if (reason === null) return;
      await api.archiveLibraryItem(token, i.id, !i.isArchived, reason || undefined);
    });

  const editKind = edit ? kindOf(parseHex(edit.typeHex)) : null;

  const setData = (patch: Partial<LibraryItemSave>) => edit && setEdit({ ...edit, data: { ...edit.data, ...patch } });
  const setProp = (idx: number, key: 'key' | 'value' | 'unit', v: string) =>
    edit && setData({ props: edit.data.props.map((p, i) => (i === idx ? { ...p, [key]: v } : p)) });

  return (
    <div className="page">
      <div className="page-header">
        <h1>{t('nav.library')}</h1>
        {canEdit && !archived && (
          <button type="button" className="btn btn-primary" onClick={openNew}>
            + {t('library.add')}
          </button>
        )}
      </div>
      <p className="muted">{t('library.hint')}</p>

      <div className="tabs">
        {libraryCategories.map((c) => (
          <NavLink key={c} to={`/library/${c}`} className={({ isActive }) => (isActive ? 'tab active' : 'tab')}>
            {t(`library.categories.${c}`)}
          </NavLink>
        ))}
      </div>

      <div className="toolbar mt">
        <label className="checkbox-row">
          <input type="checkbox" checked={archived} onChange={(e) => setArchived(e.target.checked)} />
          {t('library.showArchived')}
        </label>
        <input placeholder={t('common.search')} value={search} onChange={(e) => setSearch(e.target.value)} {...bidiAutoInput('search-input')} />
      </div>

      {error && <div className="error-banner">{error}</div>}

      <div className="card table-wrap">
        <table className="data-table">
          <thead>
            <tr>
              <th>{t('library.code')}</th>
              {cat === 'module' && <th>{t('library.typeCode')}</th>}
              {cat === 'module' && <th>{t('library.systemPrefix')}</th>}
              <th>{t('library.name')}</th>
              <th>{t('library.manufacturer')}</th>
              <th>{t('library.props')}</th>
              <th>{t('library.version')}</th>
              <th>{t('library.used')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {items !== null && filtered.length === 0 && (
              <tr>
                <td colSpan={cat === 'module' ? 9 : 7} className="muted">
                  {t('library.empty')}
                </td>
              </tr>
            )}
            {filtered.map((i) => {
              const n = pickName(i.name, i18n.language);
              return (
                <tr key={i.id}>
                  <td className="ltr-value nowrap">
                    <b>{i.code}</b>
                    {i.graphic && (
                      <div className="library-thumb">
                        <ModuleBlock graphic={i.graphic} article={i.code} scale={0.32} />
                      </div>
                    )}
                  </td>
                  {cat === 'module' && (
                    <td>
                      <span className="ltr-value">{hex(i.typeCode)}</span>
                      {i.kind && <div className="muted small">{t(`hardware.kinds.${i.kind}`)}</div>}
                    </td>
                  )}
                  {cat === 'module' && (
                    <td className="ltr-value">
                      {i.systemPrefix}
                      {i.channelCount ? <div className="muted small">{t('library.channelsN', { count: i.channelCount })}</div> : null}
                    </td>
                  )}
                  <td>
                    <BidiText>{n.value}</BidiText>
                    {n.fallback && n.value && <span className="missing-badge">{t('rooms.missingTranslation')}</span>}
                  </td>
                  <td>
                    <BidiText>{[i.manufacturer, i.model].filter(Boolean).join(' · ')}</BidiText>
                  </td>
                  <td className="small">
                    {i.props.slice(0, 3).map((p) => (
                      <div key={p.key}>
                        <BidiText>{p.key}</BidiText>: <span className="ltr-value">{`${p.value} ${p.unit}`.trim()}</span>
                      </div>
                    ))}
                    {i.props.length > 3 && <span className="muted">+{i.props.length - 3}</span>}
                  </td>
                  <td>
                    <span className="ltr-value">v{i.version}</span>
                    <div className="muted small ltr-value">{formatDate(i.updatedAt, i18n.language)}</div>
                  </td>
                  <td className="ltr-value">{i.usedInProjects || ''}</td>
                  <td className="row-actions">
                    {canEdit && !i.isArchived && (
                      <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => openEdit(i)}>
                        {t('common.edit')}
                      </button>
                    )}
                    {canEdit && (
                      <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => archive(i)}>
                        {i.isArchived ? t('projects.restore') : t('projects.toArchive')}
                      </button>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {edit && (
        <Modal
          sizeKey={edit.data.category === 'module' ? 'library-module' : 'library-item'}
          className={edit.data.category === 'module' ? 'modal-xl' : 'modal-wide'}
          onClose={() => setEdit(null)}
        >
          <h2>
            {t(`library.categories.${edit.data.category}`)} {edit.id && <span className="count-pill ltr-value">v{edit.version}</span>}
          </h2>
          {edit.id && edit.used > 0 && <div className="warning-banner">{t('library.usedWarning', { count: edit.used })}</div>}
          {edit.data.category === 'module' && (
            <div className="tabs tabs--inline">
              {(['data', 'block'] as const).map((tab) => (
                <button key={tab} type="button" className={edit.tab === tab ? 'tab active' : 'tab'} onClick={() => setEdit({ ...edit, tab })}>
                  {t(`library.tabs.${tab}`)}
                </button>
              ))}
            </div>
          )}
          {edit.data.category === 'module' && edit.tab === 'block' && edit.data.graphic && (
            <BlockEditor
              graphic={edit.data.graphic}
              article={edit.data.code}
              name={pickName(edit.data.name, i18n.language).value}
              onChange={(graphic) => setEdit((e) => (e ? { ...e, data: { ...e.data, graphic } } : e))}
            />
          )}
          <div className="form-grid form-grid--wide" hidden={edit.data.category === 'module' && edit.tab === 'block'}>
            <div className="form-grid-2">
              <label>
                {t('library.code')}
                <input dir="ltr" value={edit.data.code} onChange={(e) => setData({ code: e.target.value.toUpperCase() })} />
                {edit.data.category === 'module' && <span className="field-hint">{t('library.articleHint')}</span>}
              </label>
              <label>
                {t('library.category')}
                <select value={edit.data.category} onChange={(e) => setData({ category: e.target.value as LibraryCategory })}>
                  {libraryCategories.map((c) => (
                    <option key={c} value={c}>
                      {t(`library.categories.${c}`)}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            {edit.data.category === 'module' && (
              <div className="form-grid-3">
                <label>
                  {t('library.typeCode')}
                  <input dir="ltr" placeholder="0x20" value={edit.typeHex} onChange={(e) => setEdit({ ...edit, typeHex: e.target.value })} />
                  <span className="field-hint">{editKind ? t(`hardware.kinds.${editKind}`) : t('library.typeCodeHint')}</span>
                </label>
                <label>
                  {t('library.systemPrefix')}
                  <input
                    dir="ltr"
                    placeholder="AI"
                    value={edit.data.systemPrefix ?? ''}
                    onChange={(e) => setData({ systemPrefix: e.target.value.toUpperCase() })}
                  />
                  <span className="field-hint">{t('library.systemPrefixHint')}</span>
                </label>
                <label>
                  {t('library.channels')}
                  <input
                    type="number"
                    min={0}
                    max={64}
                    dir="ltr"
                    value={edit.data.channelCount ?? ''}
                    onChange={(e) => setData({ channelCount: e.target.value === '' ? null : Number(e.target.value) })}
                  />
                </label>
              </div>
            )}
            {edit.data.category === 'module' && (
              <div>
                <b className="small">{t('channels.libraryModes')}</b>
                <div className="checkbox-group">
                  {SIGNAL_MODES.map((m) => (
                    <label key={m} className="checkbox-row">
                      <input
                        type="checkbox"
                        checked={(edit.data.channelModes ?? []).includes(m)}
                        onChange={(e) => {
                          const cur = edit.data.channelModes ?? [];
                          setData({ channelModes: e.target.checked ? SIGNAL_MODES.filter((x) => x === m || cur.includes(x)) : cur.filter((x) => x !== m) });
                        }}
                      />
                      {t(`channels.modes.${m}`)}
                    </label>
                  ))}
                </div>
                <span className="field-hint">{t('channels.libraryModesHint')}</span>
              </div>
            )}
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
                {t('library.manufacturer')}
                <input value={edit.data.manufacturer} onChange={(e) => setData({ manufacturer: e.target.value })} {...bidiAutoInput()} />
              </label>
              <label>
                {t('library.model')}
                <input value={edit.data.model} onChange={(e) => setData({ model: e.target.value })} {...bidiAutoInput()} />
              </label>
            </div>
            <label>
              {t('projects.fields.description')}
              <textarea rows={2} value={edit.data.description} onChange={(e) => setData({ description: e.target.value })} {...bidiAutoInput()} />
            </label>

            <div>
              <div className="props-head">
                <b>{t('library.props')}</b>
                <button
                  type="button"
                  className="btn btn-small btn-ghost-inline"
                  onClick={() => setData({ props: [...edit.data.props, { key: '', value: '', unit: '' }] })}
                >
                  + {t('library.addProp')}
                </button>
              </div>
              {edit.data.props.map((p, idx) => (
                <div key={idx} className="prop-row">
                  <input placeholder={t('library.propKey')} value={p.key} onChange={(e) => setProp(idx, 'key', e.target.value)} {...bidiAutoInput()} />
                  <input placeholder={t('library.propValue')} value={p.value} onChange={(e) => setProp(idx, 'value', e.target.value)} {...bidiAutoInput()} />
                  <input placeholder={t('library.propUnit')} value={p.unit} onChange={(e) => setProp(idx, 'unit', e.target.value)} {...bidiAutoInput()} />
                  <button
                    type="button"
                    className="btn btn-small btn-ghost-inline"
                    onClick={() => setData({ props: edit.data.props.filter((_, i) => i !== idx) })}
                  >
                    ✕
                  </button>
                </div>
              ))}
            </div>

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
