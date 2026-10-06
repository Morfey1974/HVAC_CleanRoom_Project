import { useEffect, useState, type FormEvent } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type Can3Result, type Can3Status } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useConfirm } from '../../components/Dialog';

const POLL_MS = 1000;
/** Shared_Libs/hvac_can.h: HVAC_DM_CMD_* and HVAC_DM_ST_* */
const DM_CMDS = [0, 1, 2, 4];
const DM_ST_BITS = [0x01, 0x02, 0x04];

const ver = (v: number) => `${v >> 8}.${v & 0xff}`;
const hex3 = (n: number) => `0x${n.toString(16).padStart(3, '0')}`;
const bytes = (d: string) => (d.match(/../g) ?? []).join(' ');

/** PLC CAN3 line: bus state, doors master, test frames. */
export function Can3Tab() {
  const { t } = useTranslation();
  const { token } = useAuth();
  const confirm = useConfirm();
  const [s, setS] = useState<Can3Status | null>(null);
  const [busy, setBusy] = useState('');
  const [msg, setMsg] = useState('');
  const [error, setError] = useState('');
  const [txId, setTxId] = useState('321');
  const [txData, setTxData] = useState('00ff000000000000');

  useEffect(() => {
    if (!token) return;
    let stop = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      try {
        const r = await api.can3(token);
        if (!stop && !r.stale) setS(r);
      } catch (e) {
        if (!stop) setError((e as Error).message);
      }
      if (!stop) timer = setTimeout(tick, POLL_MS);
    };
    tick();
    return () => {
      stop = true;
      clearTimeout(timer);
    };
  }, [token]);

  const result = (r: Can3Result, okText: string) => {
    if (r.ok) {
      setMsg(okText);
      setError('');
    } else {
      setMsg('');
      setError(r.error ? t(`firmware.errors.${r.error}`, { defaultValue: r.error }) : t(`can3.sendErr.${r.err ?? 0}`));
    }
  };

  const doorsCmd = async (cmd: number) => {
    if (!token) return;
    if (cmd !== 0) {
      const ok = await confirm({ title: t(`can3.cmd.${cmd}`), message: t('can3.confirmCmd', { cmd: t(`can3.cmd.${cmd}`) }), confirmText: t(`can3.cmd.${cmd}`) });
      if (ok === null) return;
    }
    setBusy(`cmd${cmd}`);
    try {
      const r = await api.doorsCmd(token, cmd, 0xff);
      result(r, t('can3.cmdSent', { seq: r.seq ?? 0 }));
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy('');
    }
  };

  const sendRaw = async (e: FormEvent) => {
    e.preventDefault();
    if (!token) return;
    const id = parseInt(txId, 16);
    const data = txData.replace(/\s+/g, '');
    if (!(id >= 0 && id <= 0x7ff) || !/^([0-9a-fA-F]{2}){0,8}$/.test(data)) {
      setError(t('can3.badFrame'));
      return;
    }
    setBusy('raw');
    try {
      result(await api.can3Send(token, id, data), t('can3.rawSent', { id: hex3(id) }));
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy('');
    }
  };

  if (!s) return <div className="card muted">{t('common.loading')}</div>;
  if (!s.online || !s.plc) return <div className="card error-banner">{t('can3.offline')}</div>;

  const p = s.plc;
  const dm = p.dm;
  const doors = Math.min(Math.max(dm.doors, 0), 8) || 8;
  const bit = (m: number, i: number) => ((m >> i) & 1) === 1;
  const busText = !p.ok ? t('can3.busNotStarted') : p.passive ? t('can3.busAlone') : t('can3.busOk');

  return (
    <>
      {msg && <div className="success-banner">{msg}</div>}
      {error && <div className="error-banner">{error}</div>}

      <div className="card">
        <h2>{t('can3.line')}</h2>
        <p className="muted">{t('can3.lineHint')}</p>
        <div className={p.ok && !p.passive ? 'cfg-st cfg-st--1' : 'cfg-st cfg-st--3'}>{busText}</div>
        <div className="muted ltr-value">
          {t('can3.counters', { rx: p.rx, rxExt: p.rxExt, tx: p.tx, txErr: p.txErr, tec: p.tec, rec: p.rec, boff: p.boff })}
        </div>
        <div>{t(`can3.doorsSrc.${p.doorsSrc}`)}</div>
      </div>

      <div className="card">
        <h2>{t('can3.master')}</h2>
        {dm.rx === 0 ? (
          <p className="muted">{t('can3.masterNever')}</p>
        ) : (
          <>
            <div className={dm.link ? 'cfg-st cfg-st--1' : 'cfg-st cfg-st--3'}>
              {dm.link ? t('can3.masterOk', { ver: ver(dm.ver), doors: dm.doors }) : t('can3.masterLost', { s: Math.round(dm.age / 1000) })}
            </div>
            <div className="table-wrap">
              <table className="data-table">
                <thead>
                  <tr>
                    <th>{t('can3.door')}</th>
                    <th>{t('can3.closed')}</th>
                    <th>{t('can3.locked')}</th>
                    <th>{t('can3.fault')}</th>
                  </tr>
                </thead>
                <tbody>
                  {Array.from({ length: doors }, (_, i) => (
                    <tr key={i}>
                      <td>{t('can3.doorN', { n: i + 1 })}</td>
                      <td>{bit(dm.closed, i) ? t('can3.yes') : t('can3.open')}</td>
                      <td>{bit(dm.locked, i) ? t('can3.yes') : '—'}</td>
                      <td>{bit(dm.fault, i) ? <span className="cfg-st cfg-st--3">{t('can3.yes')}</span> : '—'}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <div className="muted">
              {DM_ST_BITS.filter((b) => (dm.st & b) !== 0)
                .map((b) => t(`can3.st.${b}`))
                .join(', ') || t('can3.stNone')}
              {' · '}
              {t('can3.masterFrames', { rx: dm.rx, cnt: dm.cnt })}
            </div>
          </>
        )}
        <div className="toolbar">
          {DM_CMDS.map((c) => (
            <button key={c} type="button" className="btn btn-small" disabled={!!busy || !p.ok} onClick={() => doorsCmd(c)}>
              {busy === `cmd${c}` ? t('common.loading') : t(`can3.cmd.${c}`)}
            </button>
          ))}
        </div>
        <div className="muted">
          {t('can3.heartbeat', { n: dm.txPlc })}
          {dm.cmdSeq > 0 && (
            <>
              {' · '}
              {dm.ack.valid && dm.ack.seq === dm.cmdSeq
                ? t('can3.ack', { seq: dm.ack.seq, res: t(`can3.res.${dm.ack.res}`, { defaultValue: String(dm.ack.res) }) })
                : t('can3.noAck', { seq: dm.cmdSeq })}
            </>
          )}
        </div>
      </div>

      <div className="card">
        <h2>{t('can3.test')}</h2>
        <p className="muted">{t('can3.testHint')}</p>
        <form className="toolbar" onSubmit={sendRaw}>
          <label>
            {t('can3.frameId')}
            <input value={txId} dir="ltr" size={5} onChange={(e) => setTxId(e.target.value)} />
          </label>
          <label>
            {t('can3.frameData')}
            <input value={txData} dir="ltr" size={20} onChange={(e) => setTxData(e.target.value)} />
          </label>
          <button type="submit" className="btn btn-primary" disabled={!!busy || !p.ok}>
            {busy === 'raw' ? t('common.loading') : t('can3.send')}
          </button>
        </form>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>{t('can3.ago')}</th>
                <th>{t('can3.frameId')}</th>
                <th>{t('can3.frameData')}</th>
              </tr>
            </thead>
            <tbody>
              {p.frames.length === 0 && (
                <tr>
                  <td colSpan={3} className="muted">{t('can3.noFrames')}</td>
                </tr>
              )}
              {p.frames.map((f, i) => (
                <tr key={i}>
                  <td className="ltr-value">{(f.age / 1000).toFixed(1)}</td>
                  <td className="ltr-value">{hex3(f.id)}</td>
                  <td className="ltr-value">{bytes(f.d) || '—'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </>
  );
}
