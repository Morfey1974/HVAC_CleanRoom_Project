import type { CSSProperties, ReactNode } from 'react';
import type { GraphicLabel, GraphicPort, ModuleGraphic, PortType } from '../api/client';

export const PORT_COLORS: Record<PortType, string> = {
  can: '#2563eb',
  rs485: '#0891b2',
  eth: '#16a34a',
  pwr24: '#dc2626',
  ai: '#d97706',
  ao: '#ea580c',
  di: '#0369a1',
  do: '#be185d',
  relay: '#475569',
  usb: '#4b5563',
  other: '#64748b',
};

export const portColor = (p: GraphicPort) => p.color || PORT_COLORS[p.type] || PORT_COLORS.other;

export const HEADER_H = 26;

export const emptyGraphic = (): ModuleGraphic => ({
  width: 180,
  height: 120,
  fill: '#f8fafc',
  header: '#475569',
  headerText: '#ffffff',
  textColor: '#1f2937',
  border: '#475569',
  title: '',
  subtitle: '',
  showArticle: true,
  ports: [],
  labels: [],
});

/** Which side of the block the point sits on: its caption goes inwards from there. */
export function portSide(p: Pick<GraphicPort, 'x' | 'y'>): 'top' | 'bottom' | 'left' | 'right' | 'inside' {
  if (p.y <= 0.02) return 'top';
  if (p.y >= 0.98) return 'bottom';
  if (p.x <= 0.02) return 'left';
  if (p.x >= 0.98) return 'right';
  return 'inside';
}

export const posStyle = (x: number, y: number): CSSProperties => ({ left: `${x * 100}%`, top: `${y * 100}%` });

type Props = {
  graphic: ModuleGraphic;
  article: string;
  name?: string;
  systemName?: string;
  moduleId?: string;
  warn?: boolean;
  selected?: boolean;
  scale?: number;
  /** Draws one connection point; default is a plain dot. */
  renderPort?: (p: GraphicPort) => ReactNode;
  renderLabel?: (l: GraphicLabel) => ReactNode;
  children?: ReactNode;
};

/** Module block drawn from its library drawing: header, texts, captions and connection points. */
export function ModuleBlock({ graphic: g, article, name, systemName, moduleId, warn, selected, scale = 1, renderPort, renderLabel, children }: Props) {
  const title = g.title || (g.showArticle ? article : '');
  return (
    <div
      className={`mblock${selected ? ' mblock--selected' : ''}${warn ? ' mblock--warn' : ''}`}
      style={{ width: g.width * scale, height: g.height * scale, background: g.fill, borderColor: g.border, color: g.textColor, fontSize: 12 * scale }}
    >
      <div className="mblock__head" style={{ background: g.header, color: g.headerText, height: HEADER_H * scale }} dir="ltr">
        <span>{title}</span>
        {systemName && <b>{systemName}</b>}
      </div>
      <div className="mblock__body">
        {g.showArticle && g.title && <div className="mblock__article" dir="ltr">{article}</div>}
        {g.subtitle && <div>{g.subtitle}</div>}
        {name && <div className="mblock__name">{name}</div>}
        {moduleId && <div className="mblock__id" dir="ltr">{moduleId}</div>}
      </div>
      {g.labels.map((l) =>
        renderLabel ? (
          renderLabel(l)
        ) : (
          <span key={l.id} className="mblock__label" style={{ ...posStyle(l.x, l.y), color: l.color || undefined, fontSize: l.size * scale, fontWeight: l.bold ? 700 : 400 }}>
            {l.text}
          </span>
        ),
      )}
      {g.ports.map((p) => (renderPort ? renderPort(p) : <PortDot key={p.id} port={p} scale={scale} />))}
      {children}
    </div>
  );
}

export function PortCaption({ port, scale = 1 }: { port: GraphicPort; scale?: number }) {
  return (
    <span className={`mport__caption mport__caption--${portSide(port)}`} style={{ fontSize: 10 * scale }} dir="ltr">
      {port.label}
    </span>
  );
}

export function PortDot({ port, scale = 1, className = '', ...rest }: { port: GraphicPort; scale?: number; className?: string } & React.HTMLAttributes<HTMLDivElement>) {
  return (
    <div className={`mport ${className}`} style={posStyle(port.x, port.y)} {...rest}>
      <span className={`mport__dot mport__dot--${port.dir}`} style={{ background: portColor(port) }} />
      <PortCaption port={port} scale={scale} />
    </div>
  );
}
