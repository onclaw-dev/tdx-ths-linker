import assert from "node:assert/strict";
import test from "node:test";

import { buildActivationUrl, createNodeClient } from "../dist/index.mjs";

test("activation URL carries only protocol, nonce, and origin", () => {
  assert.equal(
    buildActivationUrl("https://example.com", "a".repeat(64)),
    `onclaw-tdx-ths://activate?v=1&nonce=${"a".repeat(64)}&origin=https%3A%2F%2Fexample.com`,
  );
});

test("node client delegates to the native boundary", async () => {
  const calls = [];
  const node = {
    initRuntime: (value) => (calls.push(["init", value]), { ok: true }),
    getRuntimeState: () => (calls.push(["state"]), { initialized: true }),
    setRuntimeCode: (target, code) => (calls.push(["code", target, code]), { ok: true }),
  };
  const client = createNodeClient(node);
  await client.initialize(true);
  await client.getState();
  await client.setCode("tdx", "600000");
  assert.deepEqual(calls, [
    ["init", { licenseAccepted: true }],
    ["state"],
    ["code", "tdx", "600000"],
  ]);
});
