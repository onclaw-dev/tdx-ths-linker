import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const workflow = await readFile(new URL("../.github/workflows/python-publish.yml", import.meta.url), "utf8");
const installerScript = await readFile(new URL("../setup/onclaw-tdx-ths-setup.nsi", import.meta.url), "utf8");
const npmPackage = await readFile(new URL("../packages/client/package.json", import.meta.url), "utf8");

test("release workflow is triggered only by v-prefixed tag pushes", () => {
  assert.match(workflow, /on:\s*\n\s+push:\s*\n\s+tags:\s*\["v\*"\]/);
  assert.doesNotMatch(workflow, /pull_request:|workflow_dispatch:|branches:/);
});

test("one canonical bare Agent feeds npm, PyPI, and the registered Web installer", () => {
  assert.match(workflow, /build-agent:[\s\S]*?name: canonical-agent-release/);
  assert.match(workflow, /release\/agent\/onclaw-tdx-ths-agent\.exe/);
  assert.match(workflow, /build-npm:[\s\S]*?needs: \[validate-release, build-agent\]/);
  assert.match(workflow, /Inject canonical bare Agent[\s\S]*?packages\/client\/agent\/win-x64/);
  assert.match(workflow, /build-python:[\s\S]*?needs: \[validate-release, build-agent\]/);
  assert.match(workflow, /Inject canonical bare Agent[\s\S]*?python\/src\/tdx_ths_linker\/agent\/win-x64/);
  assert.match(workflow, /publish-cloud:[\s\S]*?name: canonical-agent-release/);
  assert.match(workflow, /rm release-assets\/onclaw-tdx-ths-agent\.exe/);
  assert.match(workflow, /onclaw-tdx-ths-setup-win-x64\.exe/);
  assert.match(workflow, /onclaw-tdx-ths-web-example\.html/);
  assert.match(workflow, /release\/agent\/SHA256SUMS\.txt/);
  assert.doesNotMatch(workflow, /skill-release|build-skill:/);
});

test("obsolete in-process native release channels are absent", () => {
  assert.doesNotMatch(workflow, /build-node:|tdx-ths-linker\.node|\.dll|ONCLAW_BUILD_CAPI/);
  assert.match(workflow, /npm run test:package/);
  assert.match(workflow, /scripts\/validate-python-wheel\.py/);
  assert.match(workflow, /pypa\/gh-action-pypi-publish/);
  assert.doesNotMatch(npmPackage, /postinstall|onclaw-tdx-ths-setup-win-x64\.exe/);
});

test("release builds but never installs or uninstalls the registered Agent", () => {
  assert.doesNotMatch(workflow, /validate-agent-installer\.ps1|Start-Process|--verify-registration|--unregister/);
  assert.match(installerScript, /OnclawTdxThs\.transition\.lock/);
  assert.match(installerScript, /--shutdown/);
  assert.match(installerScript, /--register/);
  assert.match(installerScript, /--verify-health/);
  assert.match(installerScript, /Exec '\"\$INSTDIR\\onclaw-tdx-ths-agent\.exe\"'/);
});

test("npm and PyPI publication precede cloud deployment", () => {
  assert.match(workflow, /npm publish dist\/npm\/\*\.tgz --access public --tag latest/);
  assert.match(workflow, /publish-cloud:[\s\S]*?needs: \[build-agent, publish-npm, publish-pypi\]/);
  assert.match(workflow, /gh release (?:upload|create)/);
});
