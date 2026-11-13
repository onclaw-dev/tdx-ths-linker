# CLI 驱动与 Harness

## 安装

```powershell
npm install tdx-ths-linker
npx --no-install tdx-ths-linker --version
```

npm 不运行 lifecycle 安装脚本。首次 CLI 调用会复用健康服务、优先启动 Web 安装版，否则直接启动包内裸 Agent。

launcher 会依次检查健康状态、启动已有 Web 安装版或包内 Agent，并等待 `/v1/health` 就绪。CLI 本身不读取或写入目标进程，也不修改注册表。

## 命令

```powershell
npx --no-install tdx-ths-linker --json --accept-license doctor
npx --no-install tdx-ths-linker --json --accept-license state
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target tdx --wait
npx --no-install tdx-ths-linker --json --accept-license set 600825 --target ths --wait
```

`--wait` 会继续通过 Agent 轮询并确认最终状态。命令进入同一个全局调度器，不能把“已接受”误报为“目标已切换”。

## 退出码

| 退出码 | 含义 |
| --- | --- |
| `0` | 成功 |
| `2` | 参数、配置或许可确认错误 |
| `3` | 目标不可用 |
| `4` | Agent 拒绝命令 |
| `5` | 最终状态确认超时 |
| `6` | Agent 启动、连接或初始化失败 |
| `10` | 未预期错误 |

Harness 与 Codex Skill 只调用该 CLI，不加载任何平台原生模块。

## Python 驱动

PyPI 提供相同的 Agent HTTP 边界：

```powershell
py -m pip install tdx-ths-linker
tdx-ths-linker-python ensure-agent
tdx-ths-linker-python --json --accept-license state
```

wheel 内嵌 canonical bare Agent，但不包含安装程序、DLL 或 PYD。Python 首次使用时执行幂等 Agent launcher。
