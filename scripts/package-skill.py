import argparse
import re
import tomllib
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SKILL = ROOT / "skills" / "tdx-ths-linker"
PROHIBITED_SUFFIXES = {".dll", ".exe", ".node", ".pfx", ".pem", ".key"}
REQUIRED = {
    "SKILL.md",
    "agents/openai.yaml",
    "references/driver-contract.md",
}


def validate() -> list[Path]:
    files = sorted(path for path in SKILL.rglob("*") if path.is_file())
    relative = {path.relative_to(SKILL).as_posix() for path in files}
    missing = REQUIRED - relative
    if missing:
        raise SystemExit(f"Skill is missing required files: {', '.join(sorted(missing))}")
    skill_text = (SKILL / "SKILL.md").read_text(encoding="utf-8")
    if not skill_text.startswith("---\nname: tdx-ths-linker\n"):
        raise SystemExit("SKILL.md has invalid frontmatter or name")
    if "[TODO" in skill_text or not re.search(r"^description: .+", skill_text, re.MULTILINE):
        raise SystemExit("SKILL.md description is incomplete")
    for path in files:
        if path.suffix.lower() in PROHIBITED_SUFFIXES or path.name.lower() == ".env":
            raise SystemExit(f"Skill contains prohibited file: {path.relative_to(SKILL)}")
        if path.suffix.lower() in {".md", ".yaml", ".yml", ".txt"}:
            text = path.read_text(encoding="utf-8")
            if re.search(r"[A-Za-z]:\\(?:Users|WorkSpace)\\", text, re.IGNORECASE):
                raise SystemExit(f"Skill contains an environment-specific path: {path.relative_to(SKILL)}")
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate and package the tdx-ths-linker Codex Skill")
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    files = validate()
    if args.check_only:
        print(f"Skill validation passed ({len(files)} files)")
        return
    version = tomllib.loads((ROOT / "pyproject.toml").read_text(encoding="utf-8"))["project"]["version"]
    output_dir = ROOT / "dist" / "skills"
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f"tdx-ths-linker-skill-{version}.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in files:
            name = Path("tdx-ths-linker") / path.relative_to(SKILL)
            info = zipfile.ZipInfo(name.as_posix(), date_time=(2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            bundle.writestr(info, path.read_bytes())
    print(archive)


if __name__ == "__main__":
    main()
