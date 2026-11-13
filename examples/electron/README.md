# Electron Agent HTTP 示例

## 安装指令

```powershell
cd examples/electron
npm install
npm start
```

npm 包内嵌裸 Windows Agent；主进程使用 `ensureAgent` 复用或直接启动单实例服务，renderer 只通过 context-isolated preload API 调用。该流程不安装软件、不修改注册表。

## 测试指令

```powershell
npm run test:examples
```

示例不加载 `.node`，Electron、CLI 和网页请求进入同一个 Agent 调度器。

## 测试结果返回示例

```json
{
  "channel": "electron-agent-http",
  "agentReached": true,
  "tdxAvailable": true,
  "thsAvailable": true
}
```
