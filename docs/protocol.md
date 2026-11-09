# 本地 Agent 协议 v1

Windows Agent 监听 `127.0.0.1:17654`，不会绑定公网接口。浏览器必须先通过自定义协议完成用户触发的本地配对，才能访问状态与写入接口。

## 配对流程

1. 浏览器生成一个密码学安全的 32 字节随机 nonce。
2. 用户手势打开 `onclaw-tdx-ths://activate?v=1&nonce=<nonce>&origin=<origin>`。
3. Agent 将待处理 nonce 与完整 Origin 绑定。
4. 浏览器调用 `POST /v1/session/claim` 并提交 nonce。
5. Agent 消耗该 nonce，返回有效期 10 分钟的 bearer token。

允许的 Origin：

- HTTPS Origin。
- 用于本地开发的 `http://localhost[:port]`。
- 用于本地开发的 `http://127.0.0.1[:port]`。

每个业务请求必须携带配对时的精确 `Origin`，并使用：

```http
Authorization: Bearer <session-token>
```

nonce 只能消费一次；新的激活请求会替换尚未领取的配对信息。

## 接口

| 方法 | 路径 | 会话要求 | 用途 |
| --- | --- | --- | --- |
| `GET` | `/v1/health` | 无 | 查询协议版本、Agent 版本和可写目标 |
| `POST` | `/v1/session/claim` | 配对 nonce | 创建本机会话 |
| `POST` | `/v1/initialize` | bearer token | 接受 GPLv3 条款并初始化 runtime |
| `GET` | `/v1/state` | bearer token | 读取 TDX 与 THS 状态 |
| `POST` | `/v1/set-code` | bearer token | 提交六位股票代码 |

### 初始化

```json
{
  "licenseAccepted": true
}
```

### 设置代码

```json
{
  "target": "tdx",
  "code": "600825"
}
```

`target` 只能是 `tdx` 或 `ths`，`code` 必须是六位 ASCII 数字。响应中的 `accepted` 表示命令已进入处理流程，不代表目标软件已完成切换；调用方应继续读取 `/v1/state` 进行确认。

## 安全约束

- `Host` 必须精确为 `127.0.0.1:17654`。
- Agent 拒绝不受支持的 Origin、重复 nonce、过期 token 和未授权业务请求。
- 响应禁用缓存，并按已配对 Origin 返回 CORS 头。
- 请求头与正文存在大小限制，激活 URI 也有长度限制。
- THS 写入使用进程内存命令路径，必须针对目标 THS 版本单独验证。

## 版本策略

当前协议版本为 `1`。客户端必须检查 `/v1/health` 返回的 `protocolVersion`；不匹配时不得继续初始化或写入。
