from typing import Optional

from ._native import NativeBinding
from .contracts import CommandResult, InitResult, RuntimeState, Target
from .exceptions import ClientClosedError


class Client:
    def __init__(self, license_accepted: Optional[bool] = None, *, _binding=None) -> None:
        self._binding = _binding or NativeBinding()
        self._handle: Optional[int] = self._binding.create()
        if license_accepted is not None:
            try:
                self.initialize(license_accepted)
            except Exception:
                self.close()
                raise

    @property
    def closed(self) -> bool:
        return self._handle is None

    def _require_handle(self) -> int:
        if self._handle is None:
            raise ClientClosedError("Client is closed")
        return self._handle

    def initialize(self, license_accepted: bool) -> InitResult:
        return self._binding.initialize(self._require_handle(), bool(license_accepted))

    def get_state(self) -> RuntimeState:
        return self._binding.get_state(self._require_handle())

    def set_code(self, target: Target, code: str) -> CommandResult:
        if target not in {"tdx", "ths"}:
            raise ValueError("target must be 'tdx' or 'ths'")
        if len(code) != 6 or not code.isascii() or not code.isdigit():
            raise ValueError("stock code must be six ASCII digits")
        return self._binding.set_code(self._require_handle(), target, code)

    def close(self) -> None:
        handle = self._handle
        if handle is None:
            return
        self._handle = None
        self._binding.destroy(handle)

    def __enter__(self) -> "Client":
        self._require_handle()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass
