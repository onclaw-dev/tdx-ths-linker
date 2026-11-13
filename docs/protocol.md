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

### 状态与活跃度

状态包含 `active`、`lastActivityAt` 以及每个端点的 `updatedAt`/`lastScanAt`。初始化、读取状态和有效写入都会续期 runtime 活跃租约。超过空闲超时没有调用后，runtime 停止轮询并进入 `idle`；下一次状态读取或写入会自动唤醒，不需要重新初始化或重新领取 session。

## 安全约束

- `Host` 必须精确为 `127.0.0.1:17654`。
- 浏览器提供的 Origin 必须满足安全格式；未授权 Origin 可读取状态但不能写入。
- Origin-less 写入只适用于本机 CLI、Node、Electron 或 Harness 的 HTTP 调用；浏览器不能移除其自动生成的 Origin 头。
- CORS 只反射通过格式校验的 Origin，响应禁用缓存，请求头和正文存在大小限制。
- THS 写入使用进程内存命令路径，必须针对目标 THS 版本单独验证。

## 版本策略

当前协议版本为 `1`。客户端必须检查 `/v1/health` 的 `protocolVersion`。
