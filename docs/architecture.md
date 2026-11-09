# 架构与发布产物

## 设计目标

项目只维护一套兼容性敏感的 C++ runtime，并通过不同边界提供给 Web、Node/Electron、Python 与原生调用方。目标发现、代码读取、写入队列和状态模型不会在各语言中重复实现。

## 组件关系

```text
                       ┌──────────────────────────┐
Web / browser ────────▶│ Windows Agent            │
  custom protocol      │ loopback HTTP + pairing  │
                       └────────────┬─────────────┘
                                    │
Electron / Node ── Node-API ────────┤
Python ─────────── ctypes + C ABI ──┤
Codex / Harness ── Skill + CLI ─────┤
C / C++ ────────── C ABI ───────────┤
                                    ▼
                       ┌──────────────────────────┐
                       │ shared C++ runtime       │
                       │ discovery / read / write │
                       └────────────┬─────────────┘
                                    ▼
                               TDX / THS
```

## 源码目录

| 目录 | 职责 |
| --- | --- |
| `native/core` | 共享 runtime、目标发现与读写逻辑 |
| `native/c-api` | 稳定的 C ABI 与 ABI smoke test |
| `native/node` | Node-API 边界，输出 `tdx-ths-linker.node` |
| `native/agent` | Web Agent、loopback HTTP 和自定义协议注册 |
| `packages/client` | npm Web/Node transport 与 TypeScript 类型 |
| `python` | Python `ctypes` 客户端及测试 |
| `skills/tdx-ths-linker` | Codex Skill、驱动合约和代理操作边界 |
| `setup` | NSIS 安装器 |
| `scripts` | 本机构建、发布和 wheel 校验脚本 |

## 发布产物

| 发布渠道 | 主要内容 |
| --- | --- |
| npm | JavaScript transport、类型声明和 npm README |
| PyPI | Python 包、类型标记和 `onclaw-tdx-ths.dll` |
| GitHub Release `.node` 包 | Electron 目标版本对应的 `tdx-ths-linker.node` 与 SHA-256 |
| GitHub Release Agent 便携包 | `onclaw-tdx-ths-agent-portable-win-x64.zip`，用于手动启动，不自动注册协议 |
| GitHub Release Agent 安装包 | `onclaw-tdx-ths-setup-win-x64.exe`，固定安装并注册 Web 唤起协议 |
| GitHub Release Skill 包 | Codex Skill 指令、UI 元数据和 CLI 合约 |

只有新的 `v*` 标签 push 会触发统一发布工作流。工作流先验证标签与 npm、PyPI、CMake、Agent 版本完全一致，再构建全部产物；普通 pull request 和 `main` push 不运行该发布 workflow。

## 图标资源

- 根目录 `icon.png`：GitHub、PyPI 和 npm 文档展示。
- 根目录 `icon.ico`：嵌入 Agent EXE，并作为 NSIS 安装器与卸载器图标。

图标文件是单一来源。文档和构建脚本应引用根目录资源，不应在子目录维护副本。

## 平台边界

- 当前仅支持 Windows x64。
- Python wheel 使用稳定 C ABI，不绑定具体 CPython 扩展 ABI。
- `.node` 使用 Node-API，但仍按目标 Electron 版本构建，以包含正确的 Windows/Electron 装载配置。
- Web Agent 是唯一需要安装与自定义协议注册的渠道。
- Web 场景使用安装包；便携 Agent 仅用于显式手动启动或调试。
- Codex Skill 不携带 native 二进制，只调用 PyPI 包安装的 CLI 驱动。
