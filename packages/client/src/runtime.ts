import { createHash } from "node:crypto";
import { spawn, spawnSync } from "node:child_process";
import { mkdir, open, readFile, rm, stat } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const PROTOCOL_VERSION = 1;
const AGENT_NAME = "onclaw-tdx-ths-agent.exe";

export interface EnsureAgentOptions {
  fetch?: typeof globalThis.fetch;
  baseUrl?: string;
  localAppData?: string;
  packagedAgentPath?: string;
  manifestPath?: string;
  installedAgentPath?: string;
  timeoutMs?: number;
  pollIntervalMs?: number;
  platform?: NodeJS.Platform;
  arch?: string;
  launch?: (agentPath: string) => void;
}

export interface EnsureAgentResult {
  status: "running" | "started";
  source: "existing" | "installed" | "packaged";
  agentVersion: string;
}

const sleep = (milliseconds: number) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function health(request: typeof globalThis.fetch, baseUrl: string): Promise<string | undefined> {
  try {
    const response = await request(`${baseUrl}/v1/health`, { headers: { Accept: "application/json" } });
    if (!response.ok) return undefined;
    const value = await response.json() as { ok?: boolean; protocolVersion?: number; agentVersion?: string };
    if (value.ok !== true || value.protocolVersion !== PROTOCOL_VERSION || typeof value.agentVersion !== "string") return undefined;
    return value.agentVersion;
  } catch {
    return undefined;
  }
}

async function waitForHealth(request: typeof globalThis.fetch, baseUrl: string, timeoutMs: number, pollIntervalMs: number): Promise<string | undefined> {
  const deadline = Date.now() + timeoutMs;
  do {
    const version = await health(request, baseUrl);
    if (version) return version;
    await sleep(pollIntervalMs);
  } while (Date.now() < deadline);
  return undefined;
}

function launchAgent(agentPath: string): void {
  const child = spawn(agentPath, [], { detached: true, stdio: "ignore", windowsHide: true });
  child.unref();
}

function discoverInstalledAgent(): string | undefined {
  const query = spawnSync(
    "reg.exe",
    ["query", "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OnclawTdxThs", "/v", "InstallLocation"],
    { encoding: "utf8", windowsHide: true },
  );
  if (query.status !== 0 || !query.stdout) return undefined;
  const location = query.stdout.match(/InstallLocation\s+REG_SZ\s+(.+)$/im)?.[1]?.trim();
  return location ? join(location, AGENT_NAME) : undefined;
}

async function verifyPackagedAgent(agentPath: string, manifestPath: string): Promise<void> {
  const [bytes, manifest] = await Promise.all([readFile(agentPath), readFile(manifestPath, "utf8")]);
  const expected = manifest.split(/\r?\n/).map((line) => line.trim().split(/\s+/)).find((parts) => parts.at(-1) === AGENT_NAME)?.[0];
  if (!expected || !/^[a-f0-9]{64}$/i.test(expected)) throw new Error("Agent checksum is missing or invalid");
  const actual = createHash("sha256").update(bytes).digest("hex");
  if (actual.toLowerCase() !== expected.toLowerCase()) throw new Error("Agent checksum mismatch");
}

export async function ensureAgent(options: EnsureAgentOptions = {}): Promise<EnsureAgentResult> {
  const platform = options.platform ?? process.platform;
  const arch = options.arch ?? process.arch;
  if (platform !== "win32" || arch !== "x64") throw new Error(`Agent runtime supports Windows x64; received ${platform}-${arch}`);
  const request = options.fetch ?? globalThis.fetch;
  if (!request) throw new Error("Agent runtime requires fetch");
  const baseUrl = options.baseUrl ?? "http://127.0.0.1:17654";
  const existing = await health(request, baseUrl);
  if (existing) return { status: "running", source: "existing", agentVersion: existing };

  const localAppData = options.localAppData ?? process.env.LOCALAPPDATA;
  if (!localAppData) throw new Error("LOCALAPPDATA is unavailable");
  const packagedDir = fileURLToPath(new URL("../agent/win-x64/", import.meta.url));
  const packagedAgentPath = options.packagedAgentPath ?? join(packagedDir, AGENT_NAME);
  const manifestPath = options.manifestPath ?? join(packagedDir, "SHA256SUMS.txt");
  const installedAgentPath = options.installedAgentPath ?? discoverInstalledAgent();
  const lockPath = join(localAppData, "OnclawTdxThs.transition.lock");
  const timeoutMs = options.timeoutMs ?? 10_000;
  const pollIntervalMs = options.pollIntervalMs ?? 200;
  const launch = options.launch ?? launchAgent;
  await mkdir(localAppData, { recursive: true });

  let lock;
  const lockDeadline = Date.now() + timeoutMs;
  while (!lock) {
    try {
      lock = await open(lockPath, "wx");
      await lock.writeFile(String(process.pid));
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== "EEXIST") throw error;
      if (Date.now() >= lockDeadline) throw new Error("Timed out waiting for Agent transition lock");
      await sleep(pollIntervalMs);
    }
  }

  try {
    const raced = await health(request, baseUrl);
    if (raced) return { status: "running", source: "existing", agentVersion: raced };
    const installed = installedAgentPath
      ? await stat(installedAgentPath).then((value) => value.isFile()).catch(() => false)
      : false;
    const source = installed ? "installed" : "packaged";
    const agentPath = installed ? installedAgentPath! : packagedAgentPath;
    if (!installed) await verifyPackagedAgent(packagedAgentPath, manifestPath);
    launch(agentPath);
    const version = await waitForHealth(request, baseUrl, timeoutMs, pollIntervalMs);
    if (!version) throw new Error(`Agent did not become healthy after starting ${source} runtime`);
    return { status: "started", source, agentVersion: version };
  } finally {
    await lock.close();
    await rm(lockPath, { force: true });
  }
}
