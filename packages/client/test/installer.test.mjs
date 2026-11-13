import assert from "node:assert/strict";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { ensureAgent } from "../dist/runtime.mjs";

test("Agent runtime reuses a compatible running Agent without launching", async () => {
  let calls = 0;
  const result = await ensureAgent({
    platform: "win32",
    arch: "x64",
    localAppData: "C:\\Temp",
    fetch: async () => {
      calls += 1;
      return {
        ok: true,
        json: async () => ({
          ok: true,
          protocolVersion: 1,
          agentVersion: "test",
          writeAuthorized: true,
          writableTargets: ["tdx", "ths"],
        }),
      };
    },
  });
  assert.deepEqual(result, { status: "running", source: "existing", agentVersion: "test" });
  assert.equal(calls, 1);
});

test("Agent runtime rejects unsupported hosts before touching packaged assets", async () => {
  await assert.rejects(
    ensureAgent({ platform: "linux", arch: "x64" }),
    /supports Windows x64/,
  );
});

test("Agent runtime prefers the fixed Web installation without running an installer", async () => {
  const directory = await mkdtemp(join(tmpdir(), "tdx-ths-runtime-"));
  const installedAgentPath = join(directory, "OnclawTdxThs", "onclaw-tdx-ths-agent.exe");
  await writeFile(installedAgentPath, "installed", { flag: "wx" }).catch(async () => {
    const { mkdir } = await import("node:fs/promises");
    await mkdir(join(directory, "OnclawTdxThs"), { recursive: true });
    await writeFile(installedAgentPath, "installed");
  });
  let healthy = false;
  const launched = [];
  try {
    const result = await ensureAgent({
      platform: "win32",
      arch: "x64",
      localAppData: directory,
      installedAgentPath,
      launch: (path) => { launched.push(path); healthy = true; },
      pollIntervalMs: 1,
      fetch: async () => healthy
        ? { ok: true, json: async () => ({ ok: true, protocolVersion: 1, agentVersion: "installed" }) }
        : { ok: false },
    });
    assert.deepEqual(result, { status: "started", source: "installed", agentVersion: "installed" });
    assert.deepEqual(launched, [installedAgentPath]);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
