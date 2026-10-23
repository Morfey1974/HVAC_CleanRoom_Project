import type { ReactNode } from 'react';
import { Navigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

export function ProtectedRoute({ perm, children }: { perm: string; children: ReactNode }) {
  const { can } = useAuth();
  return can(perm) ? <>{children}</> : <Navigate to="/projects" replace />;
}
