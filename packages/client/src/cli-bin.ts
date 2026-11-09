#!/usr/bin/env node

import { runCli } from "./cli";
import { createNativeClient } from "./native";

const exitCode = await runCli(process.argv.slice(2), {
  clientFactory: createNativeClient,
  version: "0.1.2",
});
process.exitCode = exitCode;
