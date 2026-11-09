import argparse
import os
import subprocess
import sys
import tempfile
import venv
import zipfile
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate the tdx-ths-linker Windows wheel")
    parser.add_argument("wheel", type=Path)
    parser.add_argument("--skip-install", action="store_true")
    args = parser.parse_args()
    wheel = args.wheel.resolve()
    require(wheel.is_file(), f"Wheel does not exist: {wheel}")
    require(wheel.name.endswith("-py3-none-win_amd64.whl"), f"Unexpected wheel tag: {wheel.name}")

    with zipfile.ZipFile(wheel) as archive:
        names = set(archive.namelist())
        require("tdx_ths_linker/_native/onclaw-tdx-ths.dll" in names, "Wheel is missing native DLL")
        require("tdx_ths_linker/py.typed" in names, "Wheel is missing py.typed")
        metadata_name = next((name for name in names if name.endswith(".dist-info/METADATA")), None)
        require(metadata_name is not None, "Wheel is missing METADATA")
        metadata = archive.read(metadata_name).decode("utf-8")
        require("Name: tdx-ths-linker" in metadata, "Unexpected distribution name")
        require(
            "License-Expression: PolyForm-Noncommercial-1.0.0" in metadata,
            "Wheel is missing the SPDX license expression",
        )
        require(any(name.endswith(".dist-info/licenses/LICENSE.md") for name in names), "Missing LICENSE.md")
        require(
            any(name.endswith(".dist-info/licenses/COMMERCIAL-LICENSE.md") for name in names),
            "Missing COMMERCIAL-LICENSE.md",
        )

    if args.skip_install:
        return
    require(os.name == "nt", "Clean native-load validation requires Windows")
    with tempfile.TemporaryDirectory(prefix="tdx-ths-linker-wheel-") as directory:
        environment = Path(directory) / "venv"
        venv.EnvBuilder(with_pip=True).create(environment)
        python = environment / "Scripts" / "python.exe"
        subprocess.run(
            [str(python), "-m", "pip", "install", "--no-index", "--no-deps", str(wheel)],
            check=True,
        )
        subprocess.run(
            [
                str(python),
                "-c",
                "from tdx_ths_linker._native import NativeBinding; "
                "binding = NativeBinding(); assert binding.abi_version == 1",
            ],
            check=True,
        )


if __name__ == "__main__":
    main()
