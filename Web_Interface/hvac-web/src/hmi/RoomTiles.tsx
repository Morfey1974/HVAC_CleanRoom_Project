import { useTranslation } from 'react-i18next';
import type { HmiRoomItem, Room } from '../api/client';
import { pickName } from '../lib/localized';
import type { HmiParams } from '../api/client';
import { artLibrary, demoValues, HmiDefs, type HmiState } from './arts';
import { CASE_H } from './AhuView';
import { isPipeArt, pipeSize } from './pipeGeom';

type Props = { rooms: Room[]; items: HmiRoomItem[]; values?: Record<string, number>; demo?: HmiState };

const demoRoom = { t: 21.5, rh: 45, dp: 12 };

/** Rooms as tiles in one row: temperature, humidity and pressure against the room setpoints. */
export function RoomTiles({ rooms, items, values, demo }: Props) {
  const { t, i18n } = useTranslation();
  const shown = items.map((it) => ({ it, room: rooms.find((r) => r.id === it.roomId) })).filter((x) => x.room);
  if (!shown.length) return <div className="hmi-canvas muted hmi-empty">{t('hmi.noRoomsOnScreen')}</div>;
  return (
    <div className="hmi-canvas hmi-tiles">
      {shown.map(({ it, room }) => {
        const r = room!;
        const rows = [
          { key: 't', label: t('hmi.slots.t'), unit: '°C', dec: 1, sp: r.tempSetpointC, tol: r.tempToleranceC },
          { key: 'rh', label: t('hmi.slots.rh'), unit: '%', dec: 0, sp: r.rhSetpointPct, tol: r.rhTolerancePct },
          { key: 'dp', label: t('hmi.slots.dp'), unit: 'Pa', dec: 0, sp: r.pressureSetpointPa, tol: r.pressureTolerancePa },
        ] as const;
        const lines = rows.map((row) => {
          const tag = it.bind[row.key];
          const v = demo ? demoRoom[row.key] : tag && values ? values[tag] : undefined;
          const out = v !== undefined && row.sp != null && row.tol != null && Math.abs(v - row.sp) > row.tol;
          return { ...row, tag, v, out };
        });
        const alarm = lines.some((l) => l.out);
        return (
          <div key={it.roomId} className={`hmi-tile ${alarm ? 'hmi-tile--alarm' : ''}`}>
            <div className="hmi-tile__head">
              <b>{pickName(r.name, i18n.language).value}</b>
              {r.isoClass != null && <span className="hmi-tile__iso ltr-value">ISO {r.isoClass}</span>}
            </div>
            {lines
              .filter((l) => l.tag || demo)
              .map((l) => (
                <div key={l.key} className={`hmi-tile__row ${l.out ? 'hmi-tile__row--out' : ''}`}>
                  <span>{l.label}</span>
                  <b className="ltr-value">{l.v === undefined ? '— —' : `${l.v.toFixed(l.dec)} ${l.unit}`}</b>
                  {l.sp != null && (
                    <span className="hmi-tile__sp ltr-value">
                      {l.sp}
                      {l.tol != null ? ` ±${l.tol}` : ''}
                    </span>
                  )}
                </div>
              ))}
            {!demo && !lines.some((l) => l.tag) && <div className="muted small">{t('hmi.noBindings')}</div>}
          </div>
        );
      })}
    </div>
  );
}

/** One library picture on its own (equipment, room tile, or a single section). */
export function ArtPreview({ art, length, params, state }: { art: string; length: number; params: HmiParams; state: HmiState }) {
  const R = artLibrary[art];
  const size = isPipeArt(art) ? pipeSize(art, length, params.rot ?? 0) : { w: length, h: CASE_H };
  return (
    <svg viewBox={`-4 -4 ${size.w + 8} ${size.h + 8}`} className="hmi-preview-svg">
      <HmiDefs />
      {R ? <R w={size.w} h={size.h} v={demoValues(art, state, params)} p={params} state={state} air={state === 'stopped' ? 0 : 80} /> : null}
    </svg>
  );
}
