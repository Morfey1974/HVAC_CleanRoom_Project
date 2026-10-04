import { useEffect, useLayoutEffect, useRef, type MouseEvent as ReactMouseEvent, type ReactNode } from 'react';
import { useTranslation } from 'react-i18next';

type Props = {
  /** localStorage key suffix: the size the user dragged to is restored next time. */
  sizeKey: string;
  onClose: () => void;
  children: ReactNode;
  className?: string;
  closeOnEscape?: boolean;
  top?: boolean;
  role?: 'dialog' | 'alertdialog';
};

const MIN_W = 320;
const MIN_H = 180;
const storageKey = (k: string) => `hvac.modal-size.${k}`;

const clamp = (w: number, h: number) => ({
  width: Math.round(Math.min(window.innerWidth * 0.96, Math.max(MIN_W, w))),
  height: Math.round(Math.min(window.innerHeight * 0.92, Math.max(MIN_H, h))),
});

/** Every app window: centered, closes by Cancel / Escape (not by a backdrop click), resizable by the corner grip. */
export function Modal({ sizeKey, onClose, children, className = '', closeOnEscape = true, top, role = 'dialog' }: Props) {
  const { t } = useTranslation();
  const panelRef = useRef<HTMLDivElement>(null);
  const drag = useRef<{ x: number; y: number; w: number; h: number; rtl: boolean } | null>(null);

  useLayoutEffect(() => {
    const el = panelRef.current;
    if (!el) return;
    try {
      const saved = JSON.parse(localStorage.getItem(storageKey(sizeKey)) ?? 'null') as { width: number; height: number } | null;
      if (saved && typeof saved.width === 'number' && typeof saved.height === 'number') {
        const s = clamp(saved.width, saved.height);
        el.style.width = `${s.width}px`;
        el.style.height = `${s.height}px`;
      }
    } catch {
      /* broken saved size — keep the default */
    }
  }, [sizeKey]);

  useEffect(() => {
    if (!closeOnEscape) return;
    const onKey = (e: KeyboardEvent) => e.key === 'Escape' && onClose();
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [closeOnEscape, onClose]);

  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current;
      const el = panelRef.current;
      if (!d || !el) return;
      // The window is centered, so it grows to both sides: the corner follows the cursor at double delta.
      const dx = (e.clientX - d.x) * (d.rtl ? -2 : 2);
      const dy = (e.clientY - d.y) * 2;
      const s = clamp(d.w + dx, d.h + dy);
      el.style.width = `${s.width}px`;
      el.style.height = `${s.height}px`;
    };
    const onUp = () => {
      const el = panelRef.current;
      if (!drag.current) return;
      drag.current = null;
      document.body.style.removeProperty('user-select');
      document.body.style.removeProperty('cursor');
      if (el) localStorage.setItem(storageKey(sizeKey), JSON.stringify(clamp(el.offsetWidth, el.offsetHeight)));
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      document.body.style.removeProperty('user-select');
      document.body.style.removeProperty('cursor');
    };
  }, [sizeKey]);

  const startResize = (e: ReactMouseEvent<HTMLDivElement>) => {
    const el = panelRef.current;
    if (!el) return;
    e.preventDefault();
    e.stopPropagation();
    drag.current = { x: e.clientX, y: e.clientY, w: el.offsetWidth, h: el.offsetHeight, rtl: getComputedStyle(el).direction === 'rtl' };
    document.body.style.userSelect = 'none';
    document.body.style.cursor = getComputedStyle(el).direction === 'rtl' ? 'nesw-resize' : 'nwse-resize';
  };

  const resetSize = () => {
    const el = panelRef.current;
    if (!el) return;
    el.style.removeProperty('width');
    el.style.removeProperty('height');
    localStorage.removeItem(storageKey(sizeKey));
  };

  return (
    <div className={top ? 'modal-overlay modal-overlay--top' : 'modal-overlay'} role="presentation">
      <div ref={panelRef} className={`card modal modal--resizable ${className}`.trim()} role={role} aria-modal="true">
        <div className="modal__body">{children}</div>
        <div className="modal__resize-handle" onMouseDown={startResize} onDoubleClick={resetSize} title={t('common.resizeModal')} aria-hidden />
      </div>
    </div>
  );
}
