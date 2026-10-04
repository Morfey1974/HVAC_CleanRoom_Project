import type { LocalizedText } from '../api/client';
import { isLangCode, type LangCode } from '../i18n';

const ORDER: LangCode[] = ['he', 'ru', 'en'];

/** Name in the UI language; falls back to any filled language so the item is never blank. */
export function pickName(text: LocalizedText, lang: string): { value: string; fallback: boolean } {
  const code: LangCode = isLangCode(lang) ? lang : 'he';
  if (text[code]?.trim()) return { value: text[code], fallback: false };
  const other = ORDER.find((c) => text[c]?.trim());
  return { value: other ? text[other] : '', fallback: true };
}

const locale = (lang: string) => (lang === 'he' ? 'he-IL' : lang === 'ru' ? 'ru-RU' : 'en-GB');

export function formatDateTime(iso: string | null | undefined, lang: string) {
  if (!iso) return '';
  return new Date(iso).toLocaleString(locale(lang));
}

export function formatDate(iso: string | null | undefined, lang: string) {
  if (!iso) return '';
  return new Date(iso).toLocaleDateString(locale(lang));
}

export function formatSize(bytes: number) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}
