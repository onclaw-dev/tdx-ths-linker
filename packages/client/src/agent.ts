import type {
  LinkerConnectionResult,
  LinkerPreparedConnection,
  LinkerTarget,
  OnclawClient,
} from "./contracts";

const PROTOCOL_VERSION = 1 as const;

export interface AgentClientOptions {
  origin?: string;
  baseUrl?: string;
  fetch?: typeof globalThis.fetch;
  openUrl?: (url: string) => void;
  clientId?: string;
}

type LoopbackRequestInit = RequestInit & {
  targetAddressSpace: "loopback";
};

function identifier(): string {
  const bytes = new Uint8Array(32);
  globalThis.crypto.getRandomValues(bytes);
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export function buildActivationUrl(origin: string): string {
  return `onclaw-tdx-ths://activate?v=${PROTOCOL_VERSION}&origin=${encodeURIComponent(origin)}`;
}

export function createAgentClient(options: AgentClientOptions = {}): OnclawClient {
  const origin = options.origin ?? globalThis.location?.origin;
  const baseUrl = options.baseUrl ?? "http://127.0.0.1:17654";
  const request = options.fetch ?? globalThis.fetch;
  if (!request) throw new Error("Agent client requires fetch");
  const clientId = options.clientId ?? `${origin ? "web" : "local"}-${identifier()}`;

  const call = async <T>(path: string, init: RequestInit = {}): Promise<T> => {
    const headers = new Headers(init.headers);
    headers.set("Content-Type", "application/json");
    const requestInit: LoopbackRequestInit = {
      ...init,
      headers,
      targetAddressSpace: "loopback",
    };
    const response = await request(`${baseUrl}${path}`, requestInit);
    const body = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(String(body.error ?? `agent_http_${response.status}`));
    return body as T;
  };

  const prepareConnection = (): LinkerPreparedConnection => {
    if (!origin) throw new Error("Browser activation requires an explicit origin");
    return {
      origin,
      protocolVersion: PROTOCOL_VERSION,
      launchUrl: buildActivationUrl(origin),
    };
  };

  return {
    transport: "agent",
    ...(origin ? {
      prepareConnection,
      openAgent: (prepared: LinkerPreparedConnection) => {
        if (options.openUrl) options.openUrl(prepared.launchUrl);
        else globalThis.location.assign(prepared.launchUrl);
      },
      connect: async (_prepared: LinkerPreparedConnection): Promise<LinkerConnectionResult> => {
      let agentReached = false;
      try {
        const health = await call<{
          protocolVersion: number;
          agentVersion: string;
          writableTargets: LinkerTarget[];
          writeAuthorized: boolean;
        }>("/v1/health", { method: "GET" });
        agentReached = true;
        if (health.protocolVersion !== PROTOCOL_VERSION) {
          return { ok: false, status: "protocol_mismatch", message: "Agent protocol version mismatch" };
        }
        if (!health.writeAuthorized) {
          return { ok: false, status: "pairing_failed", message: "Agent Origin grant is pending" };
        }
        return {
          ok: true,
          status: "connected",
          message: "Local agent connected",
          agentVersion: health.agentVersion,
          writableTargets: health.writableTargets,
        };
      } catch (error) {
        return {
          ok: false,
          status: agentReached ? "pairing_failed" : "agent_unavailable",
          message: error instanceof Error ? error.message : String(error),
        };
      }
      },
    } : {}),
    initialize: (licenseAccepted) =>
      call("/v1/initialize", {
        method: "POST",
        body: JSON.stringify({ licenseAccepted }),
      }),
    getState: () => call("/v1/state", { method: "GET" }),
    setCode: (target, code) =>
      call("/v1/set-code", {
        method: "POST",
        body: JSON.stringify({ target, code, clientId, requestId: `request-${identifier()}` }),
      }),
  };
}
