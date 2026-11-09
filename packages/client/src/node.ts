import type { NativeNode, OnclawClient } from "./contracts";

export function createNodeClient(node: NativeNode): OnclawClient {
  return {
    transport: "node",
    initialize: async (licenseAccepted) => node.initRuntime({ licenseAccepted }),
    getState: async () => node.getRuntimeState(),
    setCode: async (target, code) => node.setRuntimeCode(target, code),
  };
}
