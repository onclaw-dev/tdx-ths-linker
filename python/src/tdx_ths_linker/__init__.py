from .bootstrap import ensure_agent
from .client import Client
from .contracts import CommandResult, CommandStatus, EndpointState, InitResult, RuntimeState
from .exceptions import (
    AgentConnectionError,
    AgentLaunchError,
    ClientClosedError,
    TdxThsLinkerError,
    UnsupportedPlatformError,
)

__all__ = [
    "AgentConnectionError",
    "AgentLaunchError",
    "Client",
    "ClientClosedError",
    "CommandResult",
    "CommandStatus",
    "EndpointState",
    "InitResult",
    "RuntimeState",
    "TdxThsLinkerError",
    "UnsupportedPlatformError",
    "ensure_agent",
]

__version__ = "0.2.0"
