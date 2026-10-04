import { memo, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import {
  Background,
  ConnectionMode,
  Controls,
  Handle,
  MiniMap,
  Position,
  ReactFlow,
  ReactFlowProvider,
  applyNodeChanges,
  useConnection,
  useReactFlow,
  type Connection,
  type Edge,
  type EdgeChange,
  type Node,
  type NodeChange,
  type NodeProps,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import {
  api,
  linkStyles,
  type GraphicPort,
  type LibraryItem,
  type LinkStyle,
  type ModuleChannel,
  type ProjectLink,
  type ProjectModule,
  type Scheme,
  type SchemePoint,
} from '../../api/client';
import { WireEdge, type WireEdgeType } from '../../components/WireEdge';
import { ChannelSettings } from '../../components/ChannelSettings';
import { channelsNotSet } from '../../lib/modules';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { useConfirm } from '../../components/Dialog';
import { ModuleBlock, PortCaption, portColor, portSide, posStyle } from '../../components/ModuleBlock';

const DRAG_TYPE = 'application/x-hvac-module';

type ModuleNodeData = { module: ProjectModule; name: string; busy: Set<string>; warn: boolean };
type ModuleNode = Node<ModuleNodeData, 'module'>;

const findPort = (m: ProjectModule | undefined, id: string | null | undefined) => m?.snapshot?.graphic?.ports.find((p) => p.id === id);

/** Same rule as the server: same type, and not two pure inputs or two pure outputs. */
const compatible = (a: GraphicPort, b: GraphicPort) => a.type === b.type && !(a.dir === 'in' && b.dir === 'in') && !(a.dir === 'out' && b.dir === 'out');

const sidePosition = (p: GraphicPort) =>
  ({ top: Position.Top, bottom: Position.Bottom, left: Position.Left, right: Position.Right, inside: Position.Bottom })[portSide(p)];

function busyPorts(links: Scheme['links'], moduleId: string) {
  const s = new Set<string>();
  for (const l of links) {
    if (l.fromModuleId === moduleId) s.add(l.fromPort);
    if (l.toModuleId === moduleId) s.add(l.toPort);
  }
  return s;
}

/** Module block with a wire handle on every connection point; compatible points light up while a wire is dragged. */
const ModuleNodeView = memo(function ModuleNodeView({ id, data, selected }: NodeProps<ModuleNode>) {
  const { module: m, name, busy, warn } = data;
  const from = useConnection((c) => (c.inProgress ? { node: c.fromNode.id, port: c.fromHandle?.id ?? '', data: c.fromNode.data as ModuleNodeData } : null));
  const fromPort = from ? findPort(from.data.module, from.port) : undefined;
  const g = m.snapshot?.graphic;
  if (!g) return <div className="mblock mblock--warn">{m.systemName}</div>;

  const state = (p: GraphicPort) => {
    if (!from || !fromPort) return '';
    if (from.node === id && from.port === p.id) return ' mport--source';
    const free = p.type === 'pwr24' || !busy.has(p.id);
    return from.node !== id && free && compatible(fromPort, p) ? ' mport--ok' : ' mport--dim';
  };

  return (
    <ModuleBlock
      graphic={g}
      article={m.snapshot?.code ?? ''}
      name={name}
      systemName={m.systemName}
      moduleId={m.moduleId}
      warn={warn}
      selected={selected}
      renderPort={(p) => (
        <div key={p.id} className={`mport${busy.has(p.id) ? ' mport--busy' : ''}${state(p)}`} style={posStyle(p.x, p.y)}>
          <Handle
            type="source"
            id={p.id}
            position={sidePosition(p)}
            className={`mport__dot mport__dot--${p.dir}`}
            style={{ background: portColor(p) }}
            title={p.label}
          />
          <PortCaption port={p} />
        </div>
      )}
    />
  );
});

const nodeTypes = { module: ModuleNodeView };
const edgeTypes = { wire: WireEdge };

export function SchemePage() {
  return (
    <ReactFlowProvider>
      <SchemeEditor />
    </ReactFlowProvider>
  );
}

function SchemeEditor() {
  const { t, i18n } = useTranslation();
  const { token, hasRole } = useAuth();
  const { projectId, reload: reloadProject } = useProject();
  const confirm = useConfirm();
  const flow = useReactFlow();
  const wrapper = useRef<HTMLDivElement>(null);
  const [scheme, setScheme] = useState<Scheme | null>(null);
  const [library, setLibrary] = useState<LibraryItem[]>([]);
  const [nodes, setNodes] = useState<ModuleNode[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [selectedEdgeId, setSelectedEdgeId] = useState<string | null>(null);
  const [routeDraft, setRouteDraft] = useState<Record<string, SchemePoint[]>>({});
  const [error, setError] = useState('');
  const [info, setInfo] = useState('');
  const [paletteFilter, setPaletteFilter] = useState('');
  const canEdit = hasRole('Engineer');
  const name = useCallback((n: { ru: string; en: string; he: string }) => pickName(n, i18n.language).value, [i18n.language]);
  const errText = (m: string) => t(`scheme.errors.${m}`, { defaultValue: t(`hardware.errors.${m}`, { defaultValue: m }) });

  const run = async (fn: () => Promise<unknown>) => {
    try {
      setError('');
      await fn();
    } catch (e) {
      setError(errText((e as Error).message));
    }
  };

  const load = useCallback(() => {
    if (!token) return Promise.resolve();
    return api
      .scheme(token, projectId)
      .then(setScheme)
      .catch((e: Error) => setError(e.message));
  }, [token, projectId]);
  useEffect(() => {
    load();
  }, [load]);
  useEffect(() => {
    if (token)
      api
        .library(token, 'module')
        .then((l) => setLibrary(l.filter((i) => i.kind && i.kind !== 'plugin')))
        .catch(() => undefined);
  }, [token]);

  const byId = useMemo(() => new Map((scheme?.modules ?? []).map((m) => [m.id, m])), [scheme]);

  // Checks shown in the side panel; a module in this list is outlined on the scheme.
  const warnings = useMemo(() => {
    const list: { moduleId?: string; text: string }[] = [];
    if (!scheme) return list;
    if (!scheme.modules.some((m) => m.kind === 'plc')) list.push({ text: t('scheme.warn.noPlc') });
    for (const m of scheme.modules) {
      const roles = new Set(
        scheme.links.flatMap((l) =>
          l.fromModuleId === m.id ? [findPort(m, l.fromPort)?.role] : l.toModuleId === m.id ? [findPort(m, l.toPort)?.role] : [],
        ),
      );
      if (!m.placed) list.push({ moduleId: m.id, text: t('scheme.warn.unplaced', { name: m.systemName }) });
      else if (m.kind === 'head' && !roles.has('uplink')) list.push({ moduleId: m.id, text: t('scheme.warn.noUplink', { name: m.systemName }) });
      else if (m.kind === 'wagon' && !roles.has('chainIn')) list.push({ moduleId: m.id, text: t('scheme.warn.notWired', { name: m.systemName }) });
      else if (m.kind === 'display' && !roles.has('hubIn')) list.push({ moduleId: m.id, text: t('scheme.warn.notWired', { name: m.systemName }) });
      if (m.libraryLatestVersion && m.libraryLatestVersion > m.libraryVersion)
        list.push({ moduleId: m.id, text: t('scheme.warn.outdated', { name: m.systemName, from: m.libraryVersion, to: m.libraryLatestVersion }) });
      const unset = channelsNotSet(m);
      if (unset.length) list.push({ moduleId: m.id, text: t('scheme.warn.channelsNotSet', { name: m.systemName, channels: unset.join(', ') }) });
    }
    return list;
  }, [scheme, t]);

  const spec = useMemo(() => {
    const counts = new Map<string, { article: string; name: string; count: number }>();
    for (const m of scheme?.modules ?? []) {
      const article = m.snapshot?.code ?? '?';
      const row = counts.get(article) ?? { article, name: m.snapshot ? name(m.snapshot.name) : '', count: 0 };
      row.count++;
      counts.set(article, row);
    }
    return [...counts.values()].sort((a, b) => a.article.localeCompare(b.article));
  }, [scheme, name]);

  // Nodes are kept in local state so dragging is smooth; they are rebuilt whenever the scheme changes.
  useEffect(() => {
    if (!scheme) return;
    const warned = new Set(warnings.map((w) => w.moduleId).filter(Boolean));
    setNodes((prev) => {
      const sel = new Set(prev.filter((n) => n.selected).map((n) => n.id));
      return scheme.modules.map((m) => ({
        id: m.id,
        type: 'module' as const,
        position: { x: m.schemeX, y: m.schemeY },
        selected: sel.has(m.id),
        data: { module: m, name: name(m.userName) || (m.snapshot ? name(m.snapshot.name) : ''), busy: busyPorts(scheme.links, m.id), warn: warned.has(m.id) },
      }));
    });
  }, [scheme, warnings, name]);

  const saveRoute = useCallback(
    async (link: ProjectLink, style: LinkStyle, points: SchemePoint[]) => {
      if (!token) return;
      try {
        setError('');
        const saved = await api.saveLinkRoute(token, projectId, link.id, style, points);
        setScheme((s) => s && { ...s, links: s.links.map((l) => (l.id === saved.id ? saved : l)) });
      } catch (e) {
        setError((e as Error).message);
      } finally {
        setRouteDraft((d) => {
          const { [link.id]: _, ...rest } = d;
          return rest;
        });
      }
    },
    [token, projectId],
  );

  // While a bend is dragged the route lives in the draft; it is saved when the mouse is released.
  const linksRef = useRef<ProjectLink[]>([]);
  linksRef.current = scheme?.links ?? [];
  const onRoute = useCallback(
    (id: string, points: SchemePoint[], commit: boolean) => {
      setRouteDraft((d) => ({ ...d, [id]: points }));
      const link = linksRef.current.find((l) => l.id === id);
      if (commit && link) saveRoute(link, link.style, points);
    },
    [saveRoute],
  );

  const edges: WireEdgeType[] = useMemo(
    () =>
      (scheme?.links ?? []).map((l) => {
        const p = findPort(byId.get(l.fromModuleId), l.fromPort);
        return {
          id: l.id,
          source: l.fromModuleId,
          sourceHandle: l.fromPort,
          target: l.toModuleId,
          targetHandle: l.toPort,
          type: 'wire' as const,
          selected: l.id === selectedEdgeId,
          data: { style: l.style, points: routeDraft[l.id] ?? l.points, color: p ? portColor(p) : '#64748b', editable: canEdit, onRoute },
        };
      }),
    [scheme, byId, selectedEdgeId, routeDraft, canEdit, onRoute],
  );

  const onEdgesChange = useCallback((changes: EdgeChange<WireEdgeType>[]) => {
    for (const c of changes)
      if (c.type === 'select') {
        setSelectedEdgeId((cur) => (c.selected ? c.id : cur === c.id ? null : cur));
        if (c.selected) setSelectedId(null);
      }
  }, []);

  const fitted = useRef(false);
  useEffect(() => {
    if (!fitted.current && nodes.length) {
      fitted.current = true;
      requestAnimationFrame(() => flow.fitView({ padding: 0.15 }));
    }
  }, [nodes.length, flow]);

  const onNodesChange = useCallback((changes: NodeChange<ModuleNode>[]) => {
    setNodes((ns) => applyNodeChanges(changes.filter((c) => c.type !== 'remove'), ns));
    const sel = changes.find((c) => c.type === 'select' && c.selected);
    if (sel && 'id' in sel) {
      setSelectedId(sel.id);
      setSelectedEdgeId(null);
    }
  }, []);

  const onNodeDragStop = (_: unknown, __: Node, dragged: Node[]) =>
    run(async () => {
      if (!token) return;
      await Promise.all(dragged.map((n) => api.moveSchemeModule(token, projectId, n.id, Math.round(n.position.x), Math.round(n.position.y))));
      setScheme((s) =>
        s && { ...s, modules: s.modules.map((m) => { const n = dragged.find((d) => d.id === m.id); return n ? { ...m, schemeX: n.position.x, schemeY: n.position.y } : m; }) },
      );
    });

  const isValidConnection = useCallback(
    (c: Connection | Edge) => {
      if (!scheme || c.source === c.target) return false;
      const a = findPort(byId.get(c.source), c.sourceHandle);
      const b = findPort(byId.get(c.target), c.targetHandle);
      if (!a || !b || !compatible(a, b)) return false;
      if (a.type === 'pwr24') return true;
      return !busyPorts(scheme.links, c.source).has(a.id) && !busyPorts(scheme.links, c.target).has(b.id);
    },
    [scheme, byId],
  );

  const onConnect = (c: Connection) =>
    run(async () => {
      if (!token || !c.sourceHandle || !c.targetHandle) return;
      const before = new Map((scheme?.modules ?? []).map((m) => [m.id, m.moduleId]));
      const next = await api.createLink(token, projectId, { fromModuleId: c.source, fromPort: c.sourceHandle, toModuleId: c.target, toPort: c.targetHandle });
      setScheme(next);
      const numbered = next.modules.filter((m) => m.moduleId && before.get(m.id) === '');
      setInfo(numbered.length ? t('scheme.numbered', { list: numbered.map((m) => `${m.systemName} (${m.moduleId})`).join(', ') }) : '');
      if (numbered.length) reloadProject();
    });

  const addModule = (libraryItemId: string, x: number, y: number) =>
    run(async () => {
      if (!token) return;
      const m = await api.createSchemeModule(token, projectId, libraryItemId, Math.round(x), Math.round(y));
      await load();
      reloadProject();
      setSelectedId(m.id);
      setInfo(m.placed ? '' : t('scheme.dropHint', { name: m.systemName }));
    });

  const onDrop = (e: React.DragEvent) => {
    const id = e.dataTransfer.getData(DRAG_TYPE);
    if (!id) return;
    e.preventDefault();
    const p = flow.screenToFlowPosition({ x: e.clientX, y: e.clientY });
    addModule(id, p.x - 60, p.y - 20);
  };

  const addAtCenter = (id: string) => {
    const box = wrapper.current?.getBoundingClientRect();
    if (!box) return;
    const p = flow.screenToFlowPosition({ x: box.left + box.width / 2, y: box.top + box.height / 2 });
    addModule(id, p.x - 80, p.y - 40);
  };

  const onBeforeDelete = async ({ nodes: dn, edges: de }: { nodes: Node[]; edges: Pick<Edge, 'id' | 'source' | 'target'>[] }) => {
    if (!token || (!dn.length && !de.length)) return false;
    const names = dn.map((n) => byId.get(n.id)?.systemName).filter(Boolean).join(', ');
    const reason = await confirm({
      title: t('common.delete'),
      message: dn.length ? t('scheme.deleteModules', { list: names, count: dn.length }) : t('scheme.deleteLinks', { count: de.length }),
      confirmText: t('common.delete'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return false;
    await run(async () => {
      const nodeIds = new Set(dn.map((n) => n.id));
      for (const e of de) if (!nodeIds.has(e.source) && !nodeIds.has(e.target)) await api.deleteLink(token, projectId, e.id, reason || undefined);
      // Wagons first, heads last: a rail head cannot be removed while its rail still has modules.
      const order = [...dn].sort((a, b) => Number(byId.get(a.id)?.kind === 'head') - Number(byId.get(b.id)?.kind === 'head'));
      for (const n of order) await api.deleteModule(token, projectId, n.id, reason || undefined);
    });
    await load();
    reloadProject();
    setSelectedEdgeId(null);
    return false;
  };

  const selected = selectedId ? byId.get(selectedId) : undefined;
  const selectedLink = selectedEdgeId ? scheme?.links.find((l) => l.id === selectedEdgeId) : undefined;
  const endName = (moduleId: string, port: string) => {
    const m = byId.get(moduleId);
    return `${m?.systemName ?? '?'}:${findPort(m, port)?.label ?? port}`;
  };

  const updateFromLibrary = (m: ProjectModule) =>
    run(async () => {
      if (!token) return;
      const reason = await confirm({
        title: t('scheme.updateFromLibrary'),
        message: t('scheme.updateConfirm', { name: m.systemName, from: m.libraryVersion, to: m.libraryLatestVersion }),
        confirmText: t('scheme.updateFromLibrary'),
        reason: 'optional',
      });
      if (reason === null) return;
      await api.updateModuleFromLibrary(token, projectId, m.id, reason || undefined);
      await load();
    });

  const saveChannels = (m: ProjectModule, channels: ModuleChannel[]) =>
    run(async () => {
      if (!token || !m.libraryItemId) return;
      await api.updateModule(token, projectId, m.id, {
        libraryItemId: m.libraryItemId,
        line: m.line,
        rail: m.rail,
        place: m.place,
        userName: m.userName,
        expectedSerial: m.expectedSerial,
        revision: m.revision,
        notes: m.notes,
        channels,
      });
      await load();
    });

  const palette = useMemo(() => {
    const s = paletteFilter.trim().toLowerCase();
    return library.filter((i) => !s || [i.code, i.name.ru, i.name.en, i.name.he].some((v) => v.toLowerCase().includes(s)));
  }, [library, paletteFilter]);

  return (
    <div className="page scheme-page">
      <div className="page-header">
        <h1>{t('scheme.title')}</h1>
        <div className="toolbar">
          <button type="button" className="btn btn-small btn-ghost-inline" onClick={() => flow.fitView({ padding: 0.15, duration: 300 })}>
            {t('scheme.fit')}
          </button>
          <Link to="../hardware" className="btn btn-small btn-ghost-inline">
            {t('projectNav.hardware')}
          </Link>
        </div>
      </div>
      <p className="muted small">{t('scheme.hint')}</p>
      {error && <div className="error-banner">{error}</div>}
      {info && (
        <div className="info-banner" onClick={() => setInfo('')}>
          {info}
        </div>
      )}

      <div className="scheme-layout">
        {canEdit && (
          <aside className="scheme-palette card">
            <b>{t('scheme.palette')}</b>
            <input placeholder={t('common.search')} value={paletteFilter} onChange={(e) => setPaletteFilter(e.target.value)} />
            <div className="scheme-palette__list">
              {palette.map((i) => (
                <div
                  key={i.id}
                  className="scheme-palette__item"
                  draggable
                  onDragStart={(e) => {
                    e.dataTransfer.setData(DRAG_TYPE, i.id);
                    e.dataTransfer.effectAllowed = 'copy';
                  }}
                  onDoubleClick={() => addAtCenter(i.id)}
                  title={t('scheme.paletteHint')}
                >
                  {i.graphic && (
                    <div className="scheme-palette__thumb" dir="ltr">
                      <ModuleBlock graphic={i.graphic} article={i.code} scale={0.36} />
                    </div>
                  )}
                  <div>
                    <b className="ltr-value">{i.code}</b>
                    <div className="muted small">{name(i.name)}</div>
                  </div>
                </div>
              ))}
            </div>
          </aside>
        )}

        <div
          className="scheme-canvas card"
          ref={wrapper}
          dir="ltr"
          onDragOver={(e) => {
            if (e.dataTransfer.types.includes(DRAG_TYPE)) {
              e.preventDefault();
              e.dataTransfer.dropEffect = 'copy';
            }
          }}
          onDrop={onDrop}
        >
          <ReactFlow<ModuleNode, WireEdgeType>
            nodes={nodes}
            edges={edges}
            nodeTypes={nodeTypes}
            edgeTypes={edgeTypes}
            onNodesChange={onNodesChange}
            onEdgesChange={onEdgesChange}
            elevateEdgesOnSelect
            zoomOnDoubleClick={false}
            onNodeDragStop={onNodeDragStop}
            onConnect={onConnect}
            isValidConnection={isValidConnection}
            onBeforeDelete={onBeforeDelete}
            onPaneClick={() => {
              setSelectedId(null);
              setSelectedEdgeId(null);
            }}
            connectionMode={ConnectionMode.Loose}
            connectionRadius={28}
            nodesDraggable={canEdit}
            nodesConnectable={canEdit}
            deleteKeyCode={canEdit ? ['Delete', 'Backspace'] : null}
            snapToGrid
            snapGrid={[10, 10]}
            minZoom={0.15}
            maxZoom={2.5}
            proOptions={{ hideAttribution: true }}
          >
            <Background gap={20} size={1} />
            <Controls showInteractive={false} />
            <MiniMap pannable zoomable nodeColor={(n) => (n.data as ModuleNodeData).module.snapshot?.graphic?.header ?? '#94a3b8'} />
          </ReactFlow>
        </div>

        <aside className="scheme-side card">
          {selectedLink ? (
            <div className="scheme-side__section">
              <b>{t('scheme.wire')}</b>
              <div className="small ltr-value">
                {endName(selectedLink.fromModuleId, selectedLink.fromPort)} → {endName(selectedLink.toModuleId, selectedLink.toPort)}
              </div>
              {canEdit && (
                <>
                  <div className="segmented">
                    {linkStyles.map((s) => (
                      <button
                        key={s}
                        type="button"
                        className={`btn btn-small${selectedLink.style === s ? ' btn-primary' : ' btn-ghost-inline'}`}
                        onClick={() => saveRoute(selectedLink, s, selectedLink.points)}
                      >
                        {t(`scheme.styles.${s}`)}
                      </button>
                    ))}
                  </div>
                  <div className="muted small">
                    {t('scheme.bends', { count: selectedLink.points.length })}
                  </div>
                  <div className="toolbar">
                    <button
                      type="button"
                      className="btn btn-small btn-ghost-inline"
                      disabled={!selectedLink.points.length}
                      onClick={() => saveRoute(selectedLink, selectedLink.style, [])}
                    >
                      {t('scheme.resetRoute')}
                    </button>
                    <button
                      type="button"
                      className="btn btn-small btn-ghost-inline"
                      onClick={() => onBeforeDelete({ nodes: [], edges: [{ id: selectedLink.id, source: selectedLink.fromModuleId, target: selectedLink.toModuleId }] })}
                    >
                      {t('common.delete')}
                    </button>
                  </div>
                  <p className="muted small">{t('scheme.wireHint')}</p>
                </>
              )}
            </div>
          ) : selected ? (
            <div className="scheme-side__section">
              <b>
                {selected.systemName} <span className="ltr-value muted">{selected.moduleId}</span>
              </b>
              <div className="small">
                <span className="ltr-value">{selected.snapshot?.code}</span> · {selected.snapshot ? name(selected.snapshot.name) : ''}
              </div>
              {name(selected.userName) && <div className="small">{name(selected.userName)}</div>}
              <div className="muted small">
                {t('scheme.libraryVersion', { version: selected.libraryVersion })}
                {selected.libraryLatestVersion && selected.libraryLatestVersion > selected.libraryVersion
                  ? ` → v${selected.libraryLatestVersion}`
                  : ''}
              </div>
              {canEdit && selected.libraryLatestVersion && selected.libraryLatestVersion > selected.libraryVersion ? (
                <button type="button" className="btn btn-small" onClick={() => updateFromLibrary(selected)}>
                  {t('scheme.updateFromLibrary')}
                </button>
              ) : null}
              {(selected.snapshot?.channelModes?.length ?? 0) > 0 && (selected.snapshot?.channelCount ?? 0) > 0 && (
                <div className="mt">
                  <b className="small">{t('channels.title')}</b>
                  <ChannelSettings
                    prefix={t('channels.channel')}
                    count={selected.snapshot!.channelCount!}
                    modes={selected.snapshot!.channelModes!}
                    value={selected.channels}
                    disabled={!canEdit}
                    onChange={(channels) => saveChannels(selected, channels)}
                  />
                </div>
              )}
              <div className="small mt">
                {(selected.snapshot?.graphic?.ports ?? []).map((p) => {
                  const link = scheme?.links.find(
                    (l) => (l.fromModuleId === selected.id && l.fromPort === p.id) || (l.toModuleId === selected.id && l.toPort === p.id),
                  );
                  const other = link ? byId.get(link.fromModuleId === selected.id ? link.toModuleId : link.fromModuleId) : undefined;
                  const otherPort = link ? findPort(other, link.fromModuleId === selected.id ? link.toPort : link.fromPort) : undefined;
                  return (
                    <div key={p.id} className="scheme-side__port">
                      <span className={`mport__dot mport__dot--${p.dir} mport__dot--inline`} style={{ background: portColor(p) }} />
                      <span className="ltr-value">{p.label}</span>
                      {other && (
                        <span className="muted ltr-value">
                          → {other.systemName}:{otherPort?.label}
                        </span>
                      )}
                    </div>
                  );
                })}
              </div>
            </div>
          ) : (
            <p className="muted small">{t('scheme.selectHint')}</p>
          )}

          <div className="scheme-side__section">
            <b>
              {t('scheme.checks')} <span className="count-pill">{warnings.length}</span>
            </b>
            {warnings.length === 0 && <div className="muted small">{t('scheme.allGood')}</div>}
            {warnings.map((w, i) => (
              <div
                key={i}
                className="scheme-warning small"
                onClick={() => {
                  if (!w.moduleId) return;
                  const n = nodes.find((x) => x.id === w.moduleId);
                  if (n) flow.setCenter(n.position.x + 80, n.position.y + 60, { zoom: 1, duration: 300 });
                  setSelectedId(w.moduleId);
                }}
              >
                ⚠ {w.text}
              </div>
            ))}
          </div>

          <div className="scheme-side__section">
            <b>{t('scheme.spec')}</b>
            <table className="data-table compact">
              <tbody>
                {spec.map((s) => (
                  <tr key={s.article}>
                    <td className="ltr-value nowrap">{s.article}</td>
                    <td className="small">{s.name}</td>
                    <td className="ltr-value">{s.count}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </aside>
      </div>
    </div>
  );
}
