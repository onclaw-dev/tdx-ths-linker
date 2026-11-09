import assert from "node:assert/strict";
import test from "node:test";

import { loadNativeNode, packagedNativePath } from "../dist/native.mjs";

test("native loader rejects unsupported platforms before loading", () => {
  assert.throws(() => loadNativeNode({ platform: "linux", arch: "x64" }), /supports Windows x64/);
});

test("packaged native path is relative to the installed package", () => {
  assert.match(packagedNativePath(), /packages[\\/]client[\\/]native[\\/]win-x64[\\/]tdx-ths-linker\.node$/);
});
