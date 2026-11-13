import hashlib
import json
import os
import platform
import subprocess
import sys
import time
from importlib import resources
from pathlib import Path
from typing import Callable, Optional
from urllib.request import Request, urlopen

from .exceptions import AgentLaunchError, UnsupportedPlatformError

AGENT_NAME = "onclaw-tdx-ths-agent.exe"
PROTOCOL_VERSION = 1


def _health(base_url: str, opener: Callable = urlopen) -> Optional[str]:
    try:
        request = Request(f"{base_url}/v1/health", headers={"Accept": "application/json"})
        with opener(request, timeout=1.0) as response:
            value = json.loads(response.read().decode("utf-8"))
        if (
            value.get("ok") is True
            and value.get("protocolVersion") == PROTOCOL_VERSION
            and isinstance(value.get("agentVersion"), str)
        ):
            return value["agentVersion"]
    except Exception:
        return None
    return None


def _installed_agent() -> Optional[Path]:
    try:
        import winreg

        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Uninstall\OnclawTdxThs",
        )
        location, _ = winreg.QueryValueEx(key, "InstallLocation")
        return Path(location) / AGENT_NAME
    except Exception:
        return None


def _start(agent: Path) -> None:
    subprocess.Popen(
        [str(agent)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)
        | getattr(subprocess, "DETACHED_PROCESS", 0),
        close_fds=True,
    )


def _wait(base_url: str, timeout: float, opener: Callable) -> Optional[str]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        version = _health(base_url, opener)
        if version:
            return version
        time.sleep(0.2)
    return None


def _verify_packaged_agent(agent: Path, manifest: Path) -> None:
    entries = manifest.read_text(encoding="ascii").splitlines()
    expected = next(
        (line.split()[0] for line in entries if line.split()[-1] == AGENT_NAME),
        None,
    )
    if not expected or len(expected) != 64:
        raise AgentLaunchError("Agent checksum is missing")
    actual = hashlib.sha256(agent.read_bytes()).hexdigest()
    if actual.lower() != expected.lower():
        raise AgentLaunchError("Agent checksum mismatch")


def ensure_agent(
    *,
    base_url: str = "http://127.0.0.1:17654",
    timeout: float = 10.0,
    _opener: Callable = urlopen,
    _starter: Callable[[Path], None] = _start,
    _installed_path: Optional[Path] = None,
) -> str:
    if sys.platform != "win32" or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise UnsupportedPlatformError("Agent runtime supports Windows x64 only")
    version = _health(base_url, _opener)
    if version:
        return version

    local = os.environ.get("LOCALAPPDATA")
    if not local:
        raise AgentLaunchError("LOCALAPPDATA is unavailable")
    local_app_data = Path(local)
    installed_agent = _installed_path if _installed_path is not None else _installed_agent()
    lock = local_app_data / "OnclawTdxThs.transition.lock"
    deadline = time.monotonic() + timeout
    descriptor = None
    while descriptor is None:
        try:
            descriptor = os.open(str(lock), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(descriptor, str(os.getpid()).encode("ascii"))
        except FileExistsError:
            if time.monotonic() >= deadline:
                raise AgentLaunchError("Timed out waiting for Agent transition lock")
            time.sleep(0.2)

    try:
        version = _health(base_url, _opener)
        if version:
            return version

        if installed_agent is not None and installed_agent.is_file():
            agent = installed_agent
            _starter(agent)
            version = _wait(base_url, timeout, _opener)
            if not version:
                raise AgentLaunchError("Installed Agent did not become healthy")
            return version

        package_root = resources.files("tdx_ths_linker").joinpath("agent").joinpath("win-x64")
        with resources.as_file(package_root.joinpath(AGENT_NAME)) as packaged, resources.as_file(
            package_root.joinpath("SHA256SUMS.txt")
        ) as manifest:
            _verify_packaged_agent(packaged, manifest)
            _starter(Path(packaged))
            version = _wait(base_url, timeout, _opener)
            if not version:
                raise AgentLaunchError("Packaged Agent did not become healthy")
            return version
    finally:
        if descriptor is not None:
            os.close(descriptor)
        try:
            lock.unlink()
        except FileNotFoundError:
            pass
