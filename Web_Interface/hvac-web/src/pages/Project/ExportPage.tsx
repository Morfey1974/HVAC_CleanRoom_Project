import { useCallback, useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { api, type PlcCfgMod, type PlcConfigIssue, type PlcConfigView } from '../../api/client';
import { useAuth } from '../../context/AuthContext';
import { useProject } from '../../context/ProjectContext';
import { useConfirm } from '../../components/Dialog';

const POLL_MS = 2000;
const ST_OK = 1;
const ST_MISSING = 2;
const ST_ERROR = 3;
const ST_APPLYING = 4;

/** Catalogue type code -> system prefix, for PLC rows that do not match the project. */
const PREFIX: Record<number, string> = {
  0x01: 'PLC', 0x02: 'DPLC', 0x10: 'LOC', 0x20: 'AI', 0x30: 'AO', 0x40: 'DI', 0x50: 'DO', 0x60: 'RL', 0x70: 'HUBD', 0x71: 'TFT', 0x80: 'HMI',
};

const ver = (v: number) => (v ? `${v >> 8}.${v & 0xff}` : '—');
const pad = (n: number) => n.toString().padStart(2, '0');

function duration(ms: number) {
  const s = Math.max(0, Math.floor(ms / 1000));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}:${pad(m)}:${pad(s % 60)}` : `${m}:${pad(s % 60)}`;
}

/** Project configuration -> Main PLC, and how the PLC applied it to the real modules. */
export function ExportPage() {
  const { t } = useTranslation();
  const { token } = useAuth();
  const { project, projectId } = useProject();
  const confirm = useConfirm();
  const [view, setView] = useState<PlcConfigView | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [info, setInfo] = useState('');

  const errText = useCallback(
    (code: string | null, plcError?: number | null) =>
      code === 'plc_rejected' && plcError != null
        ? t(`plcConfig.plcErrors.${plcError}`, { defaultValue: `${t('plcConfig.errors.plc_rejected')} (${plcError})` })
        : t(`plcConfig.errors.${code}`, { defaultValue: t(`firmware.errors.${code}`, { defaultValue: code ?? '' }) }),
    [t],
  );

  useEffect(() => {
    if (!token) return;
    let stop = false;
    let timer: ReturnType<typeof setTimeout>;
    const tick = async () => {
      try {
        const v = await api.plcConfig(token, projectId);
        if (!stop) setView((old) => (v.stale && old ? { ...v, plc: old.plc, matches: old.matches } : v));
      } catch (e) {
        if (!stop) setError((e as Error).message);
      }
      if (!stop) timer = setTimeout(tick, POLL_MS);
    };
    void tick();
    return () => {
      stop = true;
      clearTimeout(timer);
    };
  }, [token, projectId]);

  const upload = async () => {
    if (!token || !view) return;
    const reason = await confirm({
      title: t('plcConfig.upload'),
      message: t('plcConfig.confirmUpload', { project: view.built.project || '—' }),
      confirmText: t('plcConfig.upload'),
      reason: 'optional',
    });
    if (reason === null) return;
    setBusy(true);
    setError('');
    setInfo('');
    try {
      const r = await api.uploadPlcConfig(token, projectId, reason || undefined);
      if (r.ok) setInfo(t('plcConfig.uploaded', { gen: r.gen ?? '?' }));
      else setError(errText(r.error, r.plcError));
    } catch (e) {
      setError(errText((e as Error).message));
    } finally {
      setBusy(false);
    }
  };

  if (!view) {
    return (
      <div className="page">
        <h1>{t('projectNav.export')}</h1>
        {error ? <div className="error-banner">{error}</div> : <p className="muted">{t('common.loading')}</p>}
      </div>
    );
  }

  const b = view.built;
  const plc = view.plc;
  const blocking = b.issues.some((i) => i.blocking);
  const active = !!project?.isActiveOnSite;
  const canUpload = active && view.online && !blocking && !busy;

  const issueText = (i: PlcConfigIssue) => t(`plcConfig.issueCodes.${i.code}`, { module: i.module ?? '', channel: i.channel ?? '', defaultValue: i.code });

  const rows = plc?.present
    ? plc.mods.map((m, i) => {
        const own = view.matches ? b.modules[i] : undefined;
        return {
          key: i,
          name: own?.systemName ?? PREFIX[m.type] ?? `#${m.type}`,
          id: own?.id ?? (m.type === 0x01 ? '0.00.00' : `${m.l}.${pad(m.r)}.${pad(m.p)}`),
          channels: own?.channels ?? 0,
          m,
        };
      })
    : [];

  return (
    <div className="page cfg-page">
      <h1>{t('projectNav.export')}</h1>
      <p className="muted">{t('plcConfig.hint')}</p>
      {!active && (
        <div className="warning-banner">
          {t('plcConfig.notActive')} <Link to={`/p/${projectId}/overview`}>{t('projectNav.overview')}</Link>
        </div>
      )}
      {error && <div className="error-banner">{error}</div>}
      {info && <div className="success-banner">{info}</div>}

      <div className="cfg-grid">
        <div className="card">
          <h2>{t('plcConfig.project')}</h2>
          <div className="cfg-facts">
            <span>{t('plcConfig.modules')}</span>
            <b>{b.modules.length}</b>
            <span>{t('plcConfig.channels')}</span>
            <b>{b.channelCount}</b>
            <span>{t('plcConfig.checksum')}</span>
            <b className="ltr-value mono">{b.crc}</b>
          </div>
          <b className="small">{t('plcConfig.issues')}</b>
          {b.issues.length === 0 ? (
            <div className="small muted">{t('plcConfig.noIssues')}</div>
          ) : (
            <ul className="cfg-issues">
              {b.issues.map((i, n) => (
                <li key={n} className={i.blocking ? 'cfg-issue--block' : 'cfg-issue--warn'}>
                  {issueText(i)}
                </li>
              ))}
            </ul>
          )}
          <button type="button" className="btn btn-primary mt" disabled={!canUpload} onClick={upload}>
            {busy ? t('plcConfig.uploading') : t('plcConfig.upload')}
          </button>
        </div>

        <div className="card">
          <h2>{t('plcConfig.inPlc')}</h2>
          {!view.online ? (
            <div className="error-banner">
              {t('plcConfig.offline')}: {errText(view.error)}
            </div>
          ) : !plc ? (
            <div className="muted">{view.stale ? t('plcConfig.busy') : t('common.loading')}</div>
          ) : !plc.present ? (
            <div className="muted">{plc.flash ? t('plcConfig.none') : t('plcConfig.noFlash')}</div>
          ) : (
            <>
              <div className={`cfg-state cfg-state--${plc.state}`}>{t(`plcConfig.state.${plc.state}`)}</div>
              <div className="cfg-facts">
                <span>{t('plcConfig.projectInPlc')}</span>
                <b className="ltr-value">{plc.project || '—'}</b>
                <span>{t('plcConfig.gen')}</span>
                <b className="ltr-value">{plc.gen}</b>
                <span>{t('plcConfig.checksum')}</span>
                <b className="ltr-value mono">{plc.crc}</b>
                <span>{t('plcConfig.plcVersion')}</span>
                <b className="ltr-value">{ver(plc.plcVer)}</b>
              </div>
              <div className={view.matches ? 'small cfg-match' : 'small cfg-differs'}>
                {view.matches ? t('plcConfig.matches') : t('plcConfig.differs')}
              </div>
              {plc.state === ST_OK && plc.applied > 0 && (
                <div className="small muted">{t('plcConfig.appliedAgo', { time: duration(plc.up - plc.applied) })}</div>
              )}
              {plc.resends > 0 && <div className="small muted">{t('plcConfig.resends', { count: plc.resends })}</div>}
              {plc.extraLoco > 0 && <div className="small cfg-differs">{t('plcConfig.extraLoco', { count: plc.extraLoco })}</div>}
              {plc.extraAi > 0 && <div className="small cfg-differs">{t('plcConfig.extraAi')}</div>}
            </>
          )}
        </div>
      </div>

      {rows.length > 0 && (
        <div className="card mt">
          <h2>{t('plcConfig.check')}</h2>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>{t('plcConfig.module')}</th>
                  <th>ID</th>
                  <th>{t('plcConfig.status')}</th>
                  <th>{t('plcConfig.version')}</th>
                  <th>{t('plcConfig.channelsCol')}</th>
                  <th>{t('plcConfig.details')}</th>
                </tr>
              </thead>
              <tbody>
                {rows.map((r) => (
                  <tr key={r.key} className={r.m.st === ST_MISSING || r.m.st === ST_ERROR ? 'row-alarm' : ''}>
                    <td className="ltr-value">{r.name}</td>
                    <td className="ltr-value mono">{r.id}</td>
                    <td>
                      <span className={`cfg-st cfg-st--${r.m.st}`}>{t(`plcConfig.st.${r.m.st}`)}</span>
                    </td>
                    <td className="ltr-value">{ver(r.m.ver)}</td>
                    <td className="ltr-value">{channelText(r.m, r.channels)}</td>
                    <td className="small">{details(r.m)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );

  function details(m: PlcCfgMod) {
    if (m.st === ST_ERROR) return t(`plcConfig.moduleErr.${m.err}`, { defaultValue: `#${m.err}` });
    if (m.st === ST_APPLYING) return t('plcConfig.stHint.4');
    return t(`plcConfig.stHint.${m.st}`, { defaultValue: '' });
  }
}

/** "1 ✓ 2 ✗" for modules that confirm channels. */
function channelText(m: PlcCfgMod, count: number) {
  if (!m.ok && !m.bad) return '';
  const n = count || 8;
  const parts: string[] = [];
  for (let c = 1; c <= n; c++) {
    const bit = 1 << (c - 1);
    if (m.ok & bit) parts.push(`${c} ✓`);
    else if (m.bad & bit) parts.push(`${c} ✗`);
  }
  return parts.join('  ');
}
