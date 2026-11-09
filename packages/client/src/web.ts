import { createAgentClient, type AgentClientOptions } from "./agent";
import type {
  LinkerConnectionResult,
  LinkerPreparedConnection,
  OnclawClient,
} from "./contracts";

export const DEFAULT_AGENT_INSTALLER_URL =
  "https://github.com/onclaw-dev/tdx-ths-linker/releases/latest/download/onclaw-tdx-ths-setup-win-x64.exe";

export type WebEnvironmentStatus = "supported" | "unsupported_platform" | "insecure_origin";

export interface WebAgentEnvironment {
  ok: boolean;
  status: WebEnvironmentStatus;
  message: string;
  origin: string;
}

export type WebLinkerConnectionStatus =
  | "connected"
  | "unsupported_platform"
  | "insecure_origin"
  | "license_required"
  | "local_network_denied"
  | "installer_required"
  | "protocol_mismatch"
  | "pairing_failed"
  | "initialization_failed";

export interface WebLinkerConnectionResult {
  ok: boolean;
  status: WebLinkerConnectionStatus;
  message: string;
  installerUrl?: string;
  agentVersion?: string;
}

export interface WebLinkerConnectOptions {
  licenseAccepted: boolean;
}

export interface WebLinkerOptions extends AgentClientOptions {
  installerUrl?: string;
  timeoutMs?: number;
  pollIntervalMs?: number;
  userAgent?: string;
  platform?: string;
  client?: OnclawClient;
  now?: () => number;
  wait?: (milliseconds: number) => Promise<void>;
  queryLoopbackPermission?: () => Promise<PermissionState | undefined>;
}

export interface WebLinker {
  readonly client: OnclawClient;
  readonly environment: WebAgentEnvironment;
  readonly installerUrl: string;
  connect: (options: WebLinkerConnectOptions) => Promise<WebLinkerConnectionResult>;
}

const isLocalHostname = (hostname: string): boolean =>
  hostname === "localhost" || hostname === "127.0.0.1" || hostname === "[::1]";

export function inspectWebAgentEnvironment(options: {
  origin?: string;
  userAgent?: string;
  platform?: string;
} = {}): WebAgentEnvironment {
  const origin = options.origin ?? globalThis.location?.origin ?? "";
  const navigatorValue = globalThis.navigator;
  const platform = options.platform ?? navigatorValue?.platform ?? "";
  const userAgent = options.userAgent ?? navigatorValue?.userAgent ?? "";
  const isWindows = /win/i.test(`${platform} ${userAgent}`);

  if (!isWindows) {
    return {
      ok: false,
      status: "unsupported_platform",
      message: "Web Agent is available only on Windows",
      origin,
    };
  }

  try {
    const url = new URL(origin);
    const secure = url.protocol === "https:" || (url.protocol === "http:" && isLocalHostname(url.hostname));
    if (!secure) {
      return {
        ok: false,
        status: "insecure_origin",
        message: "Web Agent requires HTTPS or a localhost HTTP origin",
        origin,
      };
    }
  } catch {
    return {
      ok: false,
      status: "insecure_origin",
      message: "Web Agent requires a valid HTTPS or localhost origin",
      origin,
    };
  }

  return { ok: true, status: "supported", message: "Web Agent environment supported", origin };
}

const defaultWait = (milliseconds: number): Promise<void> =>
  new Promise((resolve) => globalThis.setTimeout(resolve, milliseconds));

async function defaultQueryLoopbackPermission(): Promise<PermissionState | undefined> {
  const permissions = globalThis.navigator?.permissions;
  if (!permissions?.query) return undefined;
  try {
    const result = await permissions.query({ name: "loopback-network" as PermissionName });
    return result.state;
  } catch {
    return undefined;
  }
}

async function finishConnection(
  client: OnclawClient,
  prepared: LinkerPreparedConnection,
  settings: {
    installerUrl: string;
    timeoutMs: number;
    pollIntervalMs: number;
    now: () => number;
    wait: (milliseconds: number) => Promise<void>;
    queryLoopbackPermission: () => Promise<PermissionState | undefined>;
  },
): Promise<WebLinkerConnectionResult> {
  if (!client.connect) {
    return { ok: false, status: "pairing_failed", message: "Agent client cannot establish a session" };
  }

  const deadline = settings.now() + settings.timeoutMs;
  let last: LinkerConnectionResult | undefined;
  let agentReached = false;
  do {
    last = await client.connect(prepared);
    if (last.ok) {
      const initialized = await client.initialize(true);
      if (!initialized.ok) {
        return { ok: false, status: "initialization_failed", message: initialized.message };
      }
      return {
        ok: true,
        status: "connected",
        message: initialized.message || last.message,
        agentVersion: last.agentVersion,
      };
    }
    if (last.status === "protocol_mismatch") {
      return { ok: false, status: "protocol_mismatch", message: last.message, installerUrl: settings.installerUrl };
    }
    agentReached ||= last.status === "pairing_failed";
    if (settings.now() >= deadline) break;
    await settings.wait(Math.min(settings.pollIntervalMs, Math.max(0, deadline - settings.now())));
  } while (settings.now() <= deadline);

  if (!agentReached) {
    const permission = await settings.queryLoopbackPermission();
    if (permission === "denied") {
      return {
        ok: false,
        status: "local_network_denied",
        message: "Browser access to the local Agent is denied",
      };
    }
    return {
      ok: false,
      status: "installer_required",
      message: "Local Agent could not be reached; install it or retry after starting it",
      installerUrl: settings.installerUrl,
    };
  }

  return {
    ok: false,
    status: "pairing_failed",
    message: last?.message ?? "Agent activation could not be claimed",
  };
}

export function createWebLinker(options: WebLinkerOptions = {}): WebLinker {
  const environment = inspectWebAgentEnvironment(options);
  const installerUrl = options.installerUrl ?? DEFAULT_AGENT_INSTALLER_URL;
  const client = options.client ?? createAgentClient({
    origin: options.origin,
    baseUrl: options.baseUrl,
    fetch: options.fetch,
    openUrl: options.openUrl,
  });

  const connect = (connectOptions: WebLinkerConnectOptions): Promise<WebLinkerConnectionResult> => {
    if (!environment.ok) {
      return Promise.resolve({ ok: false, status: environment.status, message: environment.message });
    }
    if (!connectOptions.licenseAccepted) {
      return Promise.resolve({
        ok: false,
        status: "license_required",
        message: "GPLv3 license acceptance is required before initialization",
      });
    }
    if (!client.prepareConnection || !client.openAgent) {
      return Promise.resolve({ ok: false, status: "pairing_failed", message: "Agent activation is unavailable" });
    }

    const prepared = client.prepareConnection();
    client.openAgent(prepared);
    return finishConnection(client, prepared, {
      installerUrl,
      timeoutMs: Math.max(0, options.timeoutMs ?? 10_000),
      pollIntervalMs: Math.max(1, options.pollIntervalMs ?? 250),
      now: options.now ?? Date.now,
      wait: options.wait ?? defaultWait,
      queryLoopbackPermission: options.queryLoopbackPermission ?? defaultQueryLoopbackPermission,
    });
  };

  return { client, environment, installerUrl, connect };
}
