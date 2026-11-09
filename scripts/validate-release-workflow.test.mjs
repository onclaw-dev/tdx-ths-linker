import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const workflow = await readFile(new URL("../.github/workflows/python-publish.yml", import.meta.url), "utf8");

test("release workflow is triggered only by v-prefixed tag pushes", () => {
  assert.match(workflow, /on:\s*\n\s+push:\s*\n\s+tags:\s*\["v\*"\]/);
  assert.doesNotMatch(workflow, /pull_request:|workflow_dispatch:|branches:/);
});

test("all release builds depend on version validation", () => {
  for (const job of ["build-node", "build-npm", "build-python", "build-agent", "build-skill"]) {
    assert.match(workflow, new RegExp(`\\n  ${job}:[\\s\\S]*?\\n    needs: validate-release`));
  }
  assert.match(workflow, /node scripts\/validate-release-version\.mjs "\$GITHUB_REF_NAME"/);
});

test("Agent release contains portable and installed distributions", () => {
  assert.match(workflow, /validate-agent-installer\.ps1/);
  assert.match(workflow, /onclaw-tdx-ths-agent-portable-win-x64\.zip/);
  assert.match(workflow, /onclaw-tdx-ths-setup-win-x64\.exe/);
  assert.match(workflow, /release\/agent\/SHA256SUMS\.txt/);
  assert.match(workflow, /path: dist\/agent-release-assets\/\*/);
});

test("registry releases precede GitHub Release publication", () => {
  assert.match(workflow, /npm publish dist\/npm\/\*\.tgz --access public --tag latest/);
  assert.match(
    workflow,
    /needs: \[build-node, build-agent, build-skill, publish-pypi, publish-npm\]/,
  );
  assert.match(workflow, /RELEASE_TAG: \$\{\{ github\.ref_name \}\}/);
});
