import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";

import type { NativeNode, OnclawClient } from "./contracts";
import { createNodeClient } from "./node";

export interface NativeLoaderOptions {
  path?: string;
  platform?: NodeJS.Platform;
  arch?: string;
}

export function packagedNativePath(): string {
  return fileURLToPath(new URL("../native/win-x64/tdx-ths-linker.node", import.meta.url));
}

export function loadNativeNode(options: NativeLoaderOptions = {}): NativeNode {
  const platform = options.platform ?? process.platform;
  const arch = options.arch ?? process.arch;
  if (platform !== "win32" || arch !== "x64") {
    throw new Error(`tdx-ths-linker native module supports Windows x64; received ${platform}-${arch}`);
  }
  const path = options.path ?? process.env.TDX_THS_LINKER_NODE_PATH ?? packagedNativePath();
  try {
    return createRequire(import.meta.url)(path) as NativeNode;
  } catch (error) {
    const reason = error instanceof Error ? error.message : String(error);
    throw new Error(`Unable to load tdx-ths-linker native module at ${path}: ${reason}`, { cause: error });
  }
}

export function createNativeClient(options: NativeLoaderOptions = {}): OnclawClient {
  return createNodeClient(loadNativeNode(options));
}
