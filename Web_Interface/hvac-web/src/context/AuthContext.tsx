import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import { api, type AuthResponse } from '../api/client';
import { setUnauthorizedHandler } from '../api/http';

type AuthState = {
  token: string | null;
  user: AuthResponse | null;
  login: (login: string, password: string) => Promise<void>;
  logout: () => void;
  can: (perm: string) => boolean;
};

const STORAGE_KEY = 'hvac_auth';
const AuthContext = createContext<AuthState | null>(null);

function readAuthRaw(): string | null {
  const own = sessionStorage.getItem(STORAGE_KEY);
  if (own) return own;
  try {
    const fromOpener = window.opener?.sessionStorage?.getItem(STORAGE_KEY) as string | null | undefined;
    if (fromOpener) {
      sessionStorage.setItem(STORAGE_KEY, fromOpener);
      return fromOpener;
    }
  } catch {
    /* popup cannot read opener */
  }
  return null;
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<AuthResponse | null>(() => {
    const raw = readAuthRaw();
    if (!raw) return null;
    const data = JSON.parse(raw) as AuthResponse;
    if (Array.isArray(data.permissions)) return data;
    sessionStorage.removeItem(STORAGE_KEY);
    return null;
  });

  const persist = useCallback((data: AuthResponse | null) => {
    if (data) sessionStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    else sessionStorage.removeItem(STORAGE_KEY);
    setUser(data);
  }, []);

  const login = useCallback(async (l: string, p: string) => persist(await api.login(l, p)), [persist]);
  const logout = useCallback(() => persist(null), [persist]);
  const can = useCallback((perm: string) => !!user?.permissions?.includes(perm), [user]);

  useEffect(() => {
    setUnauthorizedHandler(logout);
    return () => setUnauthorizedHandler(null);
  }, [logout]);

  const value = useMemo(() => ({ token: user?.token ?? null, user, login, logout, can }), [user, login, logout, can]);
  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth outside AuthProvider');
  return ctx;
}
