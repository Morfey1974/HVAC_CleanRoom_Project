import { useTranslation } from 'react-i18next';

export function StubPage({ section }: { section: string }) {
  const { t } = useTranslation();
  return (
    <div className="page">
      <h1>{t(`projectNav.${section}`)}</h1>
      <div className="card stub">
        <p>{t(`stubs.${section}`)}</p>
        <p className="muted">{t('stubs.notYet')}</p>
      </div>
    </div>
  );
}
