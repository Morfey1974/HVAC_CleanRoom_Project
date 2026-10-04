import { useCallback, useState, type MouseEvent } from 'react';
import { useTranslation } from 'react-i18next';
import { elementLibrary, formatValue } from './library';
import type { ElementInstance, Loc, Mnemonic } from './types';
import { isLangCode } from '../i18n';

type Props = {
  mnemonic: Mnemonic;
  values: Record<string, number>;
  alarmElements: Set<string>;
};

type Card = { inst: ElementInstance; x: number; y: number };

/** Renders a mimic diagram: every element from the library, bound to live tag values. */
export function MnemoView({ mnemonic, values, alarmElements }: Props) {
  const { t, i18n } = useTranslation();
  const [card, setCard] = useState<Card | null>(null);

  const text = useCallback(
    (l: Loc | undefined): string => {
      if (l === undefined) return '';
      if (typeof l === 'string') return l;
      if ('i18n' in l) return t(l.i18n);
      const code = isLangCode(i18n.language) ? i18n.language : 'he';
      return l[code] || l.he || l.ru || l.en;
    },
    [t, i18n.language]
  );

  const onClick = (inst: ElementInstance) => (e: MouseEvent) => {
    if (!inst.card) return;
    e.stopPropagation();
    setCard({ inst, x: e.clientX, y: e.clientY });
  };

  return (
    <div className="mn-wrap" onClick={() => setCard(null)}>
      <svg className="mn-svg" viewBox={`0 0 ${mnemonic.width} ${mnemonic.height}`}>
        <defs>
          <linearGradient id="mnMetal" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" stopColor="#e2e8f0" />
            <stop offset="1" stopColor="#94a3b8" />
          </linearGradient>
          <linearGradient id="mnTop" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stopColor="#f1f5f9" />
            <stop offset="1" stopColor="#cbd5e1" />
          </linearGradient>
          <linearGradient id="mnSide" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stopColor="#64748b" />
            <stop offset="1" stopColor="#475569" />
          </linearGradient>
          <pattern id="mnFilter" width="8" height="8" patternUnits="userSpaceOnUse">
            <path d="M0 8 L8 0 M-2 2 L2 -2 M6 10 L10 6" stroke="#94a3b8" strokeWidth="1.2" />
          </pattern>
        </defs>
        {mnemonic.elements.map((inst) => {
          const R = elementLibrary[inst.type];
          if (!R) return null;
          const v: Record<string, number | undefined> = {};
          for (const [prop, tag] of Object.entries(inst.bind ?? {})) v[prop] = values[tag];
          const alarm = !!inst.alarm && alarmElements.has(inst.alarm);
          return (
            <g
              key={inst.id}
              transform={`translate(${inst.x} ${inst.y})`}
              className={[inst.card ? 'mn-click' : '', alarm && inst.type !== 'room' ? 'mn-alarm' : ''].join(' ')}
              onClick={onClick(inst)}
            >
              <R inst={inst} v={v} p={inst.props ?? {}} alarm={alarm} text={text} />
            </g>
          );
        })}
      </svg>
      {card?.inst.card && (
        <div className="mn-card card" style={{ left: Math.min(card.x + 12, window.innerWidth - 280), top: card.y + 12 }}>
          <strong>{text(card.inst.card.title)}</strong>
          {card.inst.card.valueTag && (
            <div>
              {t('mnemo.value')}:{' '}
              <span className="ltr-value">
                {formatValue(values[card.inst.card.valueTag], card.inst.card.decimals ?? 1, card.inst.card.unit ?? '')}
              </span>
            </div>
          )}
          {card.inst.card.channel && (
            <div>
              {t('mnemo.channel')}: <span className="ltr-value">{card.inst.card.channel}</span>
            </div>
          )}
          {card.inst.alarm && alarmElements.has(card.inst.alarm) && <div className="mn-card__alarm">{t('mnemo.fault')}</div>}
        </div>
      )}
    </div>
  );
}
