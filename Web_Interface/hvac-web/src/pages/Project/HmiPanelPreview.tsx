import { useEffect, useState } from 'react';
import { useParams, useSearchParams } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type HmiScreen, type Room } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useLive } from '../../context/LiveDataContext';
import { useProject } from '../../context/ProjectContext';
import { pickName } from '../../lib/localized';
import { AhuView, ahuScreenProps } from '../../hmi/AhuView';
import { RoomTiles } from '../../hmi/RoomTiles';
import { hmiStates, type HmiState } from '../../hmi/arts';
import {
  HMI_PANEL_H,
  HMI_PANEL_W,
  centerHmiPreviewWindow,
  hmiPreviewKey,
  readHmiPack,
  type HmiPack,
  type HmiPreviewDemo,
  type HmiPreviewPayload,
} from '../../hmi/panel';

const asDemo = (raw: string | null): HmiPreviewDemo =>
  raw && (hmiStates as string[]).includes(raw) ? (raw as HmiState) : 'live';

function readDraft(projectId: string, screenId: string): HmiPreviewPayload | null {
  try {
    const raw = sessionStorage.getItem(hmiPreviewKey(projectId, screenId));
    if (!raw) return null;
    const p = JSON.parse(raw) as HmiPreviewPayload;
    return p?.screen?.id === screenId ? p : null;
  } catch {
    return null;
  }
}

/** 10.1" panel window: snapshot plus live overlay, or a live vector preview. */
export function HmiPanelPreview() {
  const { t, i18n } = useTranslation();
  const { screenId } = useParams();
  const [qs] = useSearchParams();
  const packMode = qs.get('pack') === '1';
  const still = qs.get('still') === '1';
  const { token } = useAuth();
  const { projectId, project } = useProject();
  const { snapshot } = useLive();
  const [pack, setPack] = useState<HmiPack | null>(null);
  const [active, setActive] = useState(0);
  const [screen, setScreen] = useState<HmiScreen | null>(null);
  const [rooms, setRooms] = useState<Room[]>([]);
  const [demo, setDemo] = useState<HmiPreviewDemo>(asDemo(qs.get('demo')));
  const [error, setError] = useState('');

  useEffect(() => {
    document.body.classList.add('hmi-panel-page');
    return () => document.body.classList.remove('hmi-panel-page');
  }, []);

  useEffect(() => {
    centerHmiPreviewWindow();
    const id = requestAnimationFrame(() => centerHmiPreviewWindow());
    return () => cancelAnimationFrame(id);
  }, []);

  useEffect(() => {
    if (packMode) {
      const p = readHmiPack(projectId);
      if (!p || p.screens.length === 0) {
        setError(t('hmi.panelPackEmpty'));
        return;
      }
      setPack(p);
      setActive(0);
      document.title = `${t('hmi.panelDemo')} · ${HMI_PANEL_W}×${HMI_PANEL_H}`;
      return;
    }
    if (!token || !screenId) return;
    const draft = readDraft(projectId, screenId);
    if (draft) {
      setScreen(draft.screen);
      setDemo(draft.demo);
    }
    Promise.all([api.hmiScreens(token, projectId), api.rooms(token, projectId)])
      .then(([list, r]) => {
        setRooms([...r].sort((a, b) => a.sortOrder - b.sortOrder));
        if (!draft) {
          const found = list.find((s) => s.id === screenId) ?? null;
          setScreen(found);
          if (!found) setError(t('hmi.previewMissing'));
        }
      })
      .catch((e: Error) => setError(e.message));
  }, [token, projectId, screenId, packMode, t]);

  useEffect(() => {
    if (packMode || !screen) return;
    document.title = `${pickName(screen.name, i18n.language).value} · ${HMI_PANEL_W}×${HMI_PANEL_H}`;
  }, [screen, i18n.language, packMode]);

  const values = demo === 'live' && project?.isActiveOnSite ? snapshot?.values : undefined;
  const run = demo === 'live' ? undefined : demo;
  const shot = pack?.screens[active];

  return (
    <div className="hmi-panel" style={{ width: HMI_PANEL_W, height: HMI_PANEL_H }}>
      {error && <p className="hmi-panel__msg">{error}</p>}
      {!error && packMode && shot && (
        <>
          <img className="hmi-panel__shot" src={shot.png} alt="" width={HMI_PANEL_W} height={HMI_PANEL_H} />
          {shot.kind === 'ahu' && (
            <div className="hmi-panel__overlay">
              <AhuView {...ahuScreenProps(shot.screen.content)} demo="running" fit layer="overlay" />
            </div>
          )}
          <div className="hmi-panel__tabs">
            <span className="hmi-panel__badge">{t('hmi.panelDemo')}</span>
            {pack.screens.map((s, n) => (
              <button key={s.id} type="button" className={n === active ? 'hmi-panel__tab is-on' : 'hmi-panel__tab'} onClick={() => setActive(n)}>
                {pickName(s.name, i18n.language).value}
              </button>
            ))}
          </div>
        </>
      )}
      {!error && !packMode && !screen && <p className="hmi-panel__msg">{t('common.loading')}</p>}
      {!error && !packMode && screen?.kind === 'ahu' && (
        <AhuView
          {...ahuScreenProps(screen.content)}
          values={values}
          demo={still ? 'running' : run}
          fit
          showFlow={!still}
          still={still}
        />
      )}
      {!error && !packMode && screen?.kind === 'rooms' && <RoomTiles rooms={rooms} items={screen.content.rooms ?? []} values={values} />}
    </div>
  );
}
