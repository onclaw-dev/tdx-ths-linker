export type LinkerTarget = "tdx" | "ths";
export type LinkerHealth =
  | "idle"
  | "initializing"
  | "ready"
  | "degraded"
  | "unavailable";
export type LinkerLicenseState = "unaccepted" | "accepted";
export type LinkerActivityState = "hot" | "warm" | "cool" | "cold";
export type LinkerCommandState =
  | "queued" | "dispatching" | "dispatched" | "confirmed"
  | "failed" | "timed_out" | "superseded";

export interface LinkerCommandStatus {
  target: LinkerTarget;
  code: string;
  clientId: string;
  requestId: string;
  state: LinkerCommandState;
  errorStage: string;
  message: string;
  queuedAt: number;
  dispatchedAt: number;
  completedAt: number;
  win32Error: number;
}

export interface LinkerEndpointState {
  name: LinkerTarget;
  code: string;
  health: LinkerHealth;
  lastError: string;
  updatedAt: number;
  lastScanAt: number;
  lastResolvedOffset: number;
  available: boolean;
  usingFallback: boolean;
  directRead: boolean;
  pid: number;
}

export interface LinkerRuntimeState {
  initialized: boolean;
  isSupported: boolean;
  active: boolean;
  lastActivityAt: number;
  activityState: LinkerActivityState;
  stateGeneration: number;
  stale: boolean;
  refreshedAt: number;
  runtimeHealth: LinkerHealth;
  licenseState: LinkerLicenseState;
  message: string;
  ths: LinkerEndpointState;
  tdx: LinkerEndpointState;
  lastCommand: LinkerCommandStatus | null;
}

export interface LinkerInitResult {
  ok: boolean;
  licenseState: LinkerLicenseState;
  message: string;
  runtimeHealth: LinkerHealth;
}

export interface LinkerCommandResult {
  ok: boolean;
  accepted: boolean;
  status: string;
  target: LinkerTarget;
  code: string;
  clientId: string;
  requestId: string;
  message: string;
}

export interface LinkerPreparedConnection {
  launchUrl: string;
  origin: string;
  protocolVersion: 1;
}

export interface LinkerConnectionResult {
  ok: boolean;
  status: "connected" | "agent_unavailable" | "protocol_mismatch" | "pairing_failed";
  message: string;
  agentVersion?: string;
  writableTargets?: LinkerTarget[];
}

export interface OnclawClient {
  readonly transport: "agent";
  prepareConnection?: () => LinkerPreparedConnection;
  openAgent?: (prepared: LinkerPreparedConnection) => void;
  connect?: (prepared: LinkerPreparedConnection) => Promise<LinkerConnectionResult>;
  initialize: (licenseAccepted: boolean) => Promise<LinkerInitResult>;
  getState: () => Promise<LinkerRuntimeState>;
  setCode: (target: LinkerTarget, code: string) => Promise<LinkerCommandResult>;
  getCommandStatus?: (clientId: string, requestId: string) => Promise<LinkerCommandStatus>;
  setCodeAndWait?: (
    target: LinkerTarget,
    code: string,
    timeoutMs?: number,
  ) => Promise<LinkerCommandStatus>;
}
