<p align="center">
  <img src="https://raw.githubusercontent.com/onclaw-dev/tdx-ths-linker/main/imgs/icon.png" width="96" height="96" alt="tdx-ths-linker icon">
</p>

# tdx-ths-linker

Web Agent and Node-API transport adapters for TDX/THS desktop integration on Windows x64.

## Install

```bash
npm install tdx-ths-linker
```

## Web Agent transport

```ts
import { createStockLinkRuntime, createWebLinker } from "tdx-ths-linker";

const web = createWebLinker();

button.addEventListener("click", () => {
  // Protocol activation occurs synchronously before the returned Promise waits.
  web.connect({ licenseAccepted: licenseCheckbox.checked }).then((connection) => {
    if (connection.status === "installer_required") {
      installerLink.href = connection.installerUrl;
      installerLink.hidden = false;
      return;
    }
    if (!connection.ok) throw new Error(connection.message);

    createStockLinkRuntime({
      client: web.client,
      mode: "ths-to-tdx",
      onState: console.log,
    }).start();
  });
});
```

Call `connect()` from a real user gesture on Windows HTTPS or localhost. It checks the environment, opens `onclaw-tdx-ths://`, retries the authenticated loopback pairing, initializes after explicit GPL acceptance, and returns structured recovery states. `installer_required` includes the official installer URL; pass `installerUrl` to `createWebLinker()` to use a pinned release or mirror.

The automatic runtime supports `off`, `read-only`, `ths-to-tdx`, and `tdx-to-ths`. The first observation after startup, direction change, or source recovery establishes a baseline and never overwrites the other application. The transport connects only to `127.0.0.1:17654`.

The low-level `createAgentClient()` API remains available for hosts that need to own the activation and retry policy. See the repository's `examples/web-quickstart` for a no-login standalone HTML example. Production applications should install this package and use a bundler; the pinned CDN import is a quickstart convenience.

## Node / Electron transport

```ts
import { createNativeClient } from "tdx-ths-linker/native";

const client = createNativeClient();

await client.initialize(true);
const state = await client.getState();
```

The Windows x64 package contains `native/win-x64/tdx-ths-linker.node`. Set `TDX_THS_LINKER_NODE_PATH` only when testing a local build. The package is smoke-tested in Node.js and the declared Electron runtime before publication.

For Codex, Harness, or process-based automation, use the npm JSON CLI and separately released Skill:

```powershell
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target ths --wait
```

## Documentation

See the [project README](https://github.com/onclaw-dev/tdx-ths-linker#readme), [architecture](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md), and [local protocol](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/protocol.md).

Licensed under [GNU GPL v3.0 only](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/LICENSE).
