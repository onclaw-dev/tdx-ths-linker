# Skill 快速接入

## 安装指令

```powershell
npm install tdx-ths-linker
New-Item -ItemType Directory -Force ".agents\skills" | Out-Null
Expand-Archive "<tdx-ths-linker-skill.zip>" -DestinationPath ".agents\skills" -Force
```

npm 安装会确保同一份用户级 Windows Agent 已部署。

## 测试指令

```powershell
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target ths --wait
```

## 测试结果返回示例

```json
{
  "skill": "tdx-ths-linker",
  "driver": "npm-agent-http",
  "agentReached": true,
  "tdxConfirmed": true,
  "thsConfirmed": true
}
```
