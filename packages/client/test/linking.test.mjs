import assert from "node:assert/strict";
import test from "node:test";

import { createStockLinkRuntime } from "../dist/index.mjs";

const endpoint = (name, code, available = true) => ({
  name,
  code,
  health: available ? "ready" : "unavailable",
  lastError: "",
  updatedAt: 0,
  lastScanAt: 0,
  lastResolvedOffset: 0,
  available,
  usingFallback: false,
  directRead: true,
  pid: available ? 1 : 0,
});

const state = (tdxCode, thsCode, available = {}) => ({
  initialized: true,
  isSupported: true,
  runtimeHealth: "ready",
  licenseState: "accepted",
  message: "ready",
  tdx: endpoint("tdx", tdxCode, available.tdx ?? true),
  ths: endpoint("ths", thsCode, available.ths ?? true),
});

const fakeClient = (states) => {
  const writes = [];
  let index = 0;
  return {
    writes,
    client: {
      transport: "agent",
      initialize: async () => ({}),
      getState: async () => states[Math.min(index++, states.length - 1)],
      setCode: async (target, code) => {
        writes.push([target, code]);
        return { ok: true, accepted: true, status: "accepted", target, code, message: "accepted" };
      },
    },
  };
};

test("read-only mode publishes state without writes", async () => {
  const observed = [];
  const fake = fakeClient([state("000001", "600000")]);
  const runtime = createStockLinkRuntime({ client: fake.client, mode: "read-only", onState: (value) => observed.push(value) });
  await runtime.pollNow();
  assert.equal(observed.length, 1);
  assert.deepEqual(fake.writes, []);
});

test("THS to TDX establishes a baseline then forwards only later changes", async () => {
  const fake = fakeClient([
    state("000001", "600000"),
    state("000001", "600519"),
    state("600519", "600519"),
  ]);
  const runtime = createStockLinkRuntime({ client: fake.client, mode: "ths-to-tdx" });
  await runtime.pollNow();
  assert.deepEqual(fake.writes, []);
  await runtime.pollNow();
  assert.deepEqual(fake.writes, [["tdx", "600519"]]);
  await runtime.pollNow();
  assert.equal(fake.writes.length, 1);
});

test("source recovery and direction changes establish new baselines", async () => {
  const fake = fakeClient([
    state("000001", "600000"),
    state("000001", "", { ths: false }),
    state("000001", "600519"),
    state("000001", "600588"),
    state("000001", "600588"),
    state("300750", "600588"),
  ]);
  const runtime = createStockLinkRuntime({ client: fake.client, mode: "ths-to-tdx" });
  await runtime.pollNow();
  await runtime.pollNow();
  await runtime.pollNow();
  assert.deepEqual(fake.writes, []);
  await runtime.pollNow();
  assert.deepEqual(fake.writes, [["tdx", "600588"]]);
  runtime.setMode("tdx-to-ths");
  await runtime.pollNow();
  await runtime.pollNow();
  assert.deepEqual(fake.writes, [["tdx", "600588"], ["ths", "300750"]]);
});

test("concurrent immediate polls share one in-flight state request", async () => {
  let resolveState;
  let calls = 0;
  const client = {
    transport: "agent",
    initialize: async () => ({}),
    getState: () => {
      calls += 1;
      return new Promise((resolve) => { resolveState = resolve; });
    },
    setCode: async () => ({}),
  };
  const runtime = createStockLinkRuntime({ client, mode: "read-only" });
  const first = runtime.pollNow();
  const second = runtime.pollNow();
  assert.equal(first, second);
  assert.equal(calls, 1);
  resolveState(state("000001", "600000"));
  await first;
});
