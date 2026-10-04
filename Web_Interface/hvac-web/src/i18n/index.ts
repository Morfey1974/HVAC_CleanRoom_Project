import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';
import ru from './locales/ru';
import en from './locales/en';
import he from './locales/he';

export type LangCode = 'he' | 'ru' | 'en';

export const languages: { code: LangCode; label: string; dir: 'rtl' | 'ltr' }[] = [
  { code: 'he', label: 'עברית', dir: 'rtl' },
  { code: 'ru', label: 'Русский', dir: 'ltr' },
  { code: 'en', label: 'English', dir: 'ltr' },
];

const LANG_KEY = 'hvac_lang';

export function isLangCode(v: unknown): v is LangCode {
  return v === 'he' || v === 'ru' || v === 'en';
}

const saved = localStorage.getItem(LANG_KEY);

i18n.use(initReactI18next).init({
  resources: {
    ru: { translation: ru },
    en: { translation: en },
    he: { translation: he },
  },
  lng: isLangCode(saved) ? saved : 'he',
  fallbackLng: 'he',
  interpolation: { escapeValue: false },
});

export function applyDocumentDirection(lang: string) {
  const meta = languages.find((l) => l.code === lang) ?? languages[0];
  document.documentElement.lang = meta.code;
  document.documentElement.dir = meta.dir;
}

/** User's explicit choice; remembered across sessions. */
export function changeLanguage(code: LangCode) {
  localStorage.setItem(LANG_KEY, code);
  void i18n.changeLanguage(code);
}

/** Startup language from system settings, used only until the user picks one. */
export function applyStartupLanguage(code: string) {
  if (!localStorage.getItem(LANG_KEY) && isLangCode(code) && i18n.language !== code) {
    void i18n.changeLanguage(code);
  }
}

applyDocumentDirection(i18n.language);
i18n.on('languageChanged', applyDocumentDirection);

export default i18n;
