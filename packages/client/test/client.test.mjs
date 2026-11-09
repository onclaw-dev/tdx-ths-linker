import assert from "node:assert/strict";
import test from "node:test";

import { buildActivationUrl, createAddonClient } from "../dist/index.mjs";

test("activation URL carries only protocol, nonce, and origin", () => {
  assert.equal(
    buildActivationUrl("https://example.com", "a".repeat(64)),
    `tdx-ths-linker://activate?v=1&nonce=${"a".repeat(64)}&origin=https%3A%2F%2Fexample.com`,
  );
});

test("addon client delegates to the native boundary", async () => {
  const calls = [];
  const addon = {
    initRuntime: (value) => (calls.push(["init", value]), { ok: true }),
    getRuntimeState: () => (calls.push(["state"]), { initialized: true }),
    setRuntimeCode: (target, code) => (calls.push(["code", target, code]), { ok: true }),
  };
  const client = createAddonClient(addon);
  await client.initialize(true);
  await client.getState();
  await client.setCode("tdx", "600000");
  assert.deepEqual(calls, [
    ["init", { licenseAccepted: true }],
    ["state"],
    ["code", "tdx", "600000"],
  ]);
});
