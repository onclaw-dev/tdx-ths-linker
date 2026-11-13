import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { basename, resolve } from "node:path";

const packageMetadata = JSON.parse(readFileSync(resolve("packages/client/package.json"), "utf8"));
const input = resolve(process.argv[2] ?? "dist/npm");
const archive = input.endsWith(".tgz")
  ? input
  : resolve(input, `tdx-ths-linker-${packageMetadata.version}.tgz`);
assert.ok(existsSync(archive), `current npm archive does not exist: ${archive}`);
const listed = spawnSync("tar", ["-tf", archive], { encoding: "utf8" });
assert.equal(listed.status, 0, listed.stderr);
const files = new Set(listed.stdout.trim().split(/\r?\n/));
for (const required of [
  "package/package.json",
  "package/dist/index.mjs",
  "package/dist/runtime.mjs",
  "package/dist/cli.mjs",
  "package/dist/cli-bin.mjs",
  "package/agent/win-x64/onclaw-tdx-ths-agent.exe",
  "package/agent/win-x64/SHA256SUMS.txt",
]) assert.ok(files.has(required), `npm archive is missing ${required}`);
const packagedReadme = spawnSync("tar", ["-xOf", archive, "package/README.md"], { encoding: "utf8" });
assert.equal(packagedReadme.status, 0, packagedReadme.stderr);
const rootReadme = readFileSync(resolve("README.md"), "utf8");
assert.equal(
  packagedReadme.stdout.replace(/\r\n/g, "\n"),
  rootReadme.replace(/\r\n/g, "\n"),
  "npm package README must be identical to the root README.md",
);
for (const obsolete of ["package/dist/native.mjs", "package/dist/installer.mjs", "package/native/win-x64/tdx-ths-linker.node", "package/agent/win-x64/onclaw-tdx-ths-setup-win-x64.exe", "package/scripts/postinstall.cjs"])
  assert.ok(!files.has(obsolete), `npm archive contains obsolete runtime ${obsolete}`);
const agentEntry = "package/agent/win-x64/onclaw-tdx-ths-agent.exe";
const manifestEntry = "package/agent/win-x64/SHA256SUMS.txt";
const agent = spawnSync("tar", ["-xOf", archive, agentEntry]);
const manifest = spawnSync("tar", ["-xOf", archive, manifestEntry], { encoding: "utf8" });
assert.equal(agent.status, 0, agent.stderr?.toString());
assert.equal(manifest.status, 0, manifest.stderr);
const expected = manifest.stdout.match(/^([a-f0-9]{64})\s+onclaw-tdx-ths-agent\.exe$/im)?.[1];
assert.ok(expected, "npm archive Agent checksum is missing");
assert.equal(createHash("sha256").update(agent.stdout).digest("hex"), expected.toLowerCase());
console.log(JSON.stringify({ status: "passed", archive: basename(archive), bareAgentIncluded: true, rootReadmeReused: true, cliIncluded: true }));
