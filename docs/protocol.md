# 本地 Agent 协议 v1

Windows Agent 只监听 `127.0.0.1:17654`。健康检查和股票状态是无登录、无 session 的本机读取接口；浏览器写入代码前必须由用户通过自定义协议明确授权网页 Origin。

## 浏览器授权

网页必须在真实用户操作中打开：

```text
onclaw-tdx-ths://activate?v=1&origin=<网页Origin>
```

Agent 校验完整 Origin，并将其加入当前 Agent 进程的写入授权集合。授权不会按时间过期，也不会替换已经授权的其他 Origin；Agent 退出后集合清空。允许的浏览器 Origin 为 HTTPS，以及本地开发使用的 `http://localhost[:port]`、`http://127.0.0.1[:port]`。

客户端随后轮询 `GET /v1/health`，直到 `writeAuthorized` 为 `true`。当前协议不包含 nonce、session claim 或 bearer token。

## 接口

| 方法 | 路径 | 访问要求 | 用途 |
| --- | --- | --- | --- |
| `GET` | `/v1/health` | 无 | 查询协议、Agent 版本和当前 Origin 的写授权 |
| `GET` | `/v1/state` | 无 | 唤醒 runtime 并读取最新状态快照 |
| `GET` | `/v1/command-status?clientId=...&requestId=...` | 无 | 查询指定命令的派发与读回确认结果 |
| `POST` | `/v1/initialize` | 无 | 幂等初始化 runtime |
| `POST` | `/v1/set-code` | Origin-less 本机调用，或已授权浏览器 Origin | 提交六位股票代码 |

### 设置代码

```json
{
  "target": "tdx",
  "code": "600825",
  "clientId": "web-稳定调用方标识",
  "requestId": "request-单次请求标识"
}
```

`clientId` 和 `requestId` 只用于归属、幂等和调度，不是认证凭据。相同 `(clientId, requestId)` 的重复请求不会重复执行。

TDX 与 THS 各自使用 300 ms 首尾合并窗口：第一个代码立即进入执行槽，窗口内只保留最后一个不同代码，中间代码不执行。响应状态包括：

- `accepted`：首个命令已进入立即槽；
- `coalesced`：请求已成为当前窗口的最新尾值；
- `duplicate`：相同调用方和请求标识已经接收过。

上述状态只描述入队，不表示桌面软件已经切换。Agent 另行跟踪 `queued`、
`dispatching`、`dispatched`、`confirmed`、`failed`、`timed_out`、
`superseded`，并保留最近 256 条结果。派发后最多等待 10 秒，只有目标端读回等于
请求代码时才进入 `confirmed`。`/v1/state` 的 `lastCommand` 与命令查询结果包含
阶段、时间戳、错误信息和可用的 Win32 错误码；后续读取成功不会抹除命令失败。

### 状态与活跃度

状态包含兼容字段 `active`、`lastActivityAt`，以及新增的 `activityState`、
`stateGeneration`、`stale`、`refreshedAt`。初始化、读取状态、有效写入和待处理命令
属于业务活动；`/v1/health` 和无效请求不会续期。业务活动停止后 runtime 先进入
`warm` 并停止全部目标读取，满 5 分钟进入 `cool` 并关闭进程句柄，满 10 分钟进入
`cold` 并清空进程解析缓存。下一次状态读取或合法写入会自动唤醒。

状态唤醒最多等待 500 ms 获取新的 `stateGeneration`；未能及时刷新时仍可返回快照，
但必须标记 `stale=true`。`lastScanAt` 只在实际解析偏移时更新。单次读取失败只把端点
标记为暂时退化；连续三次或约一秒失败后才清除偏移。整体 `ready` 要求 TDX/THS
两端均可靠，单端可用为 `degraded`，两端均不可用为 `unavailable`。

## 安全约束

- `Host` 必须精确为 `127.0.0.1:17654`。
- 浏览器提供的 Origin 必须满足安全格式；未授权 Origin 可读取状态但不能写入。
- Origin-less 写入只适用于本机 CLI、Node、Electron 或 Harness 的 HTTP 调用；浏览器不能移除其自动生成的 Origin 头。
- CORS 只反射通过格式校验的 Origin，响应禁用缓存，请求头和正文存在大小限制。
- THS 写入使用进程内存命令路径，必须针对目标 THS 版本单独验证。

当前 `1168` 消息兼容路径已在 THS 9.40.10.88 上完成真实切换与恢复验证。窗口选择
只接受目标 PID 下唯一的可见、无 owner、非 toolwindow 且具有类名和标题的顶层窗口；
零个或多个候选都会明确失败，不会退回到“第一个可见窗口”。

## 版本策略

当前协议版本为 `1`。客户端必须检查 `/v1/health` 的 `protocolVersion`。
