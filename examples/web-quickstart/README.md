# Web 快速接入

## 安装指令

安装 GitHub Release 中的 `onclaw-tdx-ths-setup-win-x64.exe`，然后安装 npm 依赖。HTML 单文件示例使用固定版本 CDN，生产项目应使用 bundler。

```powershell
npm install tdx-ths-linker
py -m http.server 8080 --directory examples/web-quickstart
```

从 Windows 浏览器打开 `http://localhost:8080`，不能直接使用 `file://`。

## 测试指令

1. 勾选 GPL-3.0-only 许可并点击“唤醒并连接本机联动服务”。
2. 读取状态，确认 TDX、THS 均显示 `available: true`。
3. 输入明确的六位测试代码，单独设置 TDX 并再次读取确认。
4. 使用相同代码单独设置 THS 并再次读取确认。
5. 最终读取，确认两端代码一致。

真实验收使用本地编译 Agent 时，设置 `ONCLAW_LIVE_TEST=1`、`ONCLAW_LIVE_CODE=<六位代码>` 和 `TDX_THS_LINKER_LICENSE_ACCEPTED=1` 后运行仓库的 live validation。

## 测试结果返回示例

以下仅表示稳定返回格式；只有 live validation 实际执行成功后才能标记为真实通过：

```json
{
  "channel": "web-agent",
  "status": "passed",
  "reads": { "tdx": true, "ths": true },
  "writes": {
    "tdx": { "accepted": true, "confirmed": true },
    "ths": { "accepted": true, "confirmed": true }
  },
  "finalCodesMatch": true
}
```
