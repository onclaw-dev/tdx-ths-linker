# 开发、测试与发布

## 环境要求

- Windows x64
- Visual Studio 2022 Build Tools 与 MSVC
- CMake 3.20 或更高版本
- Node.js 22 与 npm
- Python 3.9 或更高版本
- 构建安装器时需要 NSIS

## 安装依赖

```powershell
npm install
py -m pip install --upgrade build pytest twine "packaging>=24.2"
```

## npm 客户端

```powershell
npm run build:client
npm run test:client
```

## Electron `.node`

```powershell
$env:ONCLAW_TDX_THS_ELECTRON_VERSION = "<目标 Electron 版本>"
npm run build:native
```

输出：

```text
dist/native/win-x64/tdx-ths-linker.node
```

## Python wheel

```powershell
py -m pytest python/tests
py -m build --wheel
py -m twine check dist/*.whl
py scripts/validate-python-wheel.py dist/tdx_ths_linker-*-py3-none-win_amd64.whl
```

wheel 校验包括文件清单、GPL 元数据、干净虚拟环境安装和原生 DLL ABI 加载。

CLI 合约测试：

```powershell
$env:PYTHONPATH = "python/src"
py -m pytest python/tests/test_cli.py
py -m tdx_ths_linker --help
```

真实桌面测试会读写正在运行的 TDX 与 THS，必须显式开启：

```powershell
$env:ONCLAW_LIVE_TEST = "1"
$env:ONCLAW_LIVE_CODE = "600825"
py -m pytest python/tests/test_live.py
```

## Codex Skill

验证并生成 Skill ZIP：

```powershell
py scripts/package-skill.py --check-only
py scripts/package-skill.py
```

发布文件输出到：

```text
dist/skills/tdx-ths-linker-skill-<version>.zip
```

仓库自带的打包器会拒绝 DLL、EXE、`.node`、证书、私钥、`.env` 和环境相关绝对路径。

## Agent 与安装器

开发用未签名构建：

```powershell
$env:ONCLAW_TDX_THS_RELEASE_UNSIGNED = "1"
npm run release:agent
```

正式发布必须设置：

```text
ONCLAW_TDX_THS_SIGNTOOL
ONCLAW_TDX_THS_SIGN_CERTIFICATE
ONCLAW_TDX_THS_SIGN_PASSWORD
```

发布脚本生成 Agent、NSIS 安装器和 `SHA256SUMS.txt`。Agent 与安装器都使用根目录 `icon.ico`。

## GitHub Actions

[`.github/workflows/python-publish.yml`](../.github/workflows/python-publish.yml) 构建五类产物：

1. Electron `.node` 包。
2. PyPI Windows wheel。
3. npm tarball。
4. Agent EXE 包。
5. Codex Skill ZIP。

标签发布前需配置：

- GitHub environments：`npm`、`pypi`。
- npm Trusted Publisher，workflow 文件名为 `python-publish.yml`。
- PyPI Trusted Publisher，environment 为 `pypi`。
- 可选仓库变量 `ONCLAW_TDX_THS_ELECTRON_VERSION`。

## 发布前检查

- C ABI、Python 与 npm 测试全部通过。
- TDX/THS 真实双端读取和写入测试通过。
- 目标 Electron 版本能够加载 `.node`。
- wheel 能在干净环境加载内置 DLL。
- EXE、DLL 与 `.node` 已完成签名和哈希记录。
- Agent 配对、CORS、会话过期与单实例行为通过验证。
