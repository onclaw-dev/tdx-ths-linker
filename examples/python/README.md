# Python Agent HTTP 示例

## 安装指令

```powershell
py -m pip install tdx-ths-linker
tdx-ths-linker-python ensure-agent
```

PyPI wheel 内嵌与 npm 相同的裸 Agent；首次客户端或 CLI 调用会复用 Web 安装版或直接启动包内 Agent，不执行安装程序。

## 测试指令

```powershell
python basic.py --accept-license
python basic.py --accept-license --code <六位测试代码> --target tdx
python basic.py --accept-license --code <六位测试代码> --target ths
```

## 测试结果返回示例

```json
{
  "transport": "agent-http",
  "tdxAvailable": true,
  "thsAvailable": true
}
```
