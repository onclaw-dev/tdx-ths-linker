import type { NativeAddon, TdxThsLinkerClient } from "./contracts";

export function createAddonClient(addon: NativeAddon): TdxThsLinkerClient {
  return {
    transport: "addon",
    initialize: async (licenseAccepted) => addon.initRuntime({ licenseAccepted }),
    getState: async () => addon.getRuntimeState(),
    setCode: async (target, code) => addon.setRuntimeCode(target, code),
  };
}
