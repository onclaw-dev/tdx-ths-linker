# Harness 提示词示例

## 安装指令

按照同目录 README 安装 npm 包与 Skill ZIP。npm 会确保统一的本地 Agent 已安装。

## 测试指令

```text
请加载 tdx-ths-linker skill。执行只读状态检查，分别报告 TDX、THS 是否可用及当前代码。我已阅读并接受 GPL-3.0-only 许可。
```

```text
请加载 tdx-ths-linker skill。先读取两端，再依次把 TDX 和 THS 设置为我明确提供的六位测试代码，每端都等待确认，最后重新读取两端。我已阅读并接受 GPL-3.0-only 许可。
```

## 测试结果返回示例

成功回答必须列出初始读取、TDX 写入确认、THS 写入确认和最终读取，不能只复述命令已接受。
