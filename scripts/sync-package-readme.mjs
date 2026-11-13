import { readFile, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const source = join(root, "README.md");
const target = join(root, "packages", "client", "README.md");
const rootReadme = await readFile(source, "utf8");
const packageReadme = await readFile(target, "utf8").catch(() => "");

if (packageReadme !== rootReadme) {
  await writeFile(target, rootReadme, "utf8");
  console.log("Synchronized packages/client/README.md from root README.md");
}
