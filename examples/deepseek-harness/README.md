# DeepSeek Harness 快速接入

DeepSeek Harness 使用项目级 `.agents/skills` 发现 Skill，并通过 npm CLI 操作本机 TDX/THS；不依赖 PyPI，也不需要 Harness 专用原生插件。

## 安装指令

在 Harness 打开的目标项目中执行：

```powershell
npm install tdx-ths-linker
New-Item -ItemType Directory -Force ".agents\skills" | Out-Null
Expand-Archive "<tdx-ths-linker-skill.zip>" -DestinationPath ".agents\skills" -Force
npx --no-install tdx-ths-linker --version
```

Skill ZIP 解压后应存在 `.agents/skills/tdx-ths-linker/SKILL.md`。命令不包含任何用户目录或本机 checkout 路径。

## 测试指令

先直接验证 npm 驱动：

```powershell
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target ths --wait
npx --no-install tdx-ths-linker --json --accept-license state
```

然后在使用 `standard` preset 的 Harness 会话中请求：

```text
请加载 tdx-ths-linker skill。先读取 TDX 与 THS；再把 TDX 设置为我提供的六位测试代码并确认；然后设置 THS 并确认；最后重新读取两端。我已阅读并接受 GPL-3.0-only 许可。任何一步未确认都不得报告成功。
```

## 测试结果返回示例

以下为 Harness 应汇总的格式示例，不代表已运行模型会话：

```json
{
  "channel": "deepseek-harness",
  "driver": "npm",
  "skillDiscovered": true,
  "driverValidation": "passed",
  "modelSession": "blocked-or-passed",
  "tdxConfirmed": true,
  "thsConfirmed": true
}
```
