import { useEffect } from 'react';
import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom';
import { AuthProvider, useAuth } from './context/AuthContext';
import { LiveDataProvider } from './context/LiveDataContext';
import { Layout } from './components/Layout';
import { ProtectedRoute } from './components/ProtectedRoute';
import { ActiveProjectRedirect } from './components/ActiveProjectRedirect';
import { LoginPage } from './pages/LoginPage';
import { ProjectsPage } from './pages/Projects/ProjectsPage';
import { ProjectWizard } from './pages/Projects/ProjectWizard';
import { LibraryPage } from './pages/Library/LibraryPage';
import { SettingsPage } from './pages/Settings/SettingsPage';
import { HmiLibraryPage } from './pages/Hmi/HmiLibraryPage';
import { ProjectRoutes } from './pages/Project/ProjectRoutes';
import { api } from './api/client';
import { applyStartupLanguage } from './i18n';

function AppRoutes() {
  const { user } = useAuth();

  if (!user) {
    return (
      <Routes>
        <Route path="*" element={<LoginPage />} />
      </Routes>
    );
  }

  return (
    <LiveDataProvider>
      <Routes>
        <Route element={<Layout />}>
          <Route path="/" element={<Navigate to="/projects" replace />} />
          <Route path="/projects" element={<ProjectsPage />} />
          <Route
            path="/projects/new"
            element={
              <ProtectedRoute minRole="Engineer">
                <ProjectWizard />
              </ProtectedRoute>
            }
          />
          <Route path="/library" element={<Navigate to="/library/module" replace />} />
          <Route path="/library/:category" element={<LibraryPage />} />
          <Route path="/hmi-library" element={<HmiLibraryPage />} />
          <Route path="/system/*" element={<SettingsPage />} />
          <Route path="/p/:projectId/*" element={<ProjectRoutes />} />
          <Route path="/active/:section" element={<ActiveProjectRedirect />} />
          <Route path="*" element={<Navigate to="/projects" replace />} />
        </Route>
      </Routes>
    </LiveDataProvider>
  );
}

export default function App() {
  useEffect(() => {
    api.publicSettings().then((s) => applyStartupLanguage(s.startupLanguage)).catch(() => undefined);
  }, []);

  return (
    <BrowserRouter>
      <AuthProvider>
        <AppRoutes />
      </AuthProvider>
    </BrowserRouter>
  );
}
