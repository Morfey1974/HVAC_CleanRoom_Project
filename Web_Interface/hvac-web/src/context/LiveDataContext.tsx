import { createContext, useCallback, useContext, useEffect, useRef, useState, type ReactNode } from 'react';
import { HubConnectionBuilder, HubConnectionState, LogLevel, type HubConnection } from '@microsoft/signalr';
import { useAuth } from './AuthContext';

export type ActiveAlarm = { code: string; element: string };

export type LiveSnapshot = {
  at: string;
  mode: string;
  connected: boolean;
  values: Record<string, number>;
  alarms: ActiveAlarm[];
};

type LiveState = {
  snapshot: LiveSnapshot | null;
  linkUp: boolean;
  simInputs: Record<string, number>;
  alarmsVersion: number;
  setSimInput: (key: string, value: number) => Promise<boolean>;
};

const LiveContext = createContext<LiveState | null>(null);

export function LiveDataProvider({ children }: { children: ReactNode }) {
  const { token } = useAuth();
  const [snapshot, setSnapshot] = useState<LiveSnapshot | null>(null);
  const [linkUp, setLinkUp] = useState(false);
  const [simInputs, setSimInputs] = useState<Record<string, number>>({});
  const [alarmsVersion, setAlarmsVersion] = useState(0);
  const connRef = useRef<HubConnection | null>(null);

  useEffect(() => {
    if (!token) return;
    const conn = new HubConnectionBuilder()
      .withUrl('/hubs/live', { accessTokenFactory: () => token })
      .withAutomaticReconnect({ nextRetryDelayInMilliseconds: () => 3000 })
      .configureLogging(LogLevel.Warning)
      .build();
    connRef.current = conn;
    conn.on('snapshot', (s: LiveSnapshot) => setSnapshot(s));
    conn.on('simInputs', (s: Record<string, number>) => setSimInputs(s));
    conn.on('alarmsChanged', () => setAlarmsVersion((v) => v + 1));
    conn.onreconnecting(() => setLinkUp(false));
    conn.onreconnected(() => setLinkUp(true));
    conn.onclose(() => setLinkUp(false));

    let stopped = false;
    const start = async () => {
      try {
        await conn.start();
        if (!stopped) setLinkUp(true);
      } catch {
        if (!stopped) setTimeout(start, 3000);
      }
    };
    void start();
    return () => {
      stopped = true;
      connRef.current = null;
      void conn.stop();
    };
  }, [token]);

  const setSimInput = useCallback(async (key: string, value: number) => {
    const c = connRef.current;
    if (!c || c.state !== HubConnectionState.Connected) return false;
    return c.invoke<boolean>('SetSimInput', key, value);
  }, []);

  return (
    <LiveContext.Provider value={{ snapshot, linkUp, simInputs, alarmsVersion, setSimInput }}>
      {children}
    </LiveContext.Provider>
  );
}

export function useLive() {
  const ctx = useContext(LiveContext);
  if (!ctx) throw new Error('useLive outside LiveDataProvider');
  return ctx;
}
