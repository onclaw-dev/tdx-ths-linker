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
}

type LoopbackRequestInit = RequestInit & {
  targetAddressSpace: "loopback";
};

function nonce(): string {
  const bytes = new Uint8Array(32);
  globalThis.crypto.getRandomValues(bytes);
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export function buildActivationUrl(origin: string, value: string): string {
  return `onclaw-tdx-ths://activate?v=${PROTOCOL_VERSION}&nonce=${encodeURIComponent(value)}&origin=${encodeURIComponent(origin)}`;
}

export function createAgentClient(options: AgentClientOptions = {}): OnclawClient {
  const origin = options.origin ?? globalThis.location?.origin;
  if (!origin) throw new Error("Agent client requires an explicit browser origin");
  const baseUrl = options.baseUrl ?? "http://127.0.0.1:17654";
  const request = options.fetch ?? globalThis.fetch;
  if (!request) throw new Error("Agent client requires fetch");
  let token = "";

  const call = async <T>(path: string, init: RequestInit = {}): Promise<T> => {
    const headers = new Headers(init.headers);
    headers.set("Content-Type", "application/json");
    if (token) headers.set("Authorization", `Bearer ${token}`);
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
    const value = nonce();
    return {
      nonce: value,
      origin,
      protocolVersion: PROTOCOL_VERSION,
      launchUrl: buildActivationUrl(origin, value),
    };
  };

  return {
    transport: "agent",
    prepareConnection,
    openAgent: (prepared) => {
      if (options.openUrl) options.openUrl(prepared.launchUrl);
      else globalThis.location.assign(prepared.launchUrl);
    },
    connect: async (prepared): Promise<LinkerConnectionResult> => {
      let agentReached = false;
      try {
        const health = await call<{
          protocolVersion: number;
          agentVersion: string;
          writableTargets: LinkerTarget[];
        }>("/v1/health", { method: "GET" });
        agentReached = true;
        if (health.protocolVersion !== PROTOCOL_VERSION) {
          return { ok: false, status: "protocol_mismatch", message: "Agent protocol version mismatch" };
        }
        const claimed = await call<{
          sessionToken: string;
          writableTargets: LinkerTarget[];
        }>("/v1/session/claim", {
          method: "POST",
          body: JSON.stringify({ nonce: prepared.nonce }),
        });
        token = claimed.sessionToken;
        return {
          ok: true,
          status: "connected",
          message: "Local agent connected",
          agentVersion: health.agentVersion,
          writableTargets: claimed.writableTargets,
        };
      } catch (error) {
        return {
          ok: false,
          status: agentReached ? "pairing_failed" : "agent_unavailable",
          message: error instanceof Error ? error.message : String(error),
        };
      }
    },
    initialize: (licenseAccepted) =>
      call("/v1/initialize", {
        method: "POST",
        body: JSON.stringify({ licenseAccepted }),
      }),
    getState: () => call("/v1/state", { method: "GET" }),
    setCode: (target, code) =>
      call("/v1/set-code", {
        method: "POST",
        body: JSON.stringify({ target, code }),
      }),
  };
}
