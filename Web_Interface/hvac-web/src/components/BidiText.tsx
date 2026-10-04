import type { ElementType, HTMLAttributes } from 'react';

export const bidiAutoClassName = 'bidi-auto';

export function bidiAutoClass(...extra: Array<string | false | null | undefined>): string {
  return [bidiAutoClassName, ...extra.filter(Boolean)].join(' ');
}

type Props<T extends ElementType> = { as?: T } & HTMLAttributes<HTMLElement>;

/** User text (Hebrew + Latin) with correct direction in any UI language. */
export function BidiText<T extends ElementType = 'span'>({ as, className, ...props }: Props<T>) {
  const Tag = (as ?? 'span') as ElementType;
  return <Tag dir="auto" className={bidiAutoClass(className)} {...props} />;
}

export function bidiAutoInput(className?: string) {
  return { dir: 'auto' as const, className: bidiAutoClass(className) };
}
