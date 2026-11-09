<p align="center">
  <img src="https://raw.githubusercontent.com/onclaw-dev/tdx-ths-linker/main/icon.png" width="128" height="128" alt="tdx-ths-linker icon">
</p>

<h1 align="center">tdx-ths-linker</h1>

<p align="center">
  Windows 多端通达信（TDX）与同花顺（THS）联动组件
</p>

`tdx-ths-linker` 通过一套共享 C++ runtime，为 Web、Node/Electron、Python 和 Windows 原生程序提供统一的股票代码读取与设置能力。当前仅支持 Windows x64。

> 本项目是独立社区项目，与通达信、同花顺及其运营主体不存在隶属、授权、合作或背书关系。相关名称仅用于描述兼容目标，商标归各自权利人所有。

## 支持的接入方式

| 渠道 | 产物 | 接入方式 | 是否需要 Agent |
| --- | --- | --- | --- |
| Web | `tdx-ths-linker` npm 包 + Agent 安装包 | 自定义协议激活与 loopback HTTP | 是 |
| Node/Electron | `tdx-ths-linker.node` | Node-API | 否 |
| Python | `tdx-ths-linker` wheel | `ctypes` 加载内置 DLL | 否 |
| Codex / 自动化 | `tdx-ths-linker` Skill + CLI | JSON 命令驱动 | 否 |
| C/C++ | `onclaw-tdx-ths.dll` | 稳定 C ABI | 否 |

所有渠道复用相同的目标发现、状态读取和代码设置逻辑。TDX 与 THS 均支持读取和写入。

## 工作方式

```text
Web ── npm client ── onclaw-tdx-ths:// ── Agent ──┐
                                                   │
Electron/Harness ── tdx-ths-linker.node ──────────┤
Python ── ctypes ── onclaw-tdx-ths.dll ───────────┼── shared C++ runtime ── TDX / THS
Codex / Harness ── Skill ── npm JSON CLI ─────────┤
C/C++ ── C ABI ── onclaw-tdx-ths.dll ─────────────┘
```

详细设计参见 [架构说明](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md)。

## 快速开始

### Web

```bash
npm install tdx-ths-linker
```

```ts
import { createWebLinker } from "tdx-ths-linker";

const linker = createWebLinker();

// 必须在用户点击处理函数内调用，协议唤醒会同步发生。
const connection = await linker.connect({ licenseAccepted: true });
if (connection.status === "installer_required") {
  location.href = connection.installerUrl;
} else if (!connection.ok) {
  throw new Error(connection.message);
}

await linker.client.setCode("tdx", "600825");
await linker.client.setCode("ths", "600825");
```

Web 客户端只连接 `127.0.0.1:17654`。正式 GitHub Release 只提供会注册 `onclaw-tdx-ths://` 的安装包，不把内部 Agent EXE 作为独立产品发布。完整的无登录 HTML 示例、自动联动和安装恢复流程参见 [Web 快速接入](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/web-integration.md)。

### Node / Electron

```ts
import { createRequire } from "node:module";
import { createNodeClient } from "tdx-ths-linker";

const require = createRequire(import.meta.url);
const native = require("./native/win-x64/tdx-ths-linker.node");
const linker = createNodeClient(native);

await linker.initialize(true);
const state = await linker.getState();
await linker.setCode("tdx", "600825");
await linker.setCode("ths", "600825");
```

Windows x64 的 npm 包已内置 `tdx-ths-linker.node`，Node/Electron 入口可从 `tdx-ths-linker/native` 加载；`TDX_THS_LINKER_NODE_PATH` 可在本地开发时覆盖二进制路径。发布工作流仍会额外附加独立 `.node` ZIP，供非 npm 宿主使用。

### Python

```powershell
py -m pip install tdx-ths-linker
```

```python
from tdx_ths_linker import Client

with Client(license_accepted=True) as linker:
    state = linker.get_state()
    print(state.tdx.code, state.ths.code)

    linker.set_code("tdx", "600825")
    linker.set_code("ths", "600825")
```

Python wheel 内置 `onclaw-tdx-ths.dll`，不启动 Agent，也不使用本地 HTTP。`set_code()` 返回命令是否已接受；应继续轮询 `get_state()` 确认目标软件已完成切换。

### CLI 驱动与 Codex Skill

Harness、Codex 和进程自动化统一使用 npm 包内置的 JSON CLI：

```powershell
npx --no-install tdx-ths-linker --json --accept-license doctor
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target ths --wait
```

Codex Skill 位于 `skills/tdx-ths-linker`，也会作为独立 ZIP 附加到 GitHub Release。Skill 不包含二进制文件，只调用项目中已安装的 npm CLI。安装和 JSON 合约参见 [CLI 驱动与 Skill](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/driver.md)。

## 文档

- [Web HTML 示例](https://github.com/onclaw-dev/tdx-ths-linker/tree/main/examples/web-quickstart)
- [Python 示例](https://github.com/onclaw-dev/tdx-ths-linker/tree/main/examples/python)
- [Electron 示例](https://github.com/onclaw-dev/tdx-ths-linker/tree/main/examples/electron)
- [DeepSeek Harness 示例](https://github.com/onclaw-dev/tdx-ths-linker/tree/main/examples/deepseek-harness)
- [Skill 示例](https://github.com/onclaw-dev/tdx-ths-linker/tree/main/examples/skill)
- [架构与发布产物](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md)
- [本地 Agent 协议](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/protocol.md)
- [开发、测试与发布](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/development.md)
- [CLI 驱动与 Codex Skill](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/driver.md)
- [npm 客户端说明](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/packages/client/README.md)
- [Web 快速接入](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/web-integration.md)

## 安全与兼容性

- Agent 只监听 `127.0.0.1:17654`，不会绑定公网接口。
- Web 调用必须经过一次性 nonce、自定义协议激活、精确 Origin 和短期 bearer token 配对。
- THS 写入涉及目标进程内存操作；发布前必须针对目标软件版本完成真实兼容测试。
- fallback offset 不是长期稳定接口，不应在未验证的软件版本上假定可用。
- 正式发布的 EXE、DLL 和 `.node` 应进行 Authenticode 签名并记录 SHA-256。
- 进程内存行为可能触发安全软件启发式检测，应使用透明构建、代码签名和误报申诉流程。

安全问题请避免公开披露可利用细节，优先通过仓库的私密安全报告渠道提交。

## 许可证

本项目采用 [GNU General Public License v3.0 only](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/LICENSE)。分发本项目或其修改版本时，必须遵守 GPLv3 对应的源码提供义务。
