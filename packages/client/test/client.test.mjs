import assert from "node:assert/strict";
import test from "node:test";

import { buildActivationUrl, createAgentClient } from "../dist/index.mjs";

test("activation URL carries only protocol v1 and origin", () => {
  assert.equal(
    buildActivationUrl("https://example.com"),
    "onclaw-tdx-ths://activate?v=1&origin=https%3A%2F%2Fexample.com",
  );
});

test("local Node client uses the Origin-less Agent HTTP boundary", async () => {
  const calls = [];
  const request = async (url, init) => {
    calls.push([url, init?.method ?? "GET", init?.body]);
    return { ok: true, json: async () => ({ ok: true }) };
  };
  const client = createAgentClient({ fetch: request, clientId: "node-test" });
  assert.equal(client.transport, "agent");
  assert.equal(client.prepareConnection, undefined);
  await client.initialize(true);
  await client.getState();
  await client.setCode("tdx", "600000");
  assert.equal(calls[0][0], "http://127.0.0.1:17654/v1/initialize");
  assert.equal(calls[1][0], "http://127.0.0.1:17654/v1/state");
  assert.equal(calls[2][0], "http://127.0.0.1:17654/v1/set-code");
  assert.match(calls[2][2], /"clientId":"node-test"/);
});
