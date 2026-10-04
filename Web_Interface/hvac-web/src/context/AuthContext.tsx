import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import { api, type AuthResponse, type Role } from '../api/client';
import { setUnauthorizedHandler } from '../api/http';

const ROLE_RANK: Record<Role, number> = { Viewer: 0, Operator: 1, Engineer: 2, Admin: 3 };

type AuthState = {
  token: string | null;
  user: AuthResponse | null;
  login: (login: string, password: string) => Promise<void>;
  logout: () => void;
  hasRole: (min: Role) => boolean;
};

const STORAGE_KEY = 'hvac_auth';
const AuthContext = createContext<AuthState | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<AuthResponse | null>(() => {
    const raw = sessionStorage.getItem(STORAGE_KEY);
    return raw ? (JSON.parse(raw) as AuthResponse) : null;
  });

  const persist = useCallback((data: AuthResponse | null) => {
    if (data) sessionStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    else sessionStorage.removeItem(STORAGE_KEY);
    setUser(data);
  }, []);

  const login = useCallback(async (l: string, p: string) => persist(await api.login(l, p)), [persist]);
  const logout = useCallback(() => persist(null), [persist]);
  const hasRole = useCallback((min: Role) => !!user && ROLE_RANK[user.role] >= ROLE_RANK[min], [user]);

  useEffect(() => {
    setUnauthorizedHandler(logout);
    return () => setUnauthorizedHandler(null);
  }, [logout]);

  const value = useMemo(
    () => ({ token: user?.token ?? null, user, login, logout, hasRole }),
    [user, login, logout, hasRole]
  );
  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth outside AuthProvider');
  return ctx;
}
