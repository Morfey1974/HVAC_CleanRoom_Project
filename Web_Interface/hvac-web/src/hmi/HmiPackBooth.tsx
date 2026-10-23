import { useEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import type { HmiScreen, Room } from '../api/client';
import { AhuView, ahuScreenProps } from './AhuView';
import { RoomTiles } from './RoomTiles';
import { HMI_PANEL_H, HMI_PANEL_W, openHmiPack, saveHmiPack, type HmiPackScreen } from './panel';
import { snapshotPanel } from './snapshot';

type Props = {
  projectId: string;
  screens: HmiScreen[];
  rooms: Room[];
  onDone: () => void;
  onError: (message: string) => void;
};

/** Off-screen 1024×600 booth: one snapshot per screen, then open the panel window. */
export function HmiPackBooth({ projectId, screens, rooms, onDone, onError }: Props) {
  const box = useRef<HTMLDivElement>(null);
  const [i, setI] = useState(0);
  const shots = useRef<HmiPackScreen[]>([]);
  const done = useRef(onDone);
  const fail = useRef(onError);
  done.current = onDone;
  fail.current = onError;
  const screen = screens[i];

  useEffect(() => {
    if (!screen) return;
    let stop = false;
    const run = async () => {
      const el = box.current;
      if (!el) return;
      try {
        const png = await snapshotPanel(el);
        if (stop) return;
        shots.current.push({ id: screen.id, kind: screen.kind, name: screen.name, png, screen });
        if (i + 1 < screens.length) setI(i + 1);
        else {
          saveHmiPack({ projectId, at: new Date().toISOString(), screens: shots.current });
          openHmiPack(projectId);
          done.current();
        }
      } catch (e) {
        fail.current((e as Error).message);
      }
    };
    const t = window.setTimeout(() => void run(), 40);
    return () => {
      stop = true;
      clearTimeout(t);
    };
  }, [i, screen, screens.length, projectId]);

  if (!screen) return null;

  return createPortal(
    <div className="hmi-capture-host" aria-hidden>
      <div ref={box} className="hmi-panel" style={{ width: HMI_PANEL_W, height: HMI_PANEL_H }}>
        {screen.kind === 'ahu' ? (
          <AhuView {...ahuScreenProps(screen.content)} demo="running" fit showFlow={false} still />
        ) : (
          <RoomTiles rooms={rooms} items={screen.content.rooms ?? []} demo="running" />
        )}
      </div>
    </div>,
    document.body,
  );
}
