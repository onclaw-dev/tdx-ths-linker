import { copyFile, mkdir } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const electronVersion = process.env.ONCLAW_TDX_THS_ELECTRON_VERSION;
const arch = process.env.ONCLAW_TDX_THS_NATIVE_ARCH ?? "x64";
const buildType = process.env.ONCLAW_TDX_THS_NATIVE_BUILD_TYPE ?? "Release";

if (!electronVersion || !/^\d+\.\d+\.\d+/.test(electronVersion)) {
  throw new Error("Set ONCLAW_TDX_THS_ELECTRON_VERSION to the consuming Harness Electron version");
}

if (process.platform !== "win32" && process.env.ONCLAW_TDX_THS_ALLOW_CROSS_NATIVE !== "1") {
  throw new Error("The Windows Node-API addon must be built on Windows with MSVC");
}

const cmakeJs = join(
  root,
  "node_modules",
  ".bin",
  process.platform === "win32" ? "cmake-js.cmd" : "cmake-js",
);
const result = spawnSync(
  cmakeJs,
  [
    "compile",
    "--runtime",
    "electron",
    "--runtime-version",
    electronVersion,
    "--arch",
    arch,
    "--config",
    buildType,
    "--target",
    "onclaw_tdx_ths_node",
  ],
  { cwd: root, stdio: "inherit", shell: process.platform === "win32" },
);
if (result.status !== 0) throw new Error(`cmake-js failed with ${result.status}`);

const source = join(root, "build", buildType, "tdx-ths-linker.node");
const target = join(root, "dist", "native", `win-${arch}`, "tdx-ths-linker.node");
const packageTarget = join(root, "packages", "client", "native", `win-${arch}`, "tdx-ths-linker.node");
await mkdir(dirname(target), { recursive: true });
await mkdir(dirname(packageTarget), { recursive: true });
await copyFile(source, target);
await copyFile(source, packageTarget);
console.log(`Native addon copied to ${target}`);
console.log(`Native addon staged for npm at ${packageTarget}`);
