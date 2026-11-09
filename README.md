# tdx-ths-linker

`tdx-ths-linker` 是一个 Windows 桌面股票软件联动组件，为 Web 和基于 Node/Electron 的 Harness 提供统一的通达信（TDX）与同花顺（THS）状态读取和股票代码联动能力。

当前版本为独立工程基线，尚未替换任何现有 Onclaw 前端实现。待本项目形成稳定、签名的正式发布后，上层项目应通过版本化产物接入，不应复制本仓库源码。

## 使用边界

- Web：安装 `tdx-ths-linker-agent.exe`，通过自定义协议和 loopback HTTP API 联动。
- Harness：加载 `tdx-ths-linker.node`，通过 Node-API 联动。
- 稳定版 Agent：支持读取 TDX/THS 状态和写入 TDX。
- Beta Agent：额外启用实验性的 THS 写入能力。
- 平台：当前仅支持 Windows x64。

## 授权

本项目源码公开，但不是 OSI 定义的开源软件。源码和社区二进制采用 [PolyForm Noncommercial 1.0.0](LICENSE.md)，仅允许许可证定义的非商业用途。商业使用需要单独的书面授权，参见 [COMMERCIAL-LICENSE.md](COMMERCIAL-LICENSE.md)。

安装器和运行时要求用户确认非商业许可证。该确认是许可证记录，不是联网账号认证；项目不依赖 Onclaw 账号、Onclaw API 或其他官方认证服务。

## 第三方名称声明

本项目是独立社区项目，与通达信、同花顺及其运营主体不存在隶属、授权、合作或背书关系。TDX、THS、通达信和同花顺等名称仅用于描述兼容目标，其商标归各自权利人所有。

## 架构

```text
Web application
  └─ @tdx-ths-linker/client
      └─ tdx-ths-linker:// activation
          └─ agent.exe ─┐
                        ├─ shared C++ runtime ── TDX / THS
Harness ── .node ──────┘
```

共享 C++ runtime 内置版本化运行 profile，不再从产品后端取得用户授权或进程偏移配置。`profiles/default-profile.json` 是当前 profile 的可审查镜像；正式发布前应建立 profile 生成及签名流程，避免二进制内置配置和 JSON 漂移。

## Web 接入

```ts
import { createAgentClient } from "@tdx-ths-linker/client";

const linker = createAgentClient();
const prepared = linker.prepareConnection!();

// 必须由用户点击等明确手势触发。
linker.openAgent!(prepared);
const connection = await linker.connect!(prepared);
if (!connection.ok) throw new Error(connection.message);

await linker.initialize(true);
await linker.setCode("tdx", "600000");
```

完整的本地协议见 [docs/protocol.md](docs/protocol.md)。

## Harness 接入

```ts
import { createRequire } from "node:module";
import { createAddonClient } from "@tdx-ths-linker/client";

const require = createRequire(import.meta.url);
const addon = require("./native/win-x64/tdx-ths-linker.node");
const linker = createAddonClient(addon);

await linker.initialize(true);
const state = await linker.getState();
```

`.node` 必须针对目标 Electron/Node ABI、Windows 架构和运行环境构建并随 Harness 分发。

## 构建

需要 Windows x64、MSVC、CMake、Node.js 和 NSIS。

```powershell
npm install
npm run build:client
npm run test:client
$env:TDX_THS_ELECTRON_VERSION = "<Harness Electron version>"
npm run build:native
```

开发用未签名 Agent：

```powershell
$env:TDX_THS_RELEASE_UNSIGNED = "1"
npm run release:agent
```

正式发布必须提供：

```text
TDX_THS_SIGNTOOL
TDX_THS_SIGN_CERTIFICATE
TDX_THS_SIGN_PASSWORD
```

发布脚本生成稳定版、Beta 版、对应安装器和 `SHA256SUMS.txt`。

## 安全说明

- Agent 只监听 `127.0.0.1:17654`。
- Web 必须经过自定义协议激活、一次性 nonce、精确 Origin 和短期 bearer token 配对。
- THS 写入涉及目标进程内存操作，仅在 Beta/实验构建启用。
- 在验证第三方软件版本兼容性前，不应将 fallback offset 视为长期稳定接口。
- 正式发行二进制必须进行 Authenticode 签名。

## 稳定发布前清单

- 在 Windows x64 上完成 C++、Node-API 和安装器构建。
- 验证 TDX/THS 支持版本和 profile。
- 验证 Web 配对、CORS、session 过期和单实例启动。
- 验证目标 Electron ABI 下的 `.node` 装载。
- 完成正式代码签名和校验文件验证。
- 建立 profile 签名、升级和回滚机制。
- 经过许可证与第三方软件兼容方式的法律审查。
