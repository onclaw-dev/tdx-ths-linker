import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const html = await readFile(new URL("../examples/web-quickstart/index.html", import.meta.url), "utf8");
const readme = await readFile(new URL("../examples/web-quickstart/README.md", import.meta.url), "utf8");

test("quickstart uses a pinned npm ESM build and high-level APIs", () => {
  assert.match(html, /cdn\.jsdelivr\.net\/npm\/tdx-ths-linker@0\.1\.2\/dist\/index\.mjs/);
  assert.match(html, /createWebLinker/);
  assert.match(html, /createStockLinkRuntime/);
});

test("quickstart demonstrates consent, recovery, manual writes, and link direction", () => {
  assert.match(html, /type="checkbox"/);
  assert.match(html, /installer_required/);
  assert.match(html, /id="set-tdx"/);
  assert.match(html, /id="set-ths"/);
  assert.match(html, /setAndConfirm\("tdx"\)/);
  assert.match(html, /setAndConfirm\("ths"\)/);
  assert.doesNotMatch(html, /Promise\.all\(\[web\.client\.setCode/);
  assert.match(html, /ths-to-tdx/);
  assert.match(html, /tdx-to-ths/);
});

test("quickstart documents localhost and production npm usage", () => {
  assert.match(readme, /http:\/\/localhost:8080/);
  assert.match(readme, /file:\/\//);
  assert.match(readme, /npm install tdx-ths-linker/);
  assert.match(readme, /生产项目/);
});
