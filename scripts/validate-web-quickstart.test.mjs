import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import test from "node:test";

const html = await readFile(new URL("../examples/web-quickstart/index.html", import.meta.url), "utf8");
const readme = await readFile(new URL("../examples/web-quickstart/README.md", import.meta.url), "utf8");
const projectReadme = await readFile(new URL("../README.md", import.meta.url), "utf8");

test("quickstart uses a pinned npm ESM build and high-level APIs", () => {
  assert.match(html, /cdn\.jsdelivr\.net\/npm\/tdx-ths-linker@0\.1\.4\/dist\/index\.mjs/);
  assert.match(html, /createWebLinker/);
  assert.match(html, /createStockLinkRuntime/);
  assert.match(html, /https:\/\/fin\.onclaw\.cc\/linker\/onclaw-tdx-ths-setup-win-x64\.exe/);
  assert.match(html, /createWebLinker\(\{ installerUrl: INSTALLER_URL \}\)/);
});

test("quickstart reuses installer consent and demonstrates recovery, writes, and link direction", () => {
  assert.doesNotMatch(html, /id="license"/);
  assert.match(html, /web\.connect\(\{ licenseAccepted: true \}\)/);
  assert.match(html, /https:\/\/github\.com\/onclaw-dev\/tdx-ths-linker/);
  assert.match(html, /开源不易[^<]*Star/);
  assert.match(html, /installer_required/);
  assert.match(html, /id="installer"[^>]*href="https:\/\/fin\.onclaw\.cc\/linker\//);
  assert.doesNotMatch(html, /id="installer"[^>]*hidden/);
  assert.match(html, /已安装，唤醒并连接/);
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
  assert.match(readme, /fin\.onclaw\.cc\/linker\/onclaw-tdx-ths-setup-win-x64\.exe/);
  assert.match(readme, /生产项目/);
});

test("project and Web docs use relocated image assets and link the preview", async () => {
  assert.match(projectReadme, /imgs\/icon\.png/);
  assert.match(projectReadme, /imgs\/preview6\.png/);
  assert.match(projectReadme, /examples\/web-quickstart\/README\.md/);
  assert.match(projectReadme, /Web、Node \/ Electron、Python、CLI、Harness/);
  assert.match(readme, /\.\.\/\.\.\/imgs\/icon\.png/);
  for (let index = 1; index <= 6; index += 1) {
    assert.match(readme, new RegExp(`\\.\\.\\/\\.\\.\\/imgs\\/preview${index}\\.png`));
    assert.match(readme, new RegExp(`<a href="\\.\\.\\/\\.\\.\\/imgs\\/preview${index}\\.png">`));
  }
  for (const anchor of ["download", "smartscreen", "install", "activate", "connected", "result"]) {
    assert.match(readme, new RegExp(`href="#flow-${anchor}"`));
    assert.match(readme, new RegExp(`<a id="flow-${anchor}"></a>`));
  }
  assert.match(readme, /onclaw-tdx-ths:\/\/activate\?v=1&origin=/);
  assert.match(readme, /http:\/\/127\.0\.0\.1:17654/);
  assert.match(readme, /--verify-registration/);
  await Promise.all([
    access(new URL("../imgs/icon.png", import.meta.url)),
    access(new URL("../imgs/icon.ico", import.meta.url)),
    ...Array.from({ length: 6 }, (_, index) =>
      access(new URL(`../imgs/preview${index + 1}.png`, import.meta.url))),
  ]);
});

test("Web docs specify the complete Python requests and JSON protocol flow", () => {
  assert.match(readme, /import requests/);
  assert.match(readme, /webbrowser\.open\(activation_url\)/);
  assert.match(readme, /requests\.post\(/);
  assert.match(readme, /"writeAuthorized": true/);
  assert.match(readme, /"licenseAccepted": true/);
  assert.match(readme, /"target": "tdx"/);
  assert.match(readme, /"code": "600825"/);
  assert.match(readme, /"clientId":/);
  assert.match(readme, /"requestId":/);
  assert.match(readme, /origin_not_authorized/);
  assert.match(readme, /accepted: true.*调度器接受/s);
});
