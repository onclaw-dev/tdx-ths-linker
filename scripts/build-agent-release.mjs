import { createHash } from "node:crypto";
import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const releaseDir = join(root, "release", "agent");
const unsigned = process.env.TDX_THS_RELEASE_UNSIGNED === "1";
const variants = [
  {
    channel: "stable",
    productName: "TDX THS Linker",
    executable: "tdx-ths-linker-agent.exe",
    installer: "tdx-ths-linker-setup.exe",
  },
  {
    channel: "beta",
    productName: "TDX THS Linker Beta",
    executable: "tdx-ths-linker-agent-beta.exe",
    installer: "tdx-ths-linker-beta-setup.exe",
  },
];

const run = (command, args) => {
  const result = spawnSync(command, args, { cwd: root, stdio: "inherit", shell: false });
  if (result.status !== 0) throw new Error(`${command} failed with ${result.status}`);
};

run("cmake", ["-S", ".", "-B", "build"]);
run("cmake", ["--build", "build", "--config", "Release", "--target", "tdx-ths-linker-agents"]);
await mkdir(releaseDir, { recursive: true });
for (const variant of variants) {
  await copyFile(
    join(root, "build", "Release", variant.executable),
    join(releaseDir, variant.executable),
  );
}

const signTool = process.env.TDX_THS_SIGNTOOL;
const certificate = process.env.TDX_THS_SIGN_CERTIFICATE;
const password = process.env.TDX_THS_SIGN_PASSWORD;
if (!unsigned && (!signTool || !certificate || !password)) {
  throw new Error(
    "A signed release requires TDX_THS_SIGNTOOL, TDX_THS_SIGN_CERTIFICATE, and TDX_THS_SIGN_PASSWORD; set TDX_THS_RELEASE_UNSIGNED=1 only for development artifacts",
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
if (!unsigned) for (const variant of variants) sign(join(releaseDir, variant.executable));

const makensis = process.env.MAKENSIS ?? "makensis";
for (const variant of variants) {
  const installer = join(releaseDir, variant.installer);
  run(makensis, [
    `/DAGENT_EXE=${join(releaseDir, variant.executable)}`,
    `/DOUTPUT_EXE=${installer}`,
    `/DPRODUCT_NAME=${variant.productName}`,
    `/DRELEASE_CHANNEL=${variant.channel}`,
    `/DLICENSE_FILE=${join(root, "LICENSE.md")}`,
    join(root, "installer", "tdx-ths-linker.nsi"),
  ]);
  if (!unsigned) sign(installer);
}

const paths = variants.flatMap((variant) => [variant.executable, variant.installer]);
const inventory = [];
for (const name of paths) {
  const bytes = await readFile(join(releaseDir, name));
  inventory.push(`${createHash("sha256").update(bytes).digest("hex")}  ${name}`);
}
await writeFile(join(releaseDir, "SHA256SUMS.txt"), `${inventory.join("\n")}\n`);
console.log(`Agent release created at ${releaseDir}${unsigned ? " (unsigned development build)" : ""}`);
