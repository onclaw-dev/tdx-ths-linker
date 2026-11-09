import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
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
  "package/dist/native.mjs",
  "package/dist/cli.mjs",
  "package/dist/cli-bin.mjs",
  "package/native/win-x64/tdx-ths-linker.node",
]) assert.ok(files.has(required), `npm archive is missing ${required}`);
console.log(JSON.stringify({ status: "passed", archive: basename(archive), nativeIncluded: true, cliIncluded: true }));
