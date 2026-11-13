# 开发、测试与发布

## 环境

- Windows x64
- Visual Studio 2022 Build Tools 与 CMake 3.20+
- Node.js 22 与 npm
- Python 3.9+
- 构建安装程序时需要 NSIS

## 客户端与测试

```powershell
npm install
npm run build:client
npm run test:client
npm run test:examples
```

客户端只通过 Agent HTTP 工作，不再构建 `.node`、C ABI DLL 或 Python 原生扩展。

Python 测试与 wheel：

```powershell
$env:PYTHONPATH = "python/src"
py -m pytest python/tests
py -m build --wheel
py scripts/validate-python-wheel.py dist/tdx_ths_linker-*-py3-none-win_amd64.whl release/agent/onclaw-tdx-ths-agent.exe
```

## Agent 安装程序

开发构建：

```powershell
$env:ONCLAW_TDX_THS_RELEASE_UNSIGNED = "1"
npm run release:agent
```

输出：

```text
release/agent/onclaw-tdx-ths-agent.exe
release/agent/onclaw-tdx-ths-setup-win-x64.exe
release/agent/SHA256SUMS.txt
```

正式发布时应配置签名工具、证书与密码。安装程序将 Agent 安装到用户目录、注册自定义协议并写入卸载信息。

## npm 与 PyPI 内嵌制品

构建发布包前，将同一次 Agent 构建产生的裸 Agent 和校验清单复制到：

```text
packages/client/agent/win-x64/onclaw-tdx-ths-agent.exe
packages/client/agent/win-x64/SHA256SUMS.txt
python/src/tdx_ths_linker/agent/win-x64/onclaw-tdx-ths-agent.exe
python/src/tdx_ths_linker/agent/win-x64/SHA256SUMS.txt
```

`npm pack` 后运行 `npm run test:package`，确认裸 Agent 存在、安装程序不存在且旧原生运行时文件不存在。

## 发布

`v*` tag 触发工作流：

1. 校验 npm、lockfile、CMake、Agent 与 CLI 元数据。
2. 在 Windows 构建唯一裸 Agent和包含它的注册安装程序。
3. 把裸 Agent 注入 npm 与 PyPI wheel，并执行客户端、示例和包内容测试。
4. 发布 npm 与 PyPI。
5. 只将带注册能力的安装包、校验清单和 Web 示例 HTML 部署到网页下载 Release。

云端步骤不重新构建 Agent；npm、PyPI 与 Web 安装包包含同一次构建的 Agent 字节。
