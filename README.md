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
| 独立服务 | `onclaw-tdx-ths-agent.exe` | 本机 HTTP 转发 | 本身即 Agent |

所有渠道复用相同的目标发现、状态读取和代码设置逻辑。TDX 与 THS 均支持读取和写入。

## 工作方式

```text
Web ── npm client ── onclaw-tdx-ths:// ── Agent ──┐
                                                   │
Electron/Harness ── tdx-ths-linker.node ──────────┤
Python ── ctypes ── onclaw-tdx-ths.dll ───────────┼── shared C++ runtime ── TDX / THS
Codex / Harness ── Skill ── Python CLI ───────────┤
C/C++ ── C ABI ── onclaw-tdx-ths.dll ─────────────┘
```

详细设计参见 [架构说明](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md)。

## 快速开始

### Web

先从对应版本的 GitHub Release 下载并安装：

```text
onclaw-tdx-ths-setup-win-x64.exe
```

安装器把 Agent 固定安装到 `%LOCALAPPDATA%\OnclawTdxThs`，并注册 `onclaw-tdx-ths://`。浏览器唤起依赖该注册，因此仅下载便携 ZIP 或手动运行 EXE 不等同于完成 Web 安装。便携包 `onclaw-tdx-ths-agent-portable-win-x64.zip` 适合显式手动启动、本地调试或不需要浏览器唤起的场景。

```bash
npm install tdx-ths-linker
```

```ts
import { createAgentClient } from "tdx-ths-linker";

const linker = createAgentClient();
const prepared = linker.prepareConnection!();

// 必须由用户点击等明确手势触发自定义协议。
linker.openAgent!(prepared);

const connection = await linker.connect!(prepared);
if (!connection.ok) throw new Error(connection.message);

await linker.initialize(true);
await linker.setCode("tdx", "600825");
await linker.setCode("ths", "600825");
```

Web 客户端只连接 `127.0.0.1:17654`。协议注册可通过已安装的 Agent 执行 `--verify-registration` 检查、用 `--repair` 修复；卸载器只移除自己持有的注册。配对、Origin 校验和会话规则参见 [本地 Agent 协议](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/protocol.md)。

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

`.node` 必须针对目标 Electron 版本和 Windows x64 环境构建，并由使用方随应用分发。

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

PyPI 包同时安装 `tdx-ths-linker` 命令，适合个人脚本、Harness 和 Codex：

```powershell
tdx-ths-linker --json --accept-license doctor
tdx-ths-linker --json --accept-license state
tdx-ths-linker --json --accept-license set 600825 --target both --wait
```

Codex Skill 位于 `skills/tdx-ths-linker`，也会作为独立 ZIP 附加到 GitHub Release。Skill 不包含二进制文件，只通过已安装的 Python CLI 驱动操作。安装和 JSON 合约参见 [CLI 驱动与 Skill](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/driver.md)。

## 文档

- [架构与发布产物](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/architecture.md)
- [本地 Agent 协议](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/protocol.md)
- [开发、测试与发布](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/development.md)
- [CLI 驱动与 Codex Skill](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/docs/driver.md)
- [npm 客户端说明](https://github.com/onclaw-dev/tdx-ths-linker/blob/main/packages/client/README.md)

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
