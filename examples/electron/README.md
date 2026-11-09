# Electron 快速接入

## 安装指令

```powershell
cd examples/electron
npm install
```

将匹配 Electron 39.2.0、Windows x64 的 `tdx-ths-linker.node` 放到 `native/win-x64/tdx-ths-linker.node`，或用 `TDX_THS_LINKER_NODE_PATH` 指向本地编译文件。renderer 不直接加载原生模块。

## 测试指令

```powershell
npm start
```

在界面中依次：

1. 接受许可并初始化。
2. 读取状态，确认 TDX、THS 均可用。
3. 输入明确的六位测试代码，点击“设置 TDX”，再读取确认。
4. 点击“设置 THS”，再读取确认。
5. 最终读取，确认两端代码一致。

自动验收通过 Electron headless runner 加载同一 `.node` 并执行相同顺序。

## 测试结果返回示例

以下仅为返回格式示例：

```json
{
  "channel": "electron",
  "status": "passed",
  "reads": { "tdx": true, "ths": true },
  "writes": {
    "tdx": { "accepted": true, "confirmed": true },
    "ths": { "accepted": true, "confirmed": true }
  },
  "finalCodesMatch": true
}
```
