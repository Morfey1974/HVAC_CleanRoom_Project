import { useEffect, useMemo, useState, type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type Equipment, type HmiScreen, type ProjectModule, type Room } from '../../api/client';
import { MnemoView } from '../../mnemo/MnemoView';
import { AhuView } from '../../hmi/AhuView';
import { RoomTiles } from '../../hmi/RoomTiles';
import { BidiText } from '../../components/BidiText';
import { demoNanoMotion } from '../../mnemo/demoNanoMotion';
import { useLive, type LiveSnapshot } from '../../context/LiveDataContext';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { boundOutputs, type BoundOutput } from '../../lib/sensors';
import { SimulatorPanel } from './SimulatorPanel';

/** Live data exists only for the project that runs on the site PLC. */
export function SiteOnly({ children }: { children: ReactNode }) {
  const { t } = useTranslation();
  const { project, projectId } = useProject();
  if (project?.isActiveOnSite) return <>{children}</>;
  return (
    <div className="card info-card">
      <p>{t('overview.siteInactive')}</p>
      <Link to={`/p/${projectId}/overview`}>{t('projectNav.overview')}</Link>
    </div>
  );
}

const DEMO_PROJECT = 'DEMO-001';

function useHmiScreens() {
  const { token } = useAuth();
  const { projectId } = useProject();
  const [data, setData] = useState<{ list: HmiScreen[]; rooms: Room[] } | null>(null);
  useEffect(() => {
    if (!token) return;
    Promise.all([api.hmiScreens(token, projectId), api.rooms(token, projectId)])
      .then(([list, rooms]) => setData({ list, rooms }))
      .catch(() => setData({ list: [], rooms: [] }));
  }, [token, projectId]);
  return data;
}

export function MonitoringPage() {
  const { t, i18n } = useTranslation();
  const { snapshot } = useLive();
  const { hasRole } = useAuth();
  const { project } = useProject();
  const alarmElements = useMemo(() => new Set((snapshot?.alarms ?? []).map((a) => a.element)), [snapshot]);
  const showSim = snapshot?.mode === 'simulator' && hasRole('Engineer');
  const screens = useHmiScreens();
  const demo = project?.number === DEMO_PROJECT && screens !== null && screens.list.length === 0;

  return (
    <div className="page">
      <h1>{t('projectNav.monitoring')}</h1>
      <SiteOnly>
        {snapshot && <SourceBar snapshot={snapshot} />}
        <RoomCards />
        {screens && screens.list.length > 0 && (
          <div className="monitor-layout mt">
            <div className="hmi-live">
              {screens.list.map((s) => (
                <div key={s.id} className="card hmi-host">
                  <b className="small hmi-live__title">
                    <BidiText>{pickName(s.name, i18n.language).value}</BidiText>
                  </b>
                  {s.kind === 'ahu' ? (
                    <AhuView items={s.content.items ?? []} values={snapshot?.values} flowTag={s.content.flowTag} />
                  ) : (
                    <RoomTiles rooms={screens.rooms} items={s.content.rooms ?? []} values={snapshot?.values} />
                  )}
                </div>
              ))}
            </div>
            {showSim && <SimulatorPanel />}
          </div>
        )}
        {demo && (
          <div className="monitor-layout mt">
            <div className="card mn-card-host">
              <b className="small">{t('monitoring.demoMnemo')}</b>
              {snapshot ? (
                <MnemoView mnemonic={demoNanoMotion} values={snapshot.values} alarmElements={alarmElements} />
              ) : (
                <p className="muted">{t('common.loading')}</p>
              )}
            </div>
            {showSim && <SimulatorPanel />}
          </div>
        )}
      </SiteOnly>
    </div>
  );
}

/** Data source and, for the real PLC, the state of each link in the chain up to the display. */
function SourceBar({ snapshot }: { snapshot: LiveSnapshot }) {
  const { t } = useTranslation();
  const v = snapshot.values;
  const plc = snapshot.mode === 'plc';
  const dot = (key: string, label: string) => {
    const ok = v[key] === 1;
    return (
      <span className={`chain-item ${ok ? 'chain-item--ok' : 'chain-item--bad'}`}>
        <span className="chain-dot" />
        {label}: {ok ? t('monitoring.ok') : t('monitoring.fault')}
      </span>
    );
  };
  return (
    <div className="card chain-bar">
      <span>
        {t('monitoring.source')}: <b>{t(`settings.plcModes.${snapshot.mode}`, { defaultValue: snapshot.mode })}</b>
      </span>
      {plc && (
        <>
          {dot('PLC.ONLINE', t('monitoring.chainPlc'))}
          {dot('LOC.LINK', t('monitoring.chainLoco'))}
          {dot('AI.LINK', t('monitoring.chainAi'))}
          {dot('AI.ADC', t('monitoring.chainAdc'))}
          {v['HUB.TX'] !== undefined && (
            <span className="chain-item">
              {t('monitoring.chainDisplay')}: <b className="ltr-value">{v['HUB.TX']}</b>
              {v['HUB.TXERR'] ? <span className="muted"> · {t('monitoring.chainErrors', { count: v['HUB.TXERR'] })}</span> : null}
            </span>
          )}
        </>
      )}
    </div>
  );
}

function RoomCards() {
  const { t, i18n } = useTranslation();
  const { token } = useAuth();
  const { projectId } = useProject();
  const { snapshot } = useLive();
  const [rooms, setRooms] = useState<Room[] | null>(null);
  const [bound, setBound] = useState<BoundOutput[]>([]);

  useEffect(() => {
    if (!token) return;
    Promise.all([api.rooms(token, projectId), api.equipment(token, projectId), api.modules(token, projectId)])
      .then(([r, e, m]: [Room[], Equipment[], ProjectModule[]]) => {
        setRooms([...r].sort((a, b) => a.sortOrder - b.sortOrder));
        setBound(boundOutputs(m, e));
      })
      .catch(() => setRooms([]));
  }, [token, projectId]);

  if (!rooms) return <p className="muted">{t('common.loading')}</p>;

  const name = (r: Room) => pickName(r.name, i18n.language).value;
  const loose = bound.filter((b) => !b.equipment.roomId || !rooms.some((r) => r.id === b.equipment.roomId));

  return (
    <div className="room-cards">
      {rooms.length === 0 && <div className="card muted">{t('monitoring.noRooms')}</div>}
      {rooms.map((r) => (
        <RoomCard key={r.id} title={name(r)} room={r} items={bound.filter((b) => b.equipment.roomId === r.id)} values={snapshot?.values} />
      ))}
      {loose.length > 0 && <RoomCard title={t('monitoring.unassigned')} items={loose} values={snapshot?.values} />}
    </div>
  );
}

function RoomCard({ title, room, items, values }: { title: string; room?: Room; items: BoundOutput[]; values?: Record<string, number> }) {
  const { t } = useTranslation();
  return (
    <div className="card room-card">
      <div className="room-card__title">{title}</div>
      {items.length === 0 && <div className="muted small">{t('monitoring.noSensors')}</div>}
      {items.map((b) => {
        const val = values?.[b.key];
        const band = room ? bandOf(room, b.output.quantity) : null;
        const out = val !== undefined && band ? Math.abs(val - band.sp) > band.tol : false;
        return (
          <div key={b.key} className={`room-value ${val === undefined ? 'room-value--none' : out ? 'room-value--alarm' : ''}`}>
            <span className="room-value__label">
              {t(`quantities.${b.output.quantity}`)}
              <span className="muted small ltr-value"> {b.equipment.tag || b.equipment.snapshot?.code} · {b.module.systemName}/{b.channel}</span>
            </span>
            <b className="room-value__num ltr-value">{val === undefined ? t('monitoring.noData') : `${val.toFixed(1)} ${b.output.unit}`}</b>
            {band && (
              <span className="muted small ltr-value">{t('monitoring.setpoint', { sp: band.sp, tol: band.tol })}</span>
            )}
          </div>
        );
      })}
    </div>
  );
}

function bandOf(r: Room, quantity: string): { sp: number; tol: number } | null {
  const pair =
    quantity === 'temperature' ? [r.tempSetpointC, r.tempToleranceC]
    : quantity === 'humidity' ? [r.rhSetpointPct, r.rhTolerancePct]
    : quantity === 'pressure' ? [r.pressureSetpointPa, r.pressureTolerancePa]
    : [null, null];
  return pair[0] != null && pair[1] != null ? { sp: pair[0], tol: pair[1] } : null;
}
