export type LinkerTarget = "tdx" | "ths";
export type LinkerHealth =
  | "idle"
  | "initializing"
  | "ready"
  | "degraded"
  | "unavailable";
export type LinkerLicenseState = "unaccepted" | "accepted";

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
  runtimeHealth: LinkerHealth;
  licenseState: LinkerLicenseState;
  message: string;
  ths: LinkerEndpointState;
  tdx: LinkerEndpointState;
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
  message: string;
}

export interface LinkerPreparedConnection {
  nonce: string;
  launchUrl: string;
  origin: string;
  protocolVersion: 1;
}

export interface LinkerConnectionResult {
  ok: boolean;
  status: "connected" | "agent_unavailable" | "protocol_mismatch" | "pairing_failed";
  message: string;
  channel?: "stable" | "beta";
  agentVersion?: string;
  writableTargets?: LinkerTarget[];
}

export interface TdxThsLinkerClient {
  readonly transport: "agent" | "addon";
  prepareConnection?: () => LinkerPreparedConnection;
  openAgent?: (prepared: LinkerPreparedConnection) => void;
  connect?: (prepared: LinkerPreparedConnection) => Promise<LinkerConnectionResult>;
  initialize: (licenseAccepted: boolean) => Promise<LinkerInitResult>;
  getState: () => Promise<LinkerRuntimeState>;
  setCode: (target: LinkerTarget, code: string) => Promise<LinkerCommandResult>;
}

export interface NativeAddon {
  initRuntime(options: { licenseAccepted: boolean }): LinkerInitResult;
  getRuntimeState(): LinkerRuntimeState;
  setRuntimeCode(target: LinkerTarget, code: string): LinkerCommandResult;
}
