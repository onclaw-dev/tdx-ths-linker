from tdx_ths_linker import Client


class FakeTransport:
    def __init__(self):
        self.calls = []

    def request(self, path, method="GET", body=None):
        self.calls.append((path, method, body))
        if path == "/v1/initialize":
            return {"ok": True, "licenseState": "accepted", "runtimeHealth": "ready", "message": "ready"}
        if path == "/v1/state":
            endpoint = {
                "name": "tdx", "code": "600000", "health": "ready", "lastError": "",
                "updatedAt": 1, "lastScanAt": 1, "lastResolvedOffset": 0, "available": True,
                "usingFallback": False, "directRead": True, "pid": 1,
            }
            return {
                "initialized": True, "isSupported": True, "active": True,
                "lastActivityAt": 1, "runtimeHealth": "ready", "licenseState": "accepted",
                "message": "ready", "tdx": endpoint, "ths": {**endpoint, "name": "ths"},
                "activityState": "hot", "stateGeneration": 1, "stale": False,
                "refreshedAt": 1, "lastCommand": None,
            }
        if path.startswith("/v1/command-status?"):
            return {"target": "tdx", "code": "600825", "clientId": "python-test",
                    "requestId": "request-confirmed", "state": "confirmed",
                    "errorStage": "", "message": "confirmed", "queuedAt": 1,
                    "dispatchedAt": 2, "completedAt": 3, "win32Error": 0}
        return {**body, "ok": True, "accepted": True, "status": "accepted", "message": "accepted"}


def test_python_client_uses_agent_http_contract():
    transport = FakeTransport()
    client = Client(_transport=transport, _ensure=False, client_id="python-test")
    assert client.initialize(True).ok
    assert client.get_state().tdx.code == "600000"
    command = client.set_code("tdx", "600825")
    assert command.accepted
    assert transport.calls[-1][2]["clientId"] == "python-test"
    assert transport.calls[-1][2]["requestId"].startswith("request-")


def test_close_is_idempotent_without_owning_runtime():
    client = Client(_transport=FakeTransport(), _ensure=False)
    client.close()
    client.close()
    assert client.closed


def test_python_client_waits_for_structured_command_status():
    transport = FakeTransport()
    client = Client(_transport=transport, _ensure=False, client_id="python-test")
    result = client.set_code_and_wait("tdx", "600825", timeout=0.5)
    assert result.state == "confirmed"
    assert transport.calls[-1][0].startswith("/v1/command-status?")
