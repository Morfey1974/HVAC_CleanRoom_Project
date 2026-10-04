const API_BASE = import.meta.env.VITE_API_URL ?? '';

let onUnauthorized: (() => void) | null = null;

export function setUnauthorizedHandler(handler: (() => void) | null) {
  onUnauthorized = handler;
}

export class ApiError extends Error {
  constructor(public status: number, message: string) {
    super(message);
  }
}

async function send(path: string, options: RequestInit, token?: string | null): Promise<Response> {
  const headers: Record<string, string> = { ...(options.headers as Record<string, string>) };
  // FormData needs the browser-generated multipart boundary header.
  if (!(options.body instanceof FormData)) headers['Content-Type'] ??= 'application/json';
  if (token) headers.Authorization = `Bearer ${token}`;

  const res = await fetch(`${API_BASE}${path}`, { ...options, headers });
  if (!res.ok) {
    if (res.status === 401 && token) onUnauthorized?.();
    const body = ((await res.json().catch(() => undefined)) ?? {}) as { message?: string };
    throw new ApiError(res.status, body.message ?? res.statusText);
  }
  return res;
}

export async function request<T>(path: string, options: RequestInit = {}, token?: string | null): Promise<T> {
  const res = await send(path, options, token);
  return (res.status === 204 ? undefined : await res.json().catch(() => undefined)) as T;
}

/** Downloads a protected file (Authorization header cannot be sent by a plain link). */
export async function downloadFile(path: string, fileName: string, token: string) {
  const res = await send(path, {}, token);
  const url = URL.createObjectURL(await res.blob());
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
