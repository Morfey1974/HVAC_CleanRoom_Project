import { createContext, useCallback, useContext, useEffect, useRef, useState, type ReactNode } from 'react';
import { useTranslation } from 'react-i18next';
import { bidiAutoInput } from './BidiText';
import { Modal } from './Modal';

export type ConfirmOptions = {
  title: string;
  message?: ReactNode;
  confirmText?: string;
  danger?: boolean;
  /** Reason field for the audit log. */
  reason?: 'none' | 'optional' | 'required';
  reasonLabel?: string;
};

/** Resolves to the entered reason ('' when none) or null when cancelled. */
type Confirm = (o: ConfirmOptions) => Promise<string | null>;

const DialogContext = createContext<Confirm | null>(null);

type Pending = ConfirmOptions & { resolve: (v: string | null) => void };

export function DialogProvider({ children }: { children: ReactNode }) {
  const [pending, setPending] = useState<Pending | null>(null);
  const confirm = useCallback<Confirm>((o) => new Promise((resolve) => setPending({ ...o, resolve })), []);
  return (
    <DialogContext.Provider value={confirm}>
      {children}
      {pending && (
        <ConfirmDialog
          key={pending.title}
          {...pending}
          onClose={(v) => {
            pending.resolve(v);
            setPending(null);
          }}
        />
      )}
    </DialogContext.Provider>
  );
}

export function useConfirm(): Confirm {
  const c = useContext(DialogContext);
  if (!c) throw new Error('DialogProvider missing');
  return c;
}

function ConfirmDialog({
  title,
  message,
  confirmText,
  danger,
  reason = 'none',
  reasonLabel,
  onClose,
}: ConfirmOptions & { onClose: (v: string | null) => void }) {
  const { t } = useTranslation();
  const [text, setText] = useState('');
  const okRef = useRef<HTMLButtonElement>(null);
  const blocked = reason === 'required' && !text.trim();

  const accept = useCallback(() => {
    if (!blocked) onClose(text.trim());
  }, [blocked, text, onClose]);

  useEffect(() => {
    if (reason === 'none') okRef.current?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose(null);
      if (e.key === 'Enter' && !(e.target instanceof HTMLTextAreaElement)) {
        e.preventDefault();
        accept();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [reason, accept, onClose]);

  return (
    <Modal sizeKey="confirm" className="confirm-dialog" role="alertdialog" top closeOnEscape={false} onClose={() => onClose(null)}>
      <h2 className={danger ? 'confirm-dialog__title--danger' : undefined}>{title}</h2>
      {message && <div className="confirm-dialog__message">{message}</div>}
      {reason !== 'none' && (
        <label className="confirm-dialog__reason">
          {reasonLabel ?? t('common.reason')}
          {reason === 'required' ? ' *' : ` (${t('dialog.optional')})`}
          <textarea rows={2} autoFocus value={text} onChange={(e) => setText(e.target.value)} {...bidiAutoInput()} />
        </label>
      )}
      <div className="modal-actions">
        <button type="button" className="btn btn-ghost-inline" onClick={() => onClose(null)}>
          {t('common.cancel')}
        </button>
        <button ref={okRef} type="button" className={`btn ${danger ? 'btn-danger' : 'btn-primary'}`} disabled={blocked} onClick={accept}>
          {confirmText ?? t('dialog.ok')}
        </button>
      </div>
    </Modal>
  );
}
