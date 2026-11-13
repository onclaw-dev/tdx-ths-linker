import type { LinkerRuntimeState, LinkerTarget, OnclawClient } from "./contracts";

export const CLI_SCHEMA_VERSION = 1;
export const CLI_EXIT = {
  ok: 0,
  usage: 2,
  unavailable: 3,
  rejected: 4,
  timeout: 5,
  agent: 6,
  unexpected: 10,
} as const;

interface CliDependencies {
  clientFactory: () => OnclawClient;
  stdout?: (value: string) => void;
  stderr?: (value: string) => void;
  sleep?: (milliseconds: number) => Promise<void>;
  now?: () => number;
  version?: string;
  platform?: string;
  architecture?: string;
  licenseEnvironment?: string;
}

interface ParsedArguments {
  command: "doctor" | "state" | "set" | "version";
  json: boolean;
  licenseAccepted: boolean;
  code?: string;
  target?: LinkerTarget | "both";
  wait: boolean;
  timeoutMs: number;
  pollIntervalMs: number;
}

class CliError extends Error {
  constructor(
    readonly exitCode: number,
    readonly code: string,
    message: string,
    readonly details?: unknown,
  ) {
    super(message);
  }
}

const parsePositiveSeconds = (value: string, name: string): number => {
  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed <= 0) throw new CliError(CLI_EXIT.usage, "invalid_usage", `${name} must be greater than zero`);
  return parsed * 1000;
};

function parseArguments(argv: string[], environmentAcceptance: string): ParsedArguments {
  if (argv.includes("--version")) {
    return { command: "version", json: false, licenseAccepted: false, wait: false, timeoutMs: 10_000, pollIntervalMs: 250 };
  }
  const commandAt = argv.findIndex((value) => value === "doctor" || value === "state" || value === "set");
  if (commandAt < 0) throw new CliError(CLI_EXIT.usage, "invalid_usage", "command must be doctor, state, or set");
  const command = argv[commandAt] as ParsedArguments["command"];
  const parsed: ParsedArguments = {
    command,
    json: argv.includes("--json"),
    licenseAccepted: argv.includes("--accept-license") || /^(1|true|yes)$/i.test(environmentAcceptance.trim()),
    wait: argv.includes("--wait"),
    timeoutMs: command === "set" ? 10_000 : 5_000,
    pollIntervalMs: 250,
  };
  if (command === "set") parsed.code = argv[commandAt + 1];

  for (let index = 0; index < argv.length; index += 1) {
    const value = argv[index];
    if (value === "--target") parsed.target = argv[++index] as ParsedArguments["target"];
    else if (value === "--timeout") parsed.timeoutMs = parsePositiveSeconds(argv[++index] ?? "", "timeout");
    else if (value === "--poll-interval") parsed.pollIntervalMs = parsePositiveSeconds(argv[++index] ?? "", "poll interval");
  }
  if (command === "set") {
    if (!parsed.code || !/^\d{6}$/.test(parsed.code)) throw new CliError(CLI_EXIT.usage, "invalid_usage", "code must be six ASCII digits");
    if (!parsed.target || !["tdx", "ths", "both"].includes(parsed.target)) throw new CliError(CLI_EXIT.usage, "invalid_usage", "set requires --target tdx|ths|both");
  }
  return parsed;
}

const targetsFor = (target: LinkerTarget | "both"): LinkerTarget[] => target === "both" ? ["tdx", "ths"] : [target];

const safeEndpoint = (state: LinkerRuntimeState, target: LinkerTarget) => ({
  name: target,
  code: state[target].code,
  health: state[target].health,
  available: state[target].available,
  lastError: state[target].lastError,
});

export async function runCli(argv: string[], dependencies: CliDependencies): Promise<number> {
  const stdout = dependencies.stdout ?? console.log;
  const stderr = dependencies.stderr ?? console.error;
  const sleep = dependencies.sleep ?? ((milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds)));
  const now = dependencies.now ?? Date.now;
  let command = "unknown";
  let jsonMode = argv.includes("--json");
  const emit = (value: unknown, error = false): void => {
    const output = JSON.stringify(value);
    if (jsonMode) stdout(output);
    else (error ? stderr : stdout)(JSON.stringify(value, null, 2));
  };

  try {
    const args = parseArguments(argv, dependencies.licenseEnvironment ?? process.env.TDX_THS_LINKER_LICENSE_ACCEPTED ?? "");
    command = args.command;
    jsonMode = args.json;
    if (args.command === "version") {
      stdout(`tdx-ths-linker ${dependencies.version ?? "0.0.0"}`);
      return CLI_EXIT.ok;
    }
    if (!args.licenseAccepted) throw new CliError(CLI_EXIT.usage, "license_not_accepted", "Pass --accept-license or set TDX_THS_LINKER_LICENSE_ACCEPTED=1");

    let client: OnclawClient;
    try {
      client = dependencies.clientFactory();
    } catch (error) {
      throw new CliError(CLI_EXIT.agent, "agent_unavailable", error instanceof Error ? error.message : String(error));
    }
    let initialized;
    try {
      initialized = await client.initialize(true);
    } catch (error) {
      throw new CliError(CLI_EXIT.agent, "initialize_failed", error instanceof Error ? error.message : String(error));
    }
    if (!initialized.ok) throw new CliError(CLI_EXIT.agent, "initialize_failed", initialized.message, initialized);
    const initialDeadline = now() + Math.min(args.timeoutMs, 5_000);
    let state = await client.getState();
    while (state.runtimeHealth === "initializing" && now() < initialDeadline) {
      await sleep(Math.min(args.pollIntervalMs, initialDeadline - now()));
      state = await client.getState();
    }

    let data: unknown;
    if (args.command === "doctor") {
      data = {
        packageVersion: dependencies.version ?? "0.0.0",
        schemaVersion: CLI_SCHEMA_VERSION,
        platform: dependencies.platform ?? process.platform,
        architecture: dependencies.architecture ?? process.arch,
        initialize: initialized,
        runtime: {
          initialized: state.initialized,
          supported: state.isSupported,
          runtimeHealth: state.runtimeHealth,
          licenseState: state.licenseState,
        },
        endpoints: { tdx: safeEndpoint(state, "tdx"), ths: safeEndpoint(state, "ths") },
      };
    } else if (args.command === "state") {
      data = state;
    } else {
      const targets = targetsFor(args.target!);
      const unavailable = targets.filter((target) => !state[target].available);
      if (unavailable.length) throw new CliError(CLI_EXIT.unavailable, "target_unavailable", "Requested desktop target is unavailable", { targets, state: Object.fromEntries(unavailable.map((target) => [target, safeEndpoint(state, target)])) });
      const commands = Object.fromEntries(await Promise.all(targets.map(async (target) => [target, await client.setCode(target, args.code!)])));
      const rejected = targets.filter((target) => !commands[target].accepted);
      if (rejected.length) throw new CliError(CLI_EXIT.rejected, "command_rejected", "One or more targets rejected the command", { targets: rejected, commands });
      data = { code: args.code, targets, commands, confirmed: false };
      if (args.wait) {
        const deadline = now() + args.timeoutMs;
        while (true) {
          state = await client.getState();
          const confirmedTargets = targets.filter((target) => state[target].code === args.code);
          if (confirmedTargets.length === targets.length) {
            data = { ...data, confirmed: true, state: Object.fromEntries(targets.map((target) => [target, safeEndpoint(state, target)])) };
            break;
          }
          if (now() >= deadline) throw new CliError(CLI_EXIT.timeout, "confirmation_timeout", "Timed out waiting for requested targets to report the code", { code: args.code, confirmedTargets, state: Object.fromEntries(targets.map((target) => [target, safeEndpoint(state, target)])) });
          await sleep(Math.min(args.pollIntervalMs, Math.max(0, deadline - now())));
        }
      }
    }
    emit({ schemaVersion: CLI_SCHEMA_VERSION, ok: true, command, data });
    return CLI_EXIT.ok;
  } catch (error) {
    const failure = error instanceof CliError
      ? error
      : new CliError(CLI_EXIT.unexpected, "unexpected_error", error instanceof Error ? error.message : String(error));
    emit({ schemaVersion: CLI_SCHEMA_VERSION, ok: false, command, error: { code: failure.code, message: failure.message, ...(failure.details === undefined ? {} : { details: failure.details }) } }, true);
    return failure.exitCode;
  }
}
