/** Text in a mimic diagram: fixed string, user text in 3 languages, or an i18n key. */
export type Loc = string | { ru: string; en: string; he: string } | { i18n: string };

export type CardSpec = {
  title: Loc;
  channel?: string;
  valueTag?: string;
  unit?: string;
  decimals?: number;
};

/** One placed element: library type + position + static props + live data bindings. */
export type ElementInstance = {
  id: string;
  type: string;
  x: number;
  y: number;
  props?: Record<string, unknown>;
  /** element property name → PLC tag */
  bind?: Record<string, string>;
  /** matches ActiveAlarm.element: element blinks red while any such alarm is active */
  alarm?: string;
  card?: CardSpec;
};

export type Mnemonic = {
  id: string;
  name: Loc;
  width: number;
  height: number;
  elements: ElementInstance[];
};

export type ElementRenderProps = {
  inst: ElementInstance;
  /** bound values by property name; undefined = no data (sensor fault / no link) */
  v: Record<string, number | undefined>;
  p: Record<string, unknown>;
  alarm: boolean;
  text: (l: Loc | undefined) => string;
};
