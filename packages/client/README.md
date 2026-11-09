<p align="center">
  <img src="https://raw.githubusercontent.com/onclaw-dev/tdx-ths-linker/main/icon.png" width="96" height="96" alt="tdx-ths-linker icon">
</p>

# tdx-ths-linker

Web Agent and Node-API transport adapters for TDX/THS desktop integration on Windows x64.

## Install

```bash
npm install tdx-ths-linker
```

## Web Agent transport

```ts
import { createAgentClient } from "tdx-ths-linker";

const client = createAgentClient();
const prepared = client.prepareConnection!();

client.openAgent!(prepared);
const connection = await client.connect!(prepared);
if (!connection.ok) throw new Error(connection.message);

await client.initialize(true);
await client.setCode("tdx", "600825");
await client.setCode("ths", "600825");
```

The Web transport requires `onclaw-tdx-ths-setup-win-x64.exe` from the matching GitHub Release. The installer places the Agent at a stable per-user path and registers `onclaw-tdx-ths://`; extracting the portable Agent ZIP does not register browser activation. The transport connects only to `127.0.0.1:17654`.

## Node / Electron transport

```ts
import { createNodeClient } from "tdx-ths-linker";

const native = require("./native/win-x64/tdx-ths-linker.node");
const client = createNodeClient(native);

await client.initialize(true);
const state = await client.getState();
```

The native `.node` binary is released separately because it must match the target Electron runtime and Windows architecture.

For Codex or process-based automation, prefer the PyPI-installed `tdx-ths-linker` CLI and the separately released Codex Skill. Use this npm transport when embedding the integration into a Web or Node/Electron host.

## Documentation

See the [project README](https://github.com/onclaw-dev/tdx-ths-linker#readme), [architecture](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md), and [local protocol](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/protocol.md).

Licensed under [GNU GPL v3.0 only](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/LICENSE).
