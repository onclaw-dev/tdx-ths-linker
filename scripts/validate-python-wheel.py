import hashlib
import sys
import zipfile
from pathlib import Path

wheel = Path(sys.argv[1])
canonical = Path(sys.argv[2]) if len(sys.argv) > 2 else None
if not wheel.name.endswith("-py3-none-win_amd64.whl"):
    raise SystemExit(f"unexpected wheel tag: {wheel.name}")

agent_name = "tdx_ths_linker/agent/win-x64/onclaw-tdx-ths-agent.exe"
manifest_name = "tdx_ths_linker/agent/win-x64/SHA256SUMS.txt"
with zipfile.ZipFile(wheel) as archive:
    names = set(archive.namelist())
    if agent_name not in names or manifest_name not in names:
        raise SystemExit("wheel is missing canonical Agent assets")
    if any(name.lower().endswith("setup-win-x64.exe") for name in names):
        raise SystemExit("wheel must not contain the registered Web installer")
    forbidden = [name for name in names if name.lower().endswith((".dll", ".pyd", ".node"))]
    if forbidden:
        raise SystemExit(f"wheel contains obsolete native runtimes: {forbidden}")
    agent = archive.read(agent_name)
    manifest = archive.read(manifest_name).decode("ascii")
    metadata_name = next(name for name in names if name.endswith(".dist-info/METADATA"))
    metadata = archive.read(metadata_name).decode("utf-8").replace("\r\n", "\n")

metadata_body = metadata.partition("\n\n")[2].strip()
root_readme = Path("README.md").read_text(encoding="utf-8").replace("\r\n", "\n").strip()
if metadata_body != root_readme:
    raise SystemExit("PyPI wheel description must be identical to the root README.md")

expected = next(
    (line.split()[0] for line in manifest.splitlines() if line.split()[-1] == Path(agent_name).name),
    None,
)
actual = hashlib.sha256(agent).hexdigest()
if not expected or actual.lower() != expected.lower():
    raise SystemExit("wheel Agent checksum mismatch")
if canonical and hashlib.sha256(canonical.read_bytes()).hexdigest() != actual:
    raise SystemExit("wheel Agent differs from canonical bare release artifact")
print(f"validated {wheel.name}: {actual}; root README reused")
