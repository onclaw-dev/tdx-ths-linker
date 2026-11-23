# 架构与发布产物

## 单一运行时

Windows Agent 是唯一允许访问 TDX/THS 进程的公共运行时。网页、Node、Electron、Python、CLI 和 Harness 都通过 `http://127.0.0.1:17654` 调用同一个 Agent。

```text
Web / Node / Electron / Python / CLI / Harness
                  |
          loopback HTTP
                  |
         Windows Agent EXE
         - 全局状态快照
         - 请求去重
         - TDX/THS 独立首尾合并
         - 活跃唤醒与闲置释放
                  |
              TDX / THS
```

`native/core` 只静态链接进 Agent 和原生测试，不再提供 Node-API、C ABI 或 Python 原生运行时。

## 目标轮询生命周期

runtime 为 TDX 和 THS 各维护一个进程级轮询 session。首次发现目标时，session 缓存 PID、进程创建时间、模块基址、模块大小、进程句柄和已验证的代码偏移；目标进程身份不变且小范围代码读取持续成功时，后续轮询直接复用这些信息，不再重复枚举进程、枚举模块或扫描模块映像。

生命周期由业务活动驱动：持续联动和待确认命令处于 `hot`；业务心跳停止后进入 `warm`，立即停止目标内存读取但保留只读句柄；最后活动满 5 分钟进入 `cool` 并关闭句柄，仅保留可验证的进程/偏移提示；满 10 分钟进入 `cold` 并清空进程缓存。健康检查不续期，状态读取或合法写入会唤醒 runtime。

进程退出、PID 创建时间变化或缓存偏移连续三次（或约一秒）读取失败时，runtime 才使缓存失效。进程发现按 250 ms–2 s 退避，偏移解析按 4–60 s 独立退避。解析只扫描 PE 可执行节，并验证所有动态候选和 fallback；代码必须是恰好六个连续 ASCII 数字。唤醒后的状态带有代次和新鲜度证据，客户端不得用 `stale` 快照建立联动基线。

## 目录职责

| 目录 | 职责 |
| --- | --- |
| `native/core` | Agent 内部的目标发现、读取、写入与调度 |
| `native/agent` | 单实例 Agent、loopback HTTP 与自定义协议 |
| `packages/client` | HTTP 客户端、CLI、launcher 和内嵌裸 Agent |
| `python` | PyPI HTTP 客户端、CLI、launcher 和内嵌裸 Agent |
| `setup` | 用户级 NSIS 安装程序 |
| `skills/tdx-ths-linker` | Harness/Codex 操作边界 |

## 单一 Agent、三种封装

发布流程只构建一次 `onclaw-tdx-ths-agent.exe`。相同字节和校验值进入 npm、PyPI wheel 与 Web 注册安装包：

```text
canonical bare Agent
   |-- npm package: agent/win-x64/
   |-- PyPI wheel: tdx_ths_linker/agent/win-x64/
   `-- registered Web installer -> Web download release
```

npm 与 PyPI 不安装、不注册，直接运行包内 Agent。Web 安装包会停止当前 Agent，将同一二进制放到 `%LOCALAPPDATA%\OnclawTdxThs`，注册 `onclaw-tdx-ths://` 并启动固定路径服务。此后 npm/PyPI 优先复用该安装版本。

## 安全边界

- Agent 只监听 `127.0.0.1`。
- 浏览器写入需要自定义协议激活后的精确 Origin 授权。
- 无 Origin 的本地 CLI/Node/Harness 请求按当前用户本地信任边界处理。
- 所有调用共享一个 target-global 调度器，不会创建互相竞争的原生 Runtime。
