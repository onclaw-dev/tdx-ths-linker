import assert from "node:assert/strict";
import test from "node:test";

import { CLI_EXIT, runCli } from "../dist/cli.mjs";

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

const state = (tdx = "000001", ths = "600000") => ({
  initialized: true,
  isSupported: true,
  runtimeHealth: "ready",
  licenseState: "accepted",
  message: "ready",
  tdx: endpoint("tdx", tdx),
  ths: endpoint("ths", ths),
});

const run = async (argv, states = [state()]) => {
  const output = [];
  const writes = [];
  let index = 0;
  let clock = 0;
  const client = {
    transport: "agent",
    initialize: async (accepted) => ({ ok: accepted, licenseState: accepted ? "accepted" : "unaccepted", message: accepted ? "ready" : "license", runtimeHealth: accepted ? "ready" : "unavailable" }),
    getState: async () => states[Math.min(index++, states.length - 1)],
    setCode: async (target, code) => {
      writes.push([target, code]);
      return { ok: true, accepted: true, status: "accepted", target, code, message: "accepted" };
    },
  };
  const exitCode = await runCli(argv, {
    clientFactory: () => client,
    stdout: (value) => output.push(value),
    stderr: (value) => output.push(value),
    now: () => clock,
    sleep: async (milliseconds) => { clock += milliseconds; },
    version: "test",
    licenseEnvironment: "",
  });
  return { exitCode, envelope: output.length ? JSON.parse(output.at(-1)) : undefined, writes };
};

test("CLI refuses Agent commands before explicit license acceptance", async () => {
  const result = await run(["--json", "state"]);
  assert.equal(result.exitCode, CLI_EXIT.usage);
  assert.equal(result.envelope.error.code, "license_not_accepted");
  assert.deepEqual(result.writes, []);
});

test("CLI state emits one schema-versioned JSON envelope", async () => {
  const result = await run(["--json", "--accept-license", "state"]);
  assert.equal(result.exitCode, CLI_EXIT.ok);
  assert.equal(result.envelope.schemaVersion, 1);
  assert.equal(result.envelope.command, "state");
  assert.equal(result.envelope.data.tdx.code, "000001");
  assert.equal(result.envelope.data.ths.code, "600000");
});

test("CLI confirms a target-specific write", async () => {
  const result = await run(
    ["--json", "--accept-license", "set", "600825", "--target", "tdx", "--wait"],
    [state(), state("000001"), state("600825")],
  );
  assert.equal(result.exitCode, CLI_EXIT.ok);
  assert.equal(result.envelope.data.confirmed, true);
  assert.deepEqual(result.writes, [["tdx", "600825"]]);
});

test("CLI returns timeout details when not every target confirms", async () => {
  const result = await run(
    ["--json", "--accept-license", "set", "600825", "--target", "both", "--wait", "--timeout", "0.01", "--poll-interval", "0.01"],
    [state(), state("600825", "600000")],
  );
  assert.equal(result.exitCode, CLI_EXIT.timeout);
  assert.equal(result.envelope.error.code, "confirmation_timeout");
  assert.deepEqual(result.envelope.error.details.confirmedTargets, ["tdx"]);
});
