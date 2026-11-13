import assert from "node:assert/strict";
import { readdir, readFile } from "node:fs/promises";
import test from "node:test";

const read = (path) => readFile(new URL(`../${path}`, import.meta.url), "utf8");

test("Python example uses the Agent HTTP client without a native binding", async () => {
  const [source, readme, client] = await Promise.all([
    read("examples/python/basic.py"),
    read("examples/python/README.md"),
    read("python/src/tdx_ths_linker/client.py"),
  ]);
  assert.match(source, /from tdx_ths_linker import Client/);
  assert.match(readme, /tdx-ths-linker-python ensure-agent/);
  assert.match(readme, /裸 Agent/);
  assert.doesNotMatch(readme, /内嵌安装程序|setup-win-x64/);
  assert.match(client, /\/v1\/state/);
  assert.doesNotMatch(client, /ctypes|\.dll|_native/);
});

test("Electron example isolates Agent HTTP access in its main process", async () => {
  const [main, preload, html, manifest] = await Promise.all([
    read("examples/electron/main.mjs"),
    read("examples/electron/preload.cjs"),
    read("examples/electron/index.html"),
    read("examples/electron/package.json"),
  ]);
  assert.match(main, /createAgentClient/);
  assert.match(main, /ensureAgent/);
  assert.match(main, /tdx-ths-linker\/runtime/);
  assert.match(main, /createStockLinkRuntime/);
  assert.doesNotMatch(main, /\.node|createRequire/);
  assert.match(main, /contextIsolation: true/);
  assert.match(main, /nodeIntegration: false/);
  assert.match(main, /sandbox: true/);
  assert.match(preload, /contextBridge\.exposeInMainWorld/);
  assert.doesNotMatch(preload, /tdx-ths-linker\.node/);
  assert.doesNotMatch(html, /require\s*\(/);
  assert.equal(JSON.parse(manifest).dependencies["tdx-ths-linker"], "0.1.3");
});

test("DeepSeek Harness example uses a portable Skill and the npm JSON CLI", async () => {
  const [readme, prompts, skill, driver] = await Promise.all([
    read("examples/deepseek-harness/README.md"),
    read("examples/deepseek-harness/example-prompts.md"),
    read("skills/tdx-ths-linker/SKILL.md"),
    read("skills/tdx-ths-linker/references/driver-contract.md"),
  ]);
  assert.match(readme, /\.agents\\skills/);
  assert.match(readme, /npm install tdx-ths-linker/);
  assert.match(readme, /npm 安装不运行 Agent 安装程序/);
  assert.match(readme, /npx --no-install tdx-ths-linker --json --accept-license/);
  assert.match(readme, /--target tdx --wait/);
  assert.match(readme, /--target ths --wait/);
  assert.doesNotMatch(readme, /[A-Z]:\\Users\\/i);
  assert.doesNotMatch(prompts, /[A-Z]:\\Users\\/i);
  assert.match(skill, /npx --no-install tdx-ths-linker/);
  assert.match(driver, /npx --no-install tdx-ths-linker/);
  assert.doesNotMatch(`${skill}\n${driver}`, /[A-Z]:\\Users\\/i);
});

test("every example document uses the unified three-section structure", async () => {
  const documents = [
    "examples/web-quickstart/README.md",
    "examples/python/README.md",
    "examples/electron/README.md",
    "examples/deepseek-harness/README.md",
    "examples/deepseek-harness/example-prompts.md",
    "examples/skill/README.md",
  ];
  for (const document of documents) {
    const source = await read(document);
    const install = source.indexOf("## 安装指令");
    const testCommand = source.indexOf("## 测试指令");
    const result = source.indexOf("## 测试结果返回示例");
    assert.ok(install >= 0 && install < testCommand && testCommand < result, `${document} has an invalid section order`);
    assert.doesNotMatch(source, /[A-Z]:\\Users\\/i, `${document} contains a local user path`);
  }
});

test("examples do not contain native product binaries", async () => {
  const root = new URL("../examples/", import.meta.url);
  const pending = [root];
  const forbidden = [];
  while (pending.length) {
    const directory = pending.pop();
    for (const entry of await readdir(directory, { withFileTypes: true })) {
      const child = new URL(entry.name + (entry.isDirectory() ? "/" : ""), directory);
      if (entry.isDirectory()) pending.push(child);
      else if (/\.(?:dll|exe|node)$/i.test(entry.name)) forbidden.push(child.pathname);
    }
  }
  assert.deepEqual(forbidden, []);
});
