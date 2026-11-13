#!/usr/bin/env node

import { runCli } from "./cli";
import { createAgentClient } from "./agent";
import { ensureAgent } from "./runtime";

let bootstrapError: unknown;
try {
  await ensureAgent();
} catch (error) {
  bootstrapError = error;
}

const exitCode = await runCli(process.argv.slice(2), {
  clientFactory: () => {
    if (bootstrapError) throw bootstrapError;
    return createAgentClient({ clientId: `cli-${process.pid}` });
  },
  version: "0.1.4",
});
process.exitCode = exitCode;
