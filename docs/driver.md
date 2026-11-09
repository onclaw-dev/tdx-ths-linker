# CLI 驱动与 Codex Skill

个人开发者通常直接使用 Python API。需要让 Codex、Harness、脚本或其他进程调用时，使用 PyPI 包附带的 CLI 驱动；Electron 内嵌场景仍可使用 npm transport 与 `.node`。

## 安装

```powershell
py -m pip install tdx-ths-linker
tdx-ths-linker --version
```

CLI 使用 wheel 内置的 `onclaw-tdx-ths.dll`，不依赖 Node、Electron 或 Agent EXE。

## 许可证确认

native 命令要求调用方明确确认 GPLv3：

```powershell
tdx-ths-linker --accept-license state
```

自动化环境可在用户已经确认后设置：

```powershell
$env:TDX_THS_LINKER_LICENSE_ACCEPTED = "1"
```

Skill 不会静默替用户接受许可证。

## 命令

诊断环境与目标软件：

```powershell
tdx-ths-linker --json --accept-license doctor
```

读取双端状态：

```powershell
tdx-ths-linker --json --accept-license state
```

设置单端或双端：

```powershell
tdx-ths-linker --json --accept-license set 600825 --target tdx --wait
tdx-ths-linker --json --accept-license set 600825 --target ths --wait
tdx-ths-linker --json --accept-license set 600825 --target both --wait
```

`--wait` 会轮询最终状态。双端请求只有在 TDX 与 THS 都返回目标代码时才会报告 `confirmed: true`。

## JSON 合约

机器模式只向 stdout 输出一个 schema v1 JSON envelope：

```json
{
  "schemaVersion": 1,
  "ok": true,
  "command": "set",
  "data": {
    "code": "600825",
    "targets": ["tdx", "ths"],
    "confirmed": true
  }
}
```

失败响应使用 `error.code`、`error.message` 和可选 `error.details`。退出码：

| 退出码 | 含义 |
| --- | --- |
| `0` | 成功 |
| `2` | 参数、配置或许可证确认错误 |
| `3` | 目标不可用 |
| `4` | 命令被 native runtime 拒绝 |
| `5` | 最终状态确认超时 |
| `6` | 平台、DLL、ABI、初始化或 native 调用错误 |
| `10` | 未预期驱动错误 |

## Codex Skill

Skill 源码位于 [`skills/tdx-ths-linker`](../skills/tdx-ths-linker)，标签发布时同时生成 `tdx-ths-linker-skill-<version>.zip`。

将 ZIP 中的 `tdx-ths-linker` 目录放入用户的 Codex skills 目录后即可使用。Skill 会在以下请求中自动提供帮助：

- 检查 TDX/THS 是否运行及当前代码。
- 读取双端状态。
- 将用户明确给出的六位代码设置到指定端或双端。
- 等待并验证双端最终状态。

Skill 不会猜测代码、自动安装软件、绕过许可证确认，或把“命令已接受”误报为“联动完成”。
