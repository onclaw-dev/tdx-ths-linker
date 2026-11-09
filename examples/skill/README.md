# Skill 快速接入

本示例不复制 Skill 正文；正式单一来源是仓库的 `skills/tdx-ths-linker`，发布时打包为 Skill ZIP。

## 安装指令

```powershell
npm install tdx-ths-linker
New-Item -ItemType Directory -Force ".agents\skills" | Out-Null
Expand-Archive "<tdx-ths-linker-skill.zip>" -DestinationPath ".agents\skills" -Force
```

## 测试指令

```powershell
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set <六位测试代码> --target ths --wait
npx --no-install tdx-ths-linker --json --accept-license state
```

再让支持 Skill 的 Harness 加载 `tdx-ths-linker`，执行同一顺序并核对 JSON。

## 测试结果返回示例

```json
{
  "skill": "tdx-ths-linker",
  "driver": "npm",
  "stateRead": true,
  "tdxConfirmed": true,
  "thsConfirmed": true,
  "finalStateRead": true
}
```
