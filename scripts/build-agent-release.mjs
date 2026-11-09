import { createHash } from "node:crypto";
import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const releaseDir = join(root, "release", "agent");
const unsigned = process.env.ONCLAW_TDX_THS_RELEASE_UNSIGNED === "1";
const executable = "onclaw-tdx-ths-agent.exe";
const installer = "onclaw-tdx-ths-setup.exe";

const run = (command, args) => {
  const result = spawnSync(command, args, { cwd: root, stdio: "inherit", shell: false });
  if (result.status !== 0) throw new Error(`${command} failed with ${result.status}`);
};

run("cmake", ["-S", ".", "-B", "build"]);
run("cmake", ["--build", "build", "--config", "Release", "--target", "onclaw-tdx-ths-agent"]);
await mkdir(releaseDir, { recursive: true });
await copyFile(join(root, "build", "Release", executable), join(releaseDir, executable));

const signTool = process.env.ONCLAW_TDX_THS_SIGNTOOL;
const certificate = process.env.ONCLAW_TDX_THS_SIGN_CERTIFICATE;
const password = process.env.ONCLAW_TDX_THS_SIGN_PASSWORD;
if (!unsigned && (!signTool || !certificate || !password)) {
  throw new Error(
    "A signed release requires ONCLAW_TDX_THS_SIGNTOOL, ONCLAW_TDX_THS_SIGN_CERTIFICATE, and ONCLAW_TDX_THS_SIGN_PASSWORD; set ONCLAW_TDX_THS_RELEASE_UNSIGNED=1 only for development artifacts",
  );
}
const sign = (path) =>
  run(signTool, [
    "sign",
    "/fd",
    "SHA256",
    "/f",
    certificate,
    "/p",
    password,
    "/tr",
    "http://timestamp.digicert.com",
    "/td",
    "SHA256",
    path,
  ]);
if (!unsigned) sign(join(releaseDir, executable));

const makensis = process.env.MAKENSIS ?? "makensis";
const installerPath = join(releaseDir, installer);
run(makensis, [
  `/DAGENT_EXE=${join(releaseDir, executable)}`,
  `/DOUTPUT_EXE=${installerPath}`,
  "/DPRODUCT_NAME=Onclaw TDX THS",
  `/DLICENSE_FILE=${join(root, "LICENSE.md")}`,
  join(root, "setup", "onclaw-tdx-ths-setup.nsi"),
]);
if (!unsigned) sign(installerPath);

const inventory = [];
for (const name of [executable, installer]) {
  const bytes = await readFile(join(releaseDir, name));
  inventory.push(`${createHash("sha256").update(bytes).digest("hex")}  ${name}`);
}
await writeFile(join(releaseDir, "SHA256SUMS.txt"), `${inventory.join("\n")}\n`);
console.log(`Agent release created at ${releaseDir}${unsigned ? " (unsigned development build)" : ""}`);
