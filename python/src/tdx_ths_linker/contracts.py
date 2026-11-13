from dataclasses import dataclass
from typing import Literal

Target = Literal["tdx", "ths"]


@dataclass(frozen=True)
class EndpointState:
    name: str
    code: str
    health: str
    last_error: str
    updated_at: int
    last_scan_at: int
    last_resolved_offset: int
    available: bool
    using_fallback: bool
    direct_read: bool
    pid: int


@dataclass(frozen=True)
class RuntimeState:
    initialized: bool
    supported: bool
    active: bool
    last_activity_at: int
    runtime_health: str
    license_state: str
    message: str
    ths: EndpointState
    tdx: EndpointState


@dataclass(frozen=True)
class InitResult:
    ok: bool
    license_state: str
    runtime_health: str
    message: str


@dataclass(frozen=True)
class CommandResult:
    ok: bool
    accepted: bool
    status: str
    target: str
    code: str
    client_id: str
    request_id: str
    message: str
