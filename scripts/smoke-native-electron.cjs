const { resolve } = require("node:path");

try {
  if (!process.versions.electron) throw new Error("smoke test is not running in Electron");
  const binding = require(resolve(process.argv[2]));
  for (const name of ["initRuntime", "getRuntimeState", "setRuntimeCode"]) {
    if (typeof binding[name] !== "function") throw new Error(`native export is missing: ${name}`);
  }
  console.log(JSON.stringify({ runtime: "electron", version: process.versions.electron, status: "passed" }));
  process.exit(0);
} catch (error) {
  console.error(error);
  process.exit(1);
}
