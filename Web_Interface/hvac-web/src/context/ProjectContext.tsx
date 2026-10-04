import { createContext, useCallback, useContext, useEffect, useState, type ReactNode } from 'react';
import { api, type Project } from '../api/client';
import { useAuth } from './AuthContext';

type ProjectState = {
  projectId: string;
  project: Project | null;
  notFound: boolean;
  reload: () => void;
  setProject: (p: Project) => void;
};

const ProjectContext = createContext<ProjectState | null>(null);

export function ProjectProvider({ projectId, children }: { projectId: string; children: ReactNode }) {
  const { token } = useAuth();
  const [project, setProject] = useState<Project | null>(null);
  const [notFound, setNotFound] = useState(false);

  const reload = useCallback(() => {
    if (!token) return;
    api
      .project(token, projectId)
      .then((p) => {
        setProject(p);
        setNotFound(false);
      })
      .catch(() => setNotFound(true));
  }, [token, projectId]);

  useEffect(() => {
    setProject(null);
    reload();
  }, [reload]);

  return <ProjectContext.Provider value={{ projectId, project, notFound, reload, setProject }}>{children}</ProjectContext.Provider>;
}

export function useProject() {
  const ctx = useContext(ProjectContext);
  if (!ctx) throw new Error('useProject must be used inside ProjectProvider');
  return ctx;
}
