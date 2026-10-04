import type { ReactNode } from 'react';
import { Navigate } from 'react-router-dom';
import type { Role } from '../api/client';
import { useAuth } from '../context/AuthContext';

export function ProtectedRoute({ minRole, children }: { minRole: Role; children: ReactNode }) {
  const { hasRole } = useAuth();
  return hasRole(minRole) ? <>{children}</> : <Navigate to="/projects" replace />;
}
