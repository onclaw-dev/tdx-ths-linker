import json
import uuid
from typing import Any, Dict, Optional
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from .bootstrap import ensure_agent
from .contracts import CommandResult, EndpointState, InitResult, RuntimeState, Target
from .exceptions import AgentConnectionError, ClientClosedError


class HttpTransport:
    def __init__(self, base_url: str = "http://127.0.0.1:17654") -> None:
        self.base_url = base_url.rstrip("/")

    def request(self, path: str, method: str = "GET", body: Optional[Dict[str, Any]] = None):
        data = None if body is None else json.dumps(body).encode("utf-8")
        request = Request(
            f"{self.base_url}{path}",
            data=data,
            method=method,
            headers={"Accept": "application/json", "Content-Type": "application/json"},
        )
        try:
            with urlopen(request, timeout=5.0) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")
            raise AgentConnectionError(f"Agent HTTP {error.code}: {detail}") from error
        except URLError as error:
            raise AgentConnectionError(f"Agent unavailable: {error.reason}") from error


def _endpoint(value: Dict[str, Any]) -> EndpointState:
    return EndpointState(
        name=value.get("name", ""),
        code=value.get("code", ""),
        health=value.get("health", "unavailable"),
        last_error=value.get("lastError", ""),
        updated_at=int(value.get("updatedAt", 0)),
        last_scan_at=int(value.get("lastScanAt", 0)),
        last_resolved_offset=int(value.get("lastResolvedOffset", 0)),
        available=bool(value.get("available", False)),
        using_fallback=bool(value.get("usingFallback", False)),
        direct_read=bool(value.get("directRead", False)),
        pid=int(value.get("pid", 0)),
    )


class Client:
    def __init__(
        self,
        license_accepted: Optional[bool] = None,
        *,
        base_url: str = "http://127.0.0.1:17654",
        client_id: Optional[str] = None,
        _transport=None,
        _ensure=True,
    ) -> None:
        if _transport is None and _ensure:
            ensure_agent(base_url=base_url)
        self._transport = _transport or HttpTransport(base_url)
        self._client_id = client_id or f"python-{uuid.uuid4().hex}"
        self._closed = False
        if license_accepted is not None:
            self.initialize(license_accepted)

    @property
    def closed(self) -> bool:
        return self._closed

    def _require_open(self) -> None:
        if self._closed:
            raise ClientClosedError("Client is closed")

    def initialize(self, license_accepted: bool) -> InitResult:
        self._require_open()
        value = self._transport.request(
            "/v1/initialize", "POST", {"licenseAccepted": bool(license_accepted)}
        )
        return InitResult(
            ok=bool(value.get("ok")),
            license_state=value.get("licenseState", "unaccepted"),
            runtime_health=value.get("runtimeHealth", "unavailable"),
            message=value.get("message", ""),
        )

    def get_state(self) -> RuntimeState:
        self._require_open()
        value = self._transport.request("/v1/state")
        return RuntimeState(
            initialized=bool(value.get("initialized")),
            supported=bool(value.get("isSupported")),
            active=bool(value.get("active")),
            last_activity_at=int(value.get("lastActivityAt", 0)),
            runtime_health=value.get("runtimeHealth", "unavailable"),
            license_state=value.get("licenseState", "unaccepted"),
            message=value.get("message", ""),
            ths=_endpoint(value.get("ths", {})),
            tdx=_endpoint(value.get("tdx", {})),
        )

    def set_code(self, target: Target, code: str) -> CommandResult:
        self._require_open()
        if target not in {"tdx", "ths"}:
            raise ValueError("target must be 'tdx' or 'ths'")
        if len(code) != 6 or not code.isascii() or not code.isdigit():
            raise ValueError("stock code must be six ASCII digits")
        request_id = f"request-{uuid.uuid4().hex}"
        value = self._transport.request(
            "/v1/set-code",
            "POST",
            {
                "target": target,
                "code": code,
                "clientId": self._client_id,
                "requestId": request_id,
            },
        )
        return CommandResult(
            ok=bool(value.get("ok")),
            accepted=bool(value.get("accepted")),
            status=value.get("status", ""),
            target=value.get("target", target),
            code=value.get("code", code),
            client_id=value.get("clientId", self._client_id),
            request_id=value.get("requestId", request_id),
            message=value.get("message", ""),
        )

    def close(self) -> None:
        self._closed = True

    def __enter__(self):
        self._require_open()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()
