import { useState, type FormEvent } from 'react';
import { useTranslation } from 'react-i18next';
import { useAuth } from '../context/AuthContext';
import { changeLanguage, isLangCode, languages } from '../i18n';

export function LoginPage() {
  const { t, i18n } = useTranslation();
  const { login } = useAuth();
  const [form, setForm] = useState({ login: '', password: '' });
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  const submit = async (e: FormEvent) => {
    e.preventDefault();
    setBusy(true);
    setError('');
    try {
      await login(form.login, form.password);
    } catch {
      setError(t('login.invalid'));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="auth-page">
      <form className="auth-card" onSubmit={submit}>
        <div className="auth-card__top">
          <h1>{t('appTitle')}</h1>
          <select
            className="lang-select lang-select--bordered"
            value={i18n.language}
            onChange={(e) => isLangCode(e.target.value) && changeLanguage(e.target.value)}
            aria-label={t('language')}
          >
            {languages.map((l) => (
              <option key={l.code} value={l.code}>
                {l.label}
              </option>
            ))}
          </select>
        </div>
        <p className="muted">{t('login.title')}</p>
        {error && <div className="error-banner">{error}</div>}
        <label>
          {t('login.login')}
          <input value={form.login} onChange={(e) => setForm({ ...form, login: e.target.value })} autoFocus dir="ltr" />
        </label>
        <label>
          {t('login.password')}
          <input type="password" value={form.password} onChange={(e) => setForm({ ...form, password: e.target.value })} dir="ltr" />
        </label>
        <button type="submit" className="btn btn-primary" disabled={busy || !form.login}>
          {t('login.submit')}
        </button>
      </form>
    </div>
  );
}
