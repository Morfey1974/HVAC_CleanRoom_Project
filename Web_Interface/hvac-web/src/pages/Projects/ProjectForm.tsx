import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import type { Project, ProjectSave } from '../../api/client';
import { bidiAutoInput } from '../../components/BidiText';
import type { ConfirmOptions } from '../../components/Dialog';

type Confirm = (o: ConfirmOptions) => Promise<string | null>;

export const emptyProject: ProjectSave = {
  name: { ru: '', en: '', he: '' },
  number: '',
  customer: '',
  address: '',
  responsible: '',
  isoClass: null,
  description: '',
};

export const toProjectSave = (p: Project): ProjectSave => ({
  name: { ...p.name },
  number: p.number,
  customer: p.customer,
  address: p.address,
  responsible: p.responsible,
  isoClass: p.isoClass,
  description: p.description,
});

/** Archive / restore question; resolves to reason or null when cancelled. */
export const confirmArchive = (confirm: Confirm, t: TFunction, isArchived: boolean, name: string) =>
  isArchived
    ? confirm({ title: t('projects.restore'), message: t('projects.restoreConfirm', { name }), confirmText: t('projects.restore') })
    : confirm({
        title: t('projects.toArchive'),
        message: t('projects.archiveConfirm', { name }),
        confirmText: t('projects.toArchive'),
        danger: true,
        reason: 'optional',
        reasonLabel: t('projects.archiveReason'),
      });

export const hasProjectName = (p: ProjectSave) => !!(p.name.ru.trim() || p.name.en.trim() || p.name.he.trim());

/** General project data; shared by the create wizard and the overview edit dialog. */
export function ProjectForm({ value, onChange }: { value: ProjectSave; onChange: (v: ProjectSave) => void }) {
  const { t } = useTranslation();
  const text = (k: 'number' | 'customer' | 'address' | 'responsible') => (
    <label>
      {t(`projects.fields.${k}`)}
      <input value={value[k]} onChange={(e) => onChange({ ...value, [k]: e.target.value })} {...bidiAutoInput()} />
    </label>
  );

  return (
    <div className="form-grid form-grid--wide">
      <div className="form-grid-3">
        {(['he', 'ru', 'en'] as const).map((l) => (
          <label key={l} className={value.name[l].trim() ? '' : 'field-missing'}>
            {t(`projects.fields.name_${l}`)}
            <input
              value={value.name[l]}
              onChange={(e) => onChange({ ...value, name: { ...value.name, [l]: e.target.value } })}
              {...bidiAutoInput()}
            />
          </label>
        ))}
      </div>
      <span className="field-hint">{t('projects.nameHint')}</span>
      <div className="form-grid-2">
        {text('number')}
        {text('customer')}
        {text('address')}
        {text('responsible')}
        <label>
          {t('projects.fields.isoClass')}
          <select
            value={value.isoClass ?? ''}
            onChange={(e) => onChange({ ...value, isoClass: e.target.value === '' ? null : Number(e.target.value) })}
          >
            <option value="">—</option>
            {[5, 6, 7, 8, 9].map((c) => (
              <option key={c} value={c}>
                ISO {c}
              </option>
            ))}
          </select>
        </label>
      </div>
      <label>
        {t('projects.fields.description')}
        <textarea rows={3} value={value.description} onChange={(e) => onChange({ ...value, description: e.target.value })} {...bidiAutoInput()} />
      </label>
    </div>
  );
}
