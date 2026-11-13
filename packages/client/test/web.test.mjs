import assert from "node:assert/strict";
import test from "node:test";

import {
  createAgentClient,
  createWebLinker,
  inspectWebAgentEnvironment,
} from "../dist/index.mjs";

const prepared = {
  launchUrl: "onclaw-tdx-ths://activate",
  origin: "https://example.com",
  protocolVersion: 1,
};

const initialized = { ok: true, licenseState: "accepted", message: "ready", runtimeHealth: "ready" };

test("Web environment accepts Windows HTTPS and localhost only", () => {
  assert.equal(inspectWebAgentEnvironment({ origin: "https://example.com", platform: "Win32" }).status, "supported");
  assert.equal(inspectWebAgentEnvironment({ origin: "http://localhost:8080", platform: "Win32" }).status, "supported");
  assert.equal(inspectWebAgentEnvironment({ origin: "http://example.com", platform: "Win32" }).status, "insecure_origin");
  assert.equal(inspectWebAgentEnvironment({ origin: "https://example.com", platform: "MacIntel" }).status, "unsupported_platform");
});

test("Web connector opens synchronously, retries Origin grant, and initializes after consent", async () => {
  const events = [];
  let attempt = 0;
  let clock = 0;
  const client = {
    transport: "agent",
    prepareConnection: () => (events.push("prepare"), prepared),
    openAgent: () => events.push("open"),
    connect: async () => {
      events.push(`connect-${++attempt}`);
      return attempt === 1
        ? { ok: false, status: "pairing_failed", message: "pending" }
        : { ok: true, status: "connected", message: "connected", agentVersion: "0.1.2" };
    },
    initialize: async (accepted) => (events.push(`initialize-${accepted}`), initialized),
    getState: async () => ({}),
    setCode: async () => ({}),
  };
  const linker = createWebLinker({
    origin: "https://example.com",
    platform: "Win32",
    client,
    now: () => clock,
    wait: async (milliseconds) => { clock += milliseconds; },
  });

  const connecting = linker.connect({ licenseAccepted: true });
  assert.deepEqual(events, ["prepare", "open", "connect-1"]);
  const result = await connecting;
  assert.equal(result.status, "connected");
  assert.deepEqual(events, ["prepare", "open", "connect-1", "connect-2", "initialize-true"]);
});

test("Web connector requires explicit license consent before activation", async () => {
  let opened = false;
  const client = {
    transport: "agent",
    prepareConnection: () => prepared,
    openAgent: () => { opened = true; },
    initialize: async () => initialized,
    getState: async () => ({}),
    setCode: async () => ({}),
  };
  const linker = createWebLinker({ origin: "https://example.com", platform: "Win32", client });
  assert.equal((await linker.connect({ licenseAccepted: false })).status, "license_required");
  assert.equal(opened, false);
});

test("Web connector exposes installer recovery and loopback denial", async () => {
  const unavailable = {
    transport: "agent",
    prepareConnection: () => prepared,
    openAgent: () => undefined,
    connect: async () => ({ ok: false, status: "agent_unavailable", message: "offline" }),
    initialize: async () => initialized,
    getState: async () => ({}),
    setCode: async () => ({}),
  };
  let clock = 0;
  const common = {
    origin: "https://example.com",
    platform: "Win32",
    client: unavailable,
    timeoutMs: 1,
    now: () => clock,
    wait: async (milliseconds) => { clock += milliseconds; },
  };
  const recovery = createWebLinker({ ...common, installerUrl: "https://mirror.example/setup.exe" });
  const result = await recovery.connect({ licenseAccepted: true });
  assert.equal(result.status, "installer_required");
  assert.equal(result.installerUrl, "https://mirror.example/setup.exe");

  clock = 0;
  const denied = createWebLinker({ ...common, queryLoopbackPermission: async () => "denied" });
  assert.equal((await denied.connect({ licenseAccepted: true })).status, "local_network_denied");
});

test("Agent calls are sessionless and writes carry caller identity", async () => {
  const requests = [];
  const fakeFetch = async (url, init) => {
    requests.push([url, init]);
    if (url.endsWith("/v1/health")) {
      return new Response(JSON.stringify({ protocolVersion: 1, agentVersion: "test", writableTargets: ["tdx", "ths"], writeAuthorized: true }));
    }
    return new Response(JSON.stringify({ ok: true }));
  };
  const client = createAgentClient({ origin: "https://example.com", clientId: "client-test", fetch: fakeFetch });
  const connected = await client.connect(client.prepareConnection());
  assert.equal(connected.status, "connected");
  await client.getState();
  await client.setCode("tdx", "600000");
  assert.equal(requests.length, 3);
  assert.ok(requests.every(([, init]) => init.targetAddressSpace === "loopback"));
  assert.ok(requests.every(([, init]) => !new Headers(init.headers).has("Authorization")));
  const write = JSON.parse(requests[2][1].body);
  assert.equal(write.clientId, "client-test");
  assert.match(write.requestId, /^request-[0-9a-f]{64}$/);
});

test("Agent client reports protocol mismatch before initialization", async () => {
  const fakeFetch = async () => new Response(JSON.stringify({ protocolVersion: 2, agentVersion: "other", writableTargets: [], writeAuthorized: false }));
  const client = createAgentClient({ origin: "https://example.com", fetch: fakeFetch });
  assert.equal((await client.connect(client.prepareConnection())).status, "protocol_mismatch");
});
