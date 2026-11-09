import { readFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const scriptRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const versionPattern = /^v(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?)$/;

function requireMatch(text, pattern, label) {
  const match = text.match(pattern);
  if (!match) throw new Error(`Unable to read ${label} version`);
  return match[1];
}

export function releaseVersionFromTag(tag) {
  const match = String(tag ?? "").match(versionPattern);
  if (!match) throw new Error(`Release tag must match v<semver>; received ${tag || "<empty>"}`);
  return match[1];
}

export function validateVersionEntries(tag, entries) {
  const expected = releaseVersionFromTag(tag);
  const missing = entries.filter((entry) => !entry.version).map((entry) => entry.name);
  if (missing.length) throw new Error(`Missing release versions: ${missing.join(", ")}`);

  const mismatches = entries.filter((entry) => entry.version !== expected);
  if (mismatches.length) {
    const details = mismatches.map((entry) => `${entry.name}=${entry.version}`).join(", ");
    throw new Error(`Release tag ${tag} expects ${expected}; mismatched metadata: ${details}`);
  }
  return expected;
}

export async function readVersionEntries(root = scriptRoot) {
  const [rootPackageText, clientPackageText, lockText, pyproject, pythonPackage, cmake, agent, cliBin] = await Promise.all([
    readFile(join(root, "package.json"), "utf8"),
    readFile(join(root, "packages", "client", "package.json"), "utf8"),
    readFile(join(root, "package-lock.json"), "utf8"),
    readFile(join(root, "pyproject.toml"), "utf8"),
    readFile(join(root, "python", "src", "tdx_ths_linker", "__init__.py"), "utf8"),
    readFile(join(root, "CMakeLists.txt"), "utf8"),
    readFile(join(root, "native", "agent", "main.cpp"), "utf8"),
    readFile(join(root, "packages", "client", "src", "cli-bin.ts"), "utf8"),
  ]);
  const rootPackage = JSON.parse(rootPackageText);
  const clientPackage = JSON.parse(clientPackageText);
  const lock = JSON.parse(lockText);

  return [
    { name: "package.json", version: rootPackage.version },
    { name: "packages/client/package.json", version: clientPackage.version },
    { name: "package-lock.json root", version: lock.version },
    { name: "package-lock.json packages['']", version: lock.packages?.[""]?.version },
    {
      name: "package-lock.json packages/client",
      version: lock.packages?.["packages/client"]?.version,
    },
    {
      name: "pyproject.toml",
      version: requireMatch(pyproject, /^version\s*=\s*"([^"]+)"\s*$/m, "PyPI"),
    },
    {
      name: "python/src/tdx_ths_linker/__init__.py",
      version: requireMatch(pythonPackage, /^__version__\s*=\s*"([^"]+)"\s*$/m, "Python package"),
    },
    {
      name: "CMakeLists.txt",
      version: requireMatch(
        cmake,
        /project\(onclaw_tdx_ths\s+VERSION\s+([^\s)]+)/,
        "CMake",
      ),
    },
    {
      name: "native/agent/main.cpp",
      version: requireMatch(
        agent,
        /#define\s+ONCLAW_TDX_THS_VERSION\s+"([^"]+)"/,
        "Agent",
      ),
    },
    {
      name: "packages/client/src/cli-bin.ts",
      version: requireMatch(cliBin, /version:\s*"([^"]+)"/, "npm CLI"),
    },
  ];
}

export async function validateReleaseVersion(tag, root = scriptRoot) {
  const entries = await readVersionEntries(root);
  return validateVersionEntries(tag, entries);
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const tag = process.argv[2] || process.env.GITHUB_REF_NAME;
  try {
    const version = await validateReleaseVersion(tag);
    console.log(`Release version ${version} matches ${tag} across all package metadata.`);
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
  }
}
