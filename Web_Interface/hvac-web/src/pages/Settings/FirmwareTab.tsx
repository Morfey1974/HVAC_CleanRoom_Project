import { useCallback, useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { api, type FirmwareFile, type FirmwareResult, type PlcFwLog, type PlcFwStatus } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useConfirm } from '../../components/Dialog';
import { formatDateTime, formatSize } from '../../lib/localized';

const POLL_MS = 1000;
const LOG_KEEP = 200;
const MODE_DIFFERENT = 1;
const MODE_ALL = 2;
const ROLE_CURRENT = 1;
const ROLE_NEW = 2;
const ROLE_BACKUP = 3;
const EV_CFG_FIRST = 14;
const EV_ROLLBACK = 18;
const EV_ID_WALK = 19;

const ver = (v: number) => `${v >> 8}.${v & 0xff}`;

function uptime(ms: number) {
  const s = Math.floor(ms / 1000);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const pad = (n: number) => n.toString().padStart(2, '0');
  return `${h}:${pad(m)}:${pad(s % 60)}`;
}

/** Module firmware: files in the app, images in the PLC store, modules on the CAN bus, update runs. */
export function FirmwareTab() {
  const { t, i18n } = useTranslation();
  const { token } = useAuth();
  const confirm = useConfirm();
  const [files, setFiles] = useState<FirmwareFile[]>([]);
  const [status, setStatus] = useState<PlcFwStatus | null>(null);
  const [log, setLog] = useState<PlcFwLog[]>([]);
  const [busy, setBusy] = useState('');
  const [error, setError] = useState('');
  const [info, setInfo] = useState('');
  const [modes, setModes] = useState<Record<number, number>>({});
  const cursor = useRef({ boot: 0, id: 0 });

  const typeName = useCallback((n: number) => t(`firmware.types.${n}`, { defaultValue: `#${n}` }), [t]);
  const resultText = useCallback(
    (r: FirmwareResult) =>
      r.error === 'plc_rejected' && r.plcError != null
        ? t(`firmware.plcErrors.${r.plcError}`, { defaultValue: `${t('firmware.errors.plc_rejected')} (${r.plcError})` })
        : t(`firmware.errors.${r.error}`, { defaultValue: r.error ?? '' }),
    [t],
  );

  const loadFiles = useCallback(() => {
    if (token) api.firmware(token).then(setFiles).catch((e: Error) => setError(e.message));
  }, [token]);
  useEffect(loadFiles, [loadFiles]);

  useEffect(() => {
    if (!token) return;
    let stop = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      try {
        const s = await api.plcFirmware(token, cursor.current.id);
        if (stop) return;
        setStatus(s);
        const p = s.plc;
        if (p && !s.stale) {
          if (p.boot !== cursor.current.boot) {
            cursor.current = { boot: p.boot, id: 0 };
            setLog(p.log);
          } else if (p.log.length > 0) {
            setLog((old) => [...old, ...p.log.filter((e) => e.id > cursor.current.id)].slice(-LOG_KEEP));
          }
          if (p.log.length > 0) cursor.current.id = Math.max(cursor.current.id, ...p.log.map((e) => e.id));
        }
      } catch {
        /* next tick */
      }
      if (!stop) timer = setTimeout(tick, POLL_MS);
    };
    void tick();
    return () => {
      stop = true;
      clearTimeout(timer);
    };
  }, [token]);

  const run = async <T,>(key: string, fn: () => Promise<T>): Promise<T | undefined> => {
    setBusy(key);
    setError('');
    setInfo('');
    try {
      return await fn();
    } catch (e) {
      setError(t(`firmware.errors.${(e as Error).message}`, { defaultValue: (e as Error).message }));
      return undefined;
    } finally {
      setBusy('');
    }
  };

  const upload = async (list: FileList | null) => {
    if (!token || !list?.length) return;
    for (const f of Array.from(list)) {
      const r = await run('upload', () => api.uploadFirmware(token, f));
      if (!r) break;
    }
    loadFiles();
  };

  const send = async (f: FirmwareFile) => {
    if (!token) return;
    const reason = await confirm({
      title: t('firmware.send'),
      message: t('firmware.confirmSend', { type: typeName(f.moduleType), ver: ver(f.version) }),
      confirmText: t('firmware.send'),
      reason: 'optional',
    });
    if (reason === null) return;
    const r = await run(`send:${f.id}`, () => api.sendFirmware(token, f.id, reason || undefined));
    if (r) {
      if (r.ok) setInfo(t('firmware.sent', { type: typeName(f.moduleType), ver: ver(f.version) }));
      else setError(resultText(r));
      loadFiles();
    }
  };

  const remove = async (f: FirmwareFile) => {
    if (!token) return;
    const reason = await confirm({
      title: t('dialog.deleteTitle'),
      message: t('firmware.confirmDelete', { name: f.fileName }),
      confirmText: t('common.delete'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return;
    await run(`del:${f.id}`, () => api.deleteFirmware(token, f.id, reason || undefined));
    loadFiles();
  };

  const startRun = async (type: number, imageVer: number) => {
    if (!token) return;
    const mode = modes[type] ?? MODE_DIFFERENT;
    const reason = await confirm({
      title: t('firmware.run'),
      message: t('firmware.confirmRun', { type: typeName(type), ver: ver(imageVer), mode: t(`firmware.modes.${mode}`) }),
      confirmText: t('firmware.run'),
      reason: 'optional',
    });
    if (reason === null) return;
    const r = await run(`run:${type}`, () => api.runFirmware(token, type, mode, reason || undefined));
    if (r && !r.ok) setError(r.error === 'plc_rejected' && r.plcError != null ? t(`firmware.runErrors.${r.plcError}`) : resultText(r));
  };

  const rollback = async (type: number, backupVer: number) => {
    if (!token) return;
    const reason = await confirm({
      title: t('firmware.rollback'),
      message: t('firmware.confirmRollback', { type: typeName(type), ver: ver(backupVer) }),
      confirmText: t('firmware.rollback'),
      danger: true,
      reason: 'optional',
    });
    if (reason === null) return;
    const r = await run(`rb:${type}`, () => api.rollbackFirmware(token, type, reason || undefined));
    if (r && !r.ok) setError(r.error === 'plc_rejected' && r.plcError != null ? t(`firmware.runErrors.${r.plcError}`) : resultText(r));
  };

  const cancel = async () => {
    if (!token) return;
    const r = await run('cancel', () => api.cancelFirmware(token));
    if (r && !r.ok) setError(resultText(r));
  };

  const plc = status?.plc ?? null;
  const offline = status && !status.online;
  const types = [...new Set([...(plc?.slots.map((s) => s.type) ?? []), ...(plc?.nodes.map((n) => n.type) ?? [])])].sort((a, b) => a - b);
  const slotOf = (type: number, role: number) => plc?.slots.find((s) => s.type === type && s.role === role);
  const runActive = !!plc?.run.active;

  return (
    <div className="fw-page">
      {offline && (
        <div className="error-banner">
          {t(`firmware.errors.${status.error}`, { defaultValue: status.error ?? '' })}
        </div>
      )}
      {plc && !plc.store && <div className="warning-banner">{t('firmware.storeFail')}</div>}
      {status?.uploading && <div className="warning-banner">{t('firmware.uploading')}</div>}
      {error && <div className="error-banner">{error}</div>}
      {info && <div className="success-banner">{info}</div>}

      <div className="card">
        <div className="toolbar">
          <h2>{t('firmware.files')}</h2>
          <label className={busy === 'upload' ? 'btn btn-primary disabled' : 'btn btn-primary'}>
            {busy === 'upload' ? t('common.loading') : `+ ${t('firmware.upload')}`}
            <input type="file" accept=".bin,.elf" multiple hidden disabled={!!busy} onChange={(e) => { upload(e.target.files); e.target.value = ''; }} />
          </label>
        </div>
        <p className="field-hint">{t('firmware.uploadHint')}</p>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>{t('firmware.moduleType')}</th>
                <th>{t('firmware.version')}</th>
                <th>{t('firmware.board')}</th>
                <th>{t('firmware.file')}</th>
                <th>{t('firmware.size')}</th>
                <th>{t('firmware.uploaded')}</th>
                <th>{t('firmware.sentToPlc')}</th>
                <th />
              </tr>
            </thead>
            <tbody>
              {files.length === 0 && (
                <tr>
                  <td colSpan={8} className="muted">{t('firmware.empty')}</td>
                </tr>
              )}
              {files.map((f) => (
                <tr key={f.id}>
                  <td>{typeName(f.moduleType)}</td>
                  <td className="ltr-value">{ver(f.version)}</td>
                  <td className="ltr-value">{f.boardRev}</td>
                  <td className="ltr-value">
                    <button type="button" className="link-button" onClick={() => token && api.downloadFirmware(token, f)}>
                      {f.fileName}
                    </button>
                  </td>
                  <td className="ltr-value">{formatSize(f.sizeBytes)}</td>
                  <td className="ltr-value">
                    {formatDateTime(f.uploadedAt, i18n.language)} · {f.uploadedBy}
                  </td>
                  <td className="ltr-value">{formatDateTime(f.sentToPlcAt, i18n.language)}</td>
                  <td className="row-actions">
                    <button type="button" className="btn btn-small btn-primary" disabled={!!busy || runActive || !status?.online} onClick={() => send(f)}>
                      {busy === `send:${f.id}` ? t('firmware.sending') : t('firmware.send')}
                    </button>
                    <button type="button" className="btn btn-small btn-danger" disabled={!!busy} onClick={() => remove(f)}>
                      {t('common.delete')}
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      {plc && runActive && (
        <div className="card info-card">
          <div className="toolbar">
            <div>
              <strong>
                {t(plc.run.rollback ? 'firmware.rollbackActive' : 'firmware.runActive', { type: typeName(plc.run.type), ver: ver(plc.run.ver) })}
              </strong>
              <div className="muted">{t('firmware.runCounts', { done: plc.run.done, failed: plc.run.failed })}</div>
            </div>
            <button type="button" className="btn btn-danger" disabled={busy === 'cancel'} onClick={cancel}>
              {t('firmware.cancel')}
            </button>
          </div>
        </div>
      )}

      {plc && (
        <div className="card">
          <h2>{t('firmware.store')}</h2>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>{t('firmware.moduleType')}</th>
                  <th>{t('firmware.roles.1')}</th>
                  <th>{t('firmware.roles.2')}</th>
                  <th>{t('firmware.roles.3')}</th>
                  <th>{t('firmware.mode')}</th>
                  <th />
                </tr>
              </thead>
              <tbody>
                {types.length === 0 && (
                  <tr>
                    <td colSpan={6} className="muted">{t('firmware.storeEmpty')}</td>
                  </tr>
                )}
                {types.map((type) => {
                  const cur = slotOf(type, ROLE_CURRENT);
                  const nw = slotOf(type, ROLE_NEW);
                  const bak = slotOf(type, ROLE_BACKUP);
                  const image = nw ?? cur;
                  return (
                    <tr key={type}>
                      <td>{typeName(type)}</td>
                      {[cur, nw, bak].map((s, i) => (
                        <td key={i} className="ltr-value">
                          {s ? `${ver(s.ver)} · ${formatSize(s.size)}` : '—'}
                        </td>
                      ))}
                      <td>
                        <select
                          value={modes[type] ?? MODE_DIFFERENT}
                          onChange={(e) => setModes({ ...modes, [type]: Number(e.target.value) })}
                        >
                          <option value={MODE_DIFFERENT}>{t('firmware.modes.1')}</option>
                          <option value={MODE_ALL}>{t('firmware.modes.2')}</option>
                        </select>
                      </td>
                      <td className="row-actions">
                        <button
                          type="button"
                          className="btn btn-small btn-primary"
                          disabled={!image || runActive || !!busy || status?.uploading || status?.stale}
                          onClick={() => image && startRun(type, image.ver)}
                        >
                          {t('firmware.run')}
                        </button>
                        {bak && (
                          <button
                            type="button"
                            className="btn btn-small"
                            disabled={runActive || !!busy || status?.uploading || status?.stale}
                            onClick={() => rollback(type, bak.ver)}
                          >
                            {t('firmware.rollback')}
                          </button>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          <p className="field-hint">{t('firmware.storeHint')}</p>
        </div>
      )}

      {plc && (
        <div className="card">
          <h2>{t('firmware.nodes')}</h2>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>{t('firmware.moduleType')}</th>
                  <th>{t('firmware.tag')}</th>
                  <th>{t('firmware.version')}</th>
                  <th>{t('firmware.board')}</th>
                  <th>{t('firmware.state')}</th>
                  <th>{t('firmware.lastResult')}</th>
                </tr>
              </thead>
              <tbody>
                {plc.nodes.length === 0 && (
                  <tr>
                    <td colSpan={6} className="muted">{t('firmware.noNodes')}</td>
                  </tr>
                )}
                {plc.nodes.map((n) => {
                  const updating = !!plc.busy && plc.busyTag === n.tag;
                  return (
                    <tr key={n.tag}>
                      <td>{typeName(n.type)}</td>
                      <td className="ltr-value">{n.tag.toUpperCase()}</td>
                      <td className="ltr-value">
                        {n.state === 0 ? ver(n.ver) : '—'}
                        {n.mismatch ? <span className="badge badge--warn">{t('firmware.mismatch')}</span> : null}
                      </td>
                      <td className="ltr-value">{n.board}</td>
                      <td>
                        {!n.link ? (
                          <span className="muted">{t('firmware.noLink')}</span>
                        ) : updating ? (
                          <span>
                            {t(`firmware.steps.${plc.step}`)} {plc.progress}%
                            <progress className="fw-progress" max={100} value={plc.progress} />
                          </span>
                        ) : (
                          t(`firmware.states.${n.state}`, { defaultValue: `${n.state}` })
                        )}
                      </td>
                      <td>
                        {n.result === 1 && <span className="badge badge--site">{t('firmware.resultOk')}</span>}
                        {n.result === 2 && (
                          <span className="badge badge--warn">
                            {t('firmware.resultFail', {
                              err: t(`firmware.nodeErrors.${n.err}`, { defaultValue: `0x${n.err.toString(16)}` }),
                              step: t(`firmware.steps.${n.step}`),
                            })}
                          </span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          <p className="field-hint">
            {t('firmware.stats', { ok: plc.stats.ok, failed: plc.stats.failed, retries: plc.stats.retries, up: uptime(plc.up) })}
          </p>
        </div>
      )}

      {plc && (
        <div className="card">
          <h2>{t('firmware.log')}</h2>
          <div className="table-wrap">
            <table className="data-table data-table--compact">
              <thead>
                <tr>
                  <th>{t('firmware.logTime')}</th>
                  <th>{t('firmware.logEvent')}</th>
                  <th>{t('firmware.moduleType')}</th>
                  <th>{t('firmware.tag')}</th>
                  <th>{t('firmware.logDetails')}</th>
                </tr>
              </thead>
              <tbody>
                {log.length === 0 && (
                  <tr>
                    <td colSpan={5} className="muted">{t('firmware.logEmpty')}</td>
                  </tr>
                )}
                {[...log].reverse().map((e) => (
                  <tr key={e.id}>
                    <td className="ltr-value nowrap">{uptime(e.t)}</td>
                    <td>{t(`firmware.events.${e.ev}`, { defaultValue: `${e.ev}` })}</td>
                    <td>{e.type ? typeName(e.type) : ''}</td>
                    <td className="ltr-value">{e.tag !== '000000' ? e.tag.toUpperCase() : ''}</td>
                    <td className="ltr-value">
                      {e.ev === 3
                        ? `${t(`firmware.states.${e.from}`)} → ${t(`firmware.states.${e.to}`)}`
                        : e.ev === EV_ID_WALK
                          ? t('firmware.walkFound', { count: e.from })
                          : e.ev >= EV_CFG_FIRST && e.ev < EV_ROLLBACK
                            ? ''
                            : [
                            !e.from || e.from === e.to ? (e.to ? ver(e.to) : '') : `${ver(e.from)} → ${e.to ? ver(e.to) : ''}`,
                            e.ev === 6 ? t('firmware.resultFail', {
                              err: t(`firmware.nodeErrors.${e.err}`, { defaultValue: `0x${e.err.toString(16)}` }),
                              step: t(`firmware.steps.${e.step}`),
                            }) : '',
                            e.ev === 12 ? t(`firmware.plcErrors.${e.err}`, { defaultValue: `${e.err}` }) : '',
                            e.ev === 9 && e.err ? t('firmware.runFailedCount', { count: e.err }) : '',
                          ].filter(Boolean).join(' · ')}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p className="field-hint">{t('firmware.logHint')}</p>
        </div>
      )}
    </div>
  );
}
