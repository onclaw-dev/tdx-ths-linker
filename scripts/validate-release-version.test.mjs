import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  releaseVersionFromTag,
  validateReleaseVersion,
  validateVersionEntries,
} from "./validate-release-version.mjs";

const entries = (version) => [
  { name: "npm", version },
  { name: "pypi", version },
  { name: "cmake", version },
];

test("accepts a matching v-prefixed semantic version", () => {
  assert.equal(releaseVersionFromTag("v0.1.1"), "0.1.1");
  assert.equal(validateVersionEntries("v0.1.1", entries("0.1.1")), "0.1.1");
});

test("rejects a missing or malformed tag", () => {
  assert.throws(() => releaseVersionFromTag(), /received <empty>/);
  assert.throws(() => releaseVersionFromTag("0.1.1"), /must match v<semver>/);
});

test("reports every mismatched metadata source", () => {
  assert.throws(
    () =>
      validateVersionEntries("v0.1.2", [
        { name: "npm", version: "0.1.1" },
        { name: "pypi", version: "0.1.2" },
        { name: "cmake", version: "0.1.0" },
      ]),
    /npm=0\.1\.1, cmake=0\.1\.0/,
  );
});

test("rejects missing metadata values", () => {
  assert.throws(
    () => validateVersionEntries("v0.1.1", [{ name: "lockfile", version: undefined }]),
    /Missing release versions: lockfile/,
  );
});

test("validates the repository metadata for the current release", async () => {
  const rootPackage = JSON.parse(
    await readFile(new URL("../package.json", import.meta.url), "utf8"),
  );
  assert.equal(await validateReleaseVersion(`v${rootPackage.version}`), rootPackage.version);
});
