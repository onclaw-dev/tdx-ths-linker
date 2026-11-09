# Python 快速接入

## 安装指令

```powershell
py -m pip install tdx-ths-linker==0.1.2
```

Python wheel 直接加载内置 DLL，不启动 Web Agent。

## 测试指令

```powershell
# 读取 TDX 与 THS
py examples/python/basic.py --accept-license

# 单独设置并确认 TDX
py examples/python/basic.py --accept-license --code <六位测试代码> --target tdx

# 单独设置并确认 THS
py examples/python/basic.py --accept-license --code <六位测试代码> --target ths

# 最终读取两端
py examples/python/basic.py --accept-license
```

省略 `--code` 时始终只读；`--code` 与 `--target` 必须同时提供。

## 测试结果返回示例

以下为返回结构示例，不代表当前机器已经执行：

```json
{
  "confirmed": true,
  "commands": {
    "tdx": { "accepted": true, "status": "accepted" }
  },
  "after": {
    "tdx": { "available": true, "code": "<六位测试代码>" },
    "ths": { "available": true, "code": "<当前代码>" }
  }
}
```
