# Web 快速接入

Web 接入不需要登录、后端票据或短期 session。npm 包负责 Windows/Origin 检查、自定义协议写授权、loopback 重试、初始化和安装恢复；本机 Agent 负责与 TDX、THS 交互。

## 生产接入

```bash
npm install tdx-ths-linker
```

```ts
import { createStockLinkRuntime, createWebLinker } from "tdx-ths-linker";

const web = createWebLinker({
  // 可选：固定版本地址、镜像或企业分发地址。
  // installerUrl: "https://example.com/onclaw-tdx-ths-setup-win-x64.exe",
});

connectButton.addEventListener("click", () => {
  // connect() 不是 async 函数包装：它会在首次异步边界前同步唤起协议。
  const pending = web.connect({ licenseAccepted: licenseCheckbox.checked });
  pending.then(async (result) => {
    if (result.status === "installer_required") {
      installerLink.href = result.installerUrl;
      installerLink.hidden = false;
      return;
    }
    if (!result.ok) throw new Error(result.message);

    const runtime = createStockLinkRuntime({
      client: web.client,
      mode: "ths-to-tdx",
      onState: console.log,
      onError: console.error,
    });
    runtime.start();
  });
});
```

必须从 HTTPS 或 `http://localhost` 页面发起，并把 `connect()` 直接放在用户点击处理函数中。浏览器没有可靠的“协议是否已安装”查询接口，因此连接失败只能表示当前无法访问 Agent；UI 应提供安装器链接和安装后的重试按钮。

协议激活会授予当前网页 Origin 在 Agent 进程生命周期内的写权限，不创建易失 session。`health` 和 `state` 可直接读取；浏览器 `set-code` 必须来自已激活 Origin。客户端自动为每个实例和写请求生成 `clientId`/`requestId`。

## 自动联动语义

`createStockLinkRuntime()` 支持：

- `off`：关闭轮询；
- `read-only`：只读取并上报状态；
- `ths-to-tdx`：仅把 THS 后续变化转发给 TDX；
- `tdx-to-ths`：仅把 TDX 后续变化转发给 THS。

开启方向、切换方向或来源软件恢复后的第一次有效读数只建立基线，不写入目标端。只有随后发生的代码变化才会转发；目标端已经是相同代码时也不会重复写入。项目不默认提供双向联动，避免反馈循环和启动覆盖。

Agent 对每个目标执行 300 ms 首尾合并：来自网页、CLI、Node、Electron 或 Harness 的突发写入进入同一个全局调度器，立即处理第一个，并只保留窗口内最后一个不同代码。持续状态轮询会续期 Agent runtime 的活跃租约；所有调用停止后 runtime 自动进入空闲，下一次读取或写入自动唤醒。

## 一文件示例

[`examples/web-quickstart/index.html`](../examples/web-quickstart/index.html) 使用固定版本的 jsDelivr ESM 地址，适合快速验证：

```powershell
py -m http.server 8080 --directory examples/web-quickstart
```

打开 `http://localhost:8080`。CDN 方式只用于快速示例；生产项目应通过 npm 锁定依赖并由 bundler 构建。

## 安装器与权限恢复

默认安装地址指向 GitHub Release 的 `latest/download/onclaw-tdx-ths-setup-win-x64.exe`，可通过 `installerUrl` 覆盖。正式 Release 不再提供独立便携 EXE/ZIP，安装器负责固定路径、协议注册、修复和卸载。

Chromium 从 HTTPS 访问 loopback 时可能请求本地网络权限。客户端请求会声明 `targetAddressSpace: "loopback"`；若浏览器明确报告拒绝，连接结果为 `local_network_denied`，宿主应提示用户恢复站点的本地网络权限。
