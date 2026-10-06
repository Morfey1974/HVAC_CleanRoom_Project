import { downloadFile, request } from './http';

export type Role = 'Viewer' | 'Operator' | 'Engineer' | 'Admin';

export type AuthResponse = {
  token: string;
  userId: string;
  login: string;
  fullName: string;
  role: Role;
};

export type LocalizedText = { ru: string; en: string; he: string };

export type Project = {
  id: string;
  name: LocalizedText;
  number: string;
  customer: string;
  address: string;
  responsible: string;
  isoClass: number | null;
  description: string;
  isArchived: boolean;
  isActiveOnSite: boolean;
  createdAt: string;
  createdBy: string;
  updatedAt: string;
  roomCount: number;
  documentCount: number;
  equipmentCount: number;
  moduleCount: number;
};

export type ProjectSave = {
  name: LocalizedText;
  number: string;
  customer: string;
  address: string;
  responsible: string;
  isoClass: number | null;
  description: string;
  reason?: string;
};

export type Room = {
  id: string;
  name: LocalizedText;
  isoClass: number | null;
  areaM2: number | null;
  heightM: number | null;
  tempSetpointC: number | null;
  tempToleranceC: number | null;
  rhSetpointPct: number | null;
  rhTolerancePct: number | null;
  pressureSetpointPa: number | null;
  pressureTolerancePa: number | null;
  sortOrder: number;
};

export type RoomSave = Omit<Room, 'id'> & { reason?: string };

export type DocumentKind = 'pid' | 'aoi' | 'other';
export const documentKinds: DocumentKind[] = ['pid', 'aoi', 'other'];

export type ProjectDocument = {
  id: string;
  kind: DocumentKind;
  fileName: string;
  contentType: string;
  sizeBytes: number;
  uploadedAt: string;
  uploadedBy: string;
};

export type LibraryCategory = 'module' | 'sensor' | 'actuator' | 'assembly' | 'graphic' | 'regulator';
export const libraryCategories: LibraryCategory[] = ['module', 'sensor', 'actuator', 'assembly', 'graphic', 'regulator'];

export type LibraryProp = { key: string; value: string; unit: string };

export type LibraryItem = {
  id: string;
  category: LibraryCategory;
  code: string;
  name: LocalizedText;
  manufacturer: string;
  model: string;
  description: string;
  props: LibraryProp[];
  version: number;
  isArchived: boolean;
  updatedAt: string;
  usedInProjects: number;
  typeCode: number | null;
  systemPrefix: string | null;
  channelCount: number | null;
  kind: ModuleKind | null;
  graphic: ModuleGraphic | null;
  /** Signal types a channel can be set to; the project picks one per channel. */
  channelModes: string[] | null;
  /** Sensors: measured outputs, numbered 1.. in list order. */
  outputs: SensorOutput[] | null;
};

export type Quantity = 'temperature' | 'humidity' | 'pressure' | 'flow' | 'other';
export const quantities: Quantity[] = ['temperature', 'humidity', 'pressure', 'flow', 'other'];

/** Sensor output: what it measures, its signal and the values at the signal range ends. */
export type SensorOutput = { no: number; quantity: Quantity; signal: string; min: number; max: number; unit: string };

export type LibraryItemSave = Omit<
  LibraryItem,
  'id' | 'version' | 'isArchived' | 'updatedAt' | 'usedInProjects' | 'kind' | 'graphic' | 'channelModes' | 'outputs'
> & {
  graphic?: ModuleGraphic | null;
  channelModes?: string[] | null;
  outputs?: SensorOutput[] | null;
  reason?: string;
};

/** One module channel in the project: signal type (goes to the module firmware) and the sensor output wired to it. */
export type ModuleChannel = { channel: number; mode: string; equipmentId?: string | null; output?: number | null };

export type PortType = 'can' | 'rs485' | 'eth' | 'pwr24' | 'ai' | 'ao' | 'di' | 'do' | 'relay' | 'usb' | 'other';
export const portTypes: PortType[] = ['can', 'rs485', 'eth', 'pwr24', 'ai', 'ao', 'di', 'do', 'relay', 'usb', 'other'];
export type PortDir = 'in' | 'out' | 'bi';
export const portDirs: PortDir[] = ['in', 'out', 'bi'];
/** Role of a connection point in automatic numbering by wires (plan 8.3.3). */
export type PortRole = 'line' | 'uplink' | 'cableOut' | 'railOut' | 'chainIn' | 'chainOut' | 'hubPort' | 'hubIn';
export const portRoles: PortRole[] = ['line', 'uplink', 'cableOut', 'railOut', 'chainIn', 'chainOut', 'hubPort', 'hubIn'];

/** Connection point; x, y are fractions 0..1 of the block size. */
export type GraphicPort = {
  id: string;
  label: string;
  type: PortType;
  dir: PortDir;
  x: number;
  y: number;
  color?: string | null;
  role?: PortRole | null;
  line?: number | null;
  index?: number | null;
};

export type GraphicLabel = { id: string; text: string; x: number; y: number; color?: string | null; size: number; bold: boolean };

export type ModuleGraphic = {
  width: number;
  height: number;
  fill: string;
  header: string;
  headerText: string;
  textColor: string;
  border: string;
  title: string;
  subtitle: string;
  showArticle: boolean;
  ports: GraphicPort[];
  labels: GraphicLabel[];
};

export type LinkStyle = 'step' | 'straight' | 'curve';
export const linkStyles: LinkStyle[] = ['step', 'straight', 'curve'];
export type SchemePoint = { x: number; y: number };
export type ProjectLink = {
  id: string;
  fromModuleId: string;
  fromPort: string;
  toModuleId: string;
  toPort: string;
  style: LinkStyle;
  /** Bend points in scheme coordinates; empty = automatic route. */
  points: SchemePoint[];
};
export type Scheme = { modules: ProjectModule[]; links: ProjectLink[] };

/** plc = 0.00.00; noid = outside the ID chain; head = locomotive / hub (place 00); display = hub port; wagon = rail slot; plugin = not placed. */
export type ModuleKind = 'plc' | 'noid' | 'head' | 'display' | 'wagon' | 'plugin';

/** Lines of the module ID L.RR.MM (plan 8.3). */
export const moduleLines = [1, 2, 3] as const;

export type ProjectModule = {
  id: string;
  libraryItemId: string | null;
  libraryVersion: number;
  libraryLatestVersion: number | null;
  snapshot: LibraryItem | null;
  kind: ModuleKind;
  line: number;
  rail: number;
  place: number;
  moduleId: string;
  systemName: string;
  userName: LocalizedText;
  expectedSerial: string;
  revision: string;
  notes: string;
  placed: boolean;
  schemeX: number;
  schemeY: number;
  channels: ModuleChannel[];
};

export type ProjectModuleSave = {
  libraryItemId: string;
  line: number;
  rail: number;
  place: number;
  userName: LocalizedText;
  expectedSerial: string;
  revision: string;
  notes: string;
  channels?: ModuleChannel[];
  reason?: string;
};

export type Equipment = {
  id: string;
  tag: string;
  quantity: number;
  roomId: string | null;
  notes: string;
  libraryItemId: string | null;
  libraryVersion: number;
  libraryLatestVersion: number | null;
  snapshot: LibraryItem | null;
};

export type EquipmentSave = {
  libraryItemId: string | null;
  tag: string;
  quantity: number;
  roomId: string | null;
  notes: string;
  reason?: string;
};

export type Settings = { siteName: string; startupLanguage: string; plcMode: string; plcAddress: string };
export type PublicSettings = { siteName: string; startupLanguage: string };

export type UserRow = { id: string; login: string; fullName: string; role: Role; isActive: boolean; lastLoginAt: string | null };

export type AuditEntry = {
  id: number;
  at: string;
  userLogin: string;
  source: string;
  action: string;
  target: string;
  oldValue: string | null;
  newValue: string | null;
  reason: string | null;
};

export type AlarmEvent = {
  id: number;
  code: string;
  element: string;
  startedAt: string;
  clearedAt: string | null;
  ackAt: string | null;
  ackBy: string | null;
};

export type FirmwareFile = {
  id: string;
  moduleType: number;
  boardRev: number;
  /** major << 8 | minor */
  version: number;
  sizeBytes: number;
  crc32: string;
  fileName: string;
  notes: string;
  uploadedAt: string;
  uploadedBy: string;
  sentToPlcAt: string | null;
};

export type FirmwareResult = { ok: boolean; error: string | null; plcError: number | null };

export type PlcConfigModule = { index: number; moduleId: string; systemName: string; id: string; type: number; channels: number };
export type PlcConfigIssue = { code: string; module: string | null; channel: number | null; blocking: boolean };
export type PlcConfigBuilt = {
  crc: string;
  size: number;
  project: string;
  modules: PlcConfigModule[];
  channelCount: number;
  issues: PlcConfigIssue[];
};
/**
 * Module of the configuration the PLC runs: st = 0 not checked, 1 ok, 2 missing, 3 error, 4 applying, 5 on the bus,
 * 6 another type at this place. fc / tag: type and board tag of the module found there (0 / "000000" = none).
 */
export type PlcCfgMod = {
  type: number;
  l: number;
  r: number;
  p: number;
  st: number;
  err: number;
  ok: number;
  bad: number;
  ver: number;
  fc?: number;
  tag?: string;
};
/** Module found by the identification walk; r = 255 means outside the chain (no place). */
export type PlcIdModule = { type: number; l: number; r: number; p: number; tag: string; board?: number };
export type PlcIdWalk = { busy: number; walks: number; count: number; stray: number; age: number };
export type PlcCfgStatus = {
  up: number;
  flash: number;
  present: number;
  gen: number;
  /** 0 none, 1 ok, 2 applying, 3 errors */
  state: number;
  size: number;
  crc: string;
  project: string;
  modules: number;
  channels: number;
  applied: number;
  resends: number;
  extraLoco: number;
  extraAi: number;
  problems?: number;
  plcVer: number;
  walk?: PlcIdWalk;
  mods: PlcCfgMod[];
  /** Found but not in the configuration, or outside the chain. */
  extra?: PlcIdModule[];
  found?: PlcIdModule[];
};
export type PlcConfigView = {
  built: PlcConfigBuilt;
  online: boolean;
  stale: boolean;
  error: string | null;
  plc: PlcCfgStatus | null;
  matches: boolean;
};
export type PlcConfigUploadResult = { ok: boolean; error: string | null; plcError: number | null; gen: number | null };

/** Roles of a PLC store slot: 1 current, 2 new, 3 backup. */
export type PlcFwSlot = { type: number; idx: number; role: number; board: number; ver: number; size: number; crc: string };

export type PlcFwNode = {
  tag: string;
  type: number;
  board: number;
  state: number;
  boot: number;
  ver: number;
  fails: number;
  link: number;
  mismatch: number;
  result: number;
  attempts: number;
  err: number;
  step: number;
  age: number;
};

export type PlcFwLog = { id: number; t: number; tag: string; type: number; ev: number; err: number; step: number; from: number; to: number };

export type PlcFwState = {
  up: number;
  boot: number;
  store: number;
  jedec: string;
  upload: number;
  run: { active: number; type: number; mode: number; ver: number; done: number; failed: number; rollback?: number };
  busy: number;
  busyTag: string;
  step: number;
  progress: number;
  stats: { ok: number; failed: number; retries: number; rx: number; drop: number };
  slots: PlcFwSlot[];
  nodes: PlcFwNode[];
  log: PlcFwLog[];
};

export type PlcFwStatus = { online: boolean; stale: boolean; uploading: boolean; error: string | null; plc: PlcFwState | null };

const json = (body: unknown) => JSON.stringify(body);
const q = (reason?: string) => (reason ? `reason=${encodeURIComponent(reason)}` : '');
const P = (pid: string) => `/api/projects/${pid}`;

export const api = {
  login: (login: string, password: string) =>
    request<AuthResponse>('/api/auth/login', { method: 'POST', body: json({ login, password }) }),

  publicSettings: () => request<PublicSettings>('/api/settings/public'),
  settings: (t: string) => request<Settings>('/api/settings', {}, t),
  saveSettings: (t: string, s: Settings) => request<Settings>('/api/settings', { method: 'PUT', body: json(s) }, t),
  users: (t: string) => request<UserRow[]>('/api/settings/users', {}, t),

  projects: (t: string, archived = false) => request<Project[]>(`/api/projects?archived=${archived}`, {}, t),
  project: (t: string, id: string) => request<Project>(P(id), {}, t),
  createProject: (t: string, p: ProjectSave) => request<Project>('/api/projects', { method: 'POST', body: json(p) }, t),
  updateProject: (t: string, id: string, p: ProjectSave) => request<Project>(P(id), { method: 'PUT', body: json(p) }, t),
  archiveProject: (t: string, id: string, value: boolean, reason?: string) =>
    request<Project>(`${P(id)}/archive?value=${value}&${q(reason)}`, { method: 'POST' }, t),
  activateProject: (t: string, id: string, reason?: string) => request<Project>(`${P(id)}/activate?${q(reason)}`, { method: 'POST' }, t),
  copyProject: (t: string, id: string) => request<Project>(`${P(id)}/copy`, { method: 'POST' }, t),

  rooms: (t: string, pid: string) => request<Room[]>(`${P(pid)}/rooms`, {}, t),
  createRoom: (t: string, pid: string, r: RoomSave) => request<Room>(`${P(pid)}/rooms`, { method: 'POST', body: json(r) }, t),
  updateRoom: (t: string, pid: string, id: string, r: RoomSave) =>
    request<Room>(`${P(pid)}/rooms/${id}`, { method: 'PUT', body: json(r) }, t),
  deleteRoom: (t: string, pid: string, id: string, reason?: string) =>
    request<void>(`${P(pid)}/rooms/${id}?${q(reason)}`, { method: 'DELETE' }, t),

  documents: (t: string, pid: string) => request<ProjectDocument[]>(`${P(pid)}/documents`, {}, t),
  uploadDocument: (t: string, pid: string, kind: DocumentKind, file: File) => {
    const fd = new FormData();
    fd.append('kind', kind);
    fd.append('file', file);
    return request<ProjectDocument>(`${P(pid)}/documents`, { method: 'POST', body: fd }, t);
  },
  downloadDocument: (t: string, pid: string, d: ProjectDocument) => downloadFile(`${P(pid)}/documents/${d.id}/file`, d.fileName, t),
  deleteDocument: (t: string, pid: string, id: string, reason?: string) =>
    request<void>(`${P(pid)}/documents/${id}?${q(reason)}`, { method: 'DELETE' }, t),

  equipment: (t: string, pid: string) => request<Equipment[]>(`${P(pid)}/equipment`, {}, t),
  createEquipment: (t: string, pid: string, e: EquipmentSave) =>
    request<Equipment>(`${P(pid)}/equipment`, { method: 'POST', body: json(e) }, t),
  updateEquipment: (t: string, pid: string, id: string, e: EquipmentSave) =>
    request<Equipment>(`${P(pid)}/equipment/${id}`, { method: 'PUT', body: json(e) }, t),
  updateEquipmentFromLibrary: (t: string, pid: string, id: string, reason?: string) =>
    request<Equipment>(`${P(pid)}/equipment/${id}/update-from-library?${q(reason)}`, { method: 'POST' }, t),
  deleteEquipment: (t: string, pid: string, id: string, reason?: string) =>
    request<void>(`${P(pid)}/equipment/${id}?${q(reason)}`, { method: 'DELETE' }, t),

  modules: (t: string, pid: string) => request<ProjectModule[]>(`${P(pid)}/modules`, {}, t),
  createModule: (t: string, pid: string, m: ProjectModuleSave) =>
    request<ProjectModule>(`${P(pid)}/modules`, { method: 'POST', body: json(m) }, t),
  updateModule: (t: string, pid: string, id: string, m: ProjectModuleSave) =>
    request<ProjectModule>(`${P(pid)}/modules/${id}`, { method: 'PUT', body: json(m) }, t),
  deleteModule: (t: string, pid: string, id: string, reason?: string) =>
    request<void>(`${P(pid)}/modules/${id}?${q(reason)}`, { method: 'DELETE' }, t),
  updateModuleFromLibrary: (t: string, pid: string, id: string, reason?: string) =>
    request<ProjectModule>(`${P(pid)}/modules/${id}/update-from-library?${q(reason)}`, { method: 'POST' }, t),

  scheme: (t: string, pid: string) => request<Scheme>(`${P(pid)}/scheme`, {}, t),
  createSchemeModule: (t: string, pid: string, libraryItemId: string, x: number, y: number) =>
    request<ProjectModule>(`${P(pid)}/scheme/modules`, { method: 'POST', body: json({ libraryItemId, x, y }) }, t),
  moveSchemeModule: (t: string, pid: string, id: string, x: number, y: number) =>
    request<void>(`${P(pid)}/scheme/modules/${id}/position`, { method: 'PUT', body: json({ x, y }) }, t),
  saveLinkRoute: (t: string, pid: string, id: string, style: LinkStyle, points: SchemePoint[]) =>
    request<ProjectLink>(`${P(pid)}/scheme/links/${id}/route`, { method: 'PUT', body: json({ style, points }) }, t),
  createLink: (t: string, pid: string, l: Pick<ProjectLink, 'fromModuleId' | 'fromPort' | 'toModuleId' | 'toPort'>) =>
    request<Scheme>(`${P(pid)}/scheme/links`, { method: 'POST', body: json(l) }, t),
  deleteLink: (t: string, pid: string, id: string, reason?: string) =>
    request<void>(`${P(pid)}/scheme/links/${id}?${q(reason)}`, { method: 'DELETE' }, t),

  library: (t: string, category?: LibraryCategory, archived = false) =>
    request<LibraryItem[]>(`/api/library?archived=${archived}${category ? `&category=${category}` : ''}`, {}, t),
  createLibraryItem: (t: string, i: LibraryItemSave) => request<LibraryItem>('/api/library', { method: 'POST', body: json(i) }, t),
  updateLibraryItem: (t: string, id: string, i: LibraryItemSave) =>
    request<LibraryItem>(`/api/library/${id}`, { method: 'PUT', body: json(i) }, t),
  archiveLibraryItem: (t: string, id: string, value: boolean, reason?: string) =>
    request<LibraryItem>(`/api/library/${id}/archive?value=${value}&${q(reason)}`, { method: 'POST' }, t),

  audit: (t: string, offset: number, limit: number, projectId?: string) =>
    request<{ items: AuditEntry[]; total: number }>(
      `/api/journal/audit?offset=${offset}&limit=${limit}${projectId ? `&projectId=${projectId}` : ''}`,
      {},
      t,
    ),
  alarms: (t: string, active: boolean) => request<AlarmEvent[]>(`/api/journal/alarms?active=${active}`, {}, t),
  ackAlarm: (t: string, id: number) => request<void>(`/api/journal/alarms/${id}/ack`, { method: 'POST' }, t),

  firmware: (t: string) => request<FirmwareFile[]>('/api/firmware', {}, t),
  uploadFirmware: (t: string, file: File, notes?: string) => {
    const fd = new FormData();
    fd.append('file', file);
    if (notes) fd.append('notes', notes);
    return request<FirmwareFile>('/api/firmware', { method: 'POST', body: fd }, t);
  },
  downloadFirmware: (t: string, f: FirmwareFile) =>
    downloadFile(`/api/firmware/${f.id}/download`, f.fileName.replace(/\.[^.]*$/, '') + '.bin', t),
  deleteFirmware: (t: string, id: string, reason?: string) => request<void>(`/api/firmware/${id}?${q(reason)}`, { method: 'DELETE' }, t),
  sendFirmware: (t: string, id: string, reason?: string) =>
    request<FirmwareResult>(`/api/firmware/${id}/send?${q(reason)}`, { method: 'POST' }, t),
  runFirmware: (t: string, type: number, mode: number, reason?: string) =>
    request<FirmwareResult>(`/api/firmware/run?type=${type}&mode=${mode}&${q(reason)}`, { method: 'POST' }, t),
  cancelFirmware: (t: string) => request<FirmwareResult>('/api/firmware/cancel', { method: 'POST' }, t),
  rollbackFirmware: (t: string, type: number, reason?: string) =>
    request<FirmwareResult>(`/api/firmware/rollback?type=${type}&${q(reason)}`, { method: 'POST' }, t),
  idWalk: (t: string) => request<FirmwareResult>('/api/firmware/id-walk', { method: 'POST' }, t),
  plcFirmware: (t: string, logAfter: number) => request<PlcFwStatus>(`/api/firmware/plc?log=${logAfter}`, {}, t),

  plcConfig: (t: string, pid: string) => request<PlcConfigView>(`${P(pid)}/plc-config`, {}, t),
  uploadPlcConfig: (t: string, pid: string, reason?: string) =>
    request<PlcConfigUploadResult>(`${P(pid)}/plc-config/upload?${q(reason)}`, { method: 'POST' }, t),
};
