# DeepSeek Harness 快速接入

Harness 通过 npm CLI 调用同一个 Windows Agent。npm 内嵌与 Web 安装包内部相同的 canonical bare Agent。

## 安装指令

```powershell
npm install tdx-ths-linker
New-Item -ItemType Directory -Force ".agents\skills" | Out-Null
Expand-Archive "<tdx-ths-linker-skill.zip>" -DestinationPath ".agents\skills" -Force
npx --no-install tdx-ths-linker --version
```

npm 安装不运行 Agent 安装程序。首次 CLI 命令会复用健康服务、优先启动 Web 安装版，否则直接启动 npm 包内 Agent。

## 测试指令

```powershell
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target ths --wait
```

## 测试结果返回示例

```json
{
  "channel": "deepseek-harness",
  "driver": "npm-agent-http",
  "skillDiscovered": true,
  "agentReached": true,
  "tdxConfirmed": true,
  "thsConfirmed": true
}
```
