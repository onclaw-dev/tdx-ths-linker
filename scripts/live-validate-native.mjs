import { spawnSync } from "node:child_process";
import { resolve } from "node:path";

const code = process.env.ONCLAW_LIVE_CODE ?? "";
if (process.env.ONCLAW_LIVE_TEST !== "1") {
  console.log(JSON.stringify({ channel: "npm-native", status: "skipped", reason: "ONCLAW_LIVE_TEST is not 1" }));
  process.exit(0);
}
if (process.env.TDX_THS_LINKER_LICENSE_ACCEPTED !== "1") {
  throw new Error("TDX_THS_LINKER_LICENSE_ACCEPTED=1 is required for live writes");
}
if (!/^\d{6}$/.test(code)) throw new Error("ONCLAW_LIVE_CODE must be six ASCII digits");

const cli = resolve("packages/client/dist/cli-bin.mjs");
const invoke = (...args) => {
  const result = spawnSync(process.execPath, [cli, "--json", "--accept-license", ...args], {
    cwd: resolve("."),
    encoding: "utf8",
    env: process.env,
  });
  const line = result.stdout.trim().split(/\r?\n/).at(-1) ?? "";
  let payload;
  try { payload = JSON.parse(line); } catch { payload = { ok: false, error: { code: "invalid_json", message: line || result.stderr.trim() } }; }
  if (result.status !== 0 || !payload.ok) throw new Error(`${args[0]} failed: ${JSON.stringify(payload.error ?? payload)}`);
  return payload.data;
};

const before = invoke("state");
if (!before.tdx.available || !before.ths.available) throw new Error("Both TDX and THS must be available before live writes");
const tdx = invoke("set", code, "--target", "tdx", "--wait", "--timeout", "10");
const ths = invoke("set", code, "--target", "ths", "--wait", "--timeout", "10");
const after = invoke("state");
if (after.tdx.code !== code || after.ths.code !== code) throw new Error("Final TDX/THS codes do not match the requested code");

console.log(JSON.stringify({
  channel: "npm-native",
  status: "passed",
  reads: { before: true, after: true },
  writes: { tdx: { accepted: true, confirmed: tdx.confirmed }, ths: { accepted: true, confirmed: ths.confirmed } },
  finalCodesMatch: true,
}));
