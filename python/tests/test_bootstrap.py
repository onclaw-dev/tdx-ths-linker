import io
import json

from tdx_ths_linker.bootstrap import ensure_agent


class Response:
    def __enter__(self):
        return self

    def __exit__(self, *args):
        return None

    def read(self):
        return json.dumps({
            "ok": True,
            "protocolVersion": 1,
            "agentVersion": "test",
            "writeAuthorized": True,
            "writableTargets": ["tdx", "ths"],
        }).encode()


def test_bootstrap_reuses_healthy_agent_without_registry(monkeypatch, tmp_path):
    monkeypatch.setattr("sys.platform", "win32")
    monkeypatch.setattr("platform.machine", lambda: "AMD64")
    monkeypatch.setenv("LOCALAPPDATA", str(tmp_path))
    version = ensure_agent(
        _opener=lambda request, timeout: Response(),
    )
    assert version == "test"


def test_bootstrap_prefers_installed_web_agent(monkeypatch, tmp_path):
    monkeypatch.setattr("sys.platform", "win32")
    monkeypatch.setattr("platform.machine", lambda: "AMD64")
    monkeypatch.setenv("LOCALAPPDATA", str(tmp_path))
    installed = tmp_path / "OnclawTdxThs" / "onclaw-tdx-ths-agent.exe"
    installed.parent.mkdir()
    installed.write_bytes(b"installed")
    started = []

    def starter(path):
        started.append(path)

    def opener(request, timeout):
        if not started:
            raise OSError("not running")
        return Response()

    version = ensure_agent(
        _opener=opener,
        _starter=starter,
        _installed_path=installed,
    )
    assert version == "test"
    assert started == [installed]
