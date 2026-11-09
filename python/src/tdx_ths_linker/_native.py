import ctypes
import platform
import sys
from pathlib import Path
from typing import Optional, Union

from .contracts import CommandResult, EndpointState, InitResult, RuntimeState
from .exceptions import AbiMismatchError, NativeCallError, NativeLoadError, UnsupportedPlatformError

ABI_VERSION = 1
NAME_CAPACITY = 8
CODE_CAPACITY = 16
HEALTH_CAPACITY = 16
STATUS_CAPACITY = 32
MESSAGE_CAPACITY = 256


class _EndpointStateV1(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("name", ctypes.c_char * NAME_CAPACITY),
        ("code", ctypes.c_char * CODE_CAPACITY),
        ("health", ctypes.c_char * HEALTH_CAPACITY),
        ("last_error", ctypes.c_char * MESSAGE_CAPACITY),
        ("updated_at", ctypes.c_uint64),
        ("last_scan_at", ctypes.c_uint64),
        ("last_resolved_offset", ctypes.c_uint64),
        ("available", ctypes.c_uint32),
        ("using_fallback", ctypes.c_uint32),
        ("direct_read", ctypes.c_uint32),
        ("pid", ctypes.c_uint32),
    ]


class _RuntimeStateV1(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("initialized", ctypes.c_uint32),
        ("supported", ctypes.c_uint32),
        ("runtime_health", ctypes.c_char * HEALTH_CAPACITY),
        ("license_state", ctypes.c_char * HEALTH_CAPACITY),
        ("message", ctypes.c_char * MESSAGE_CAPACITY),
        ("ths", _EndpointStateV1),
        ("tdx", _EndpointStateV1),
    ]


class _InitResultV1(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("ok", ctypes.c_uint32),
        ("license_state", ctypes.c_char * HEALTH_CAPACITY),
        ("runtime_health", ctypes.c_char * HEALTH_CAPACITY),
        ("message", ctypes.c_char * MESSAGE_CAPACITY),
    ]


class _CommandResultV1(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("ok", ctypes.c_uint32),
        ("accepted", ctypes.c_uint32),
        ("status", ctypes.c_char * STATUS_CAPACITY),
        ("target", ctypes.c_char * NAME_CAPACITY),
        ("code", ctypes.c_char * CODE_CAPACITY),
        ("message", ctypes.c_char * MESSAGE_CAPACITY),
    ]


def _text(value: Union[bytes, ctypes.Array]) -> str:
    return bytes(value).split(b"\0", 1)[0].decode("utf-8", errors="replace")


def _endpoint(value: _EndpointStateV1) -> EndpointState:
    return EndpointState(
        name=_text(value.name),
        code=_text(value.code),
        health=_text(value.health),
        last_error=_text(value.last_error),
        updated_at=int(value.updated_at),
        last_scan_at=int(value.last_scan_at),
        last_resolved_offset=int(value.last_resolved_offset),
        available=bool(value.available),
        using_fallback=bool(value.using_fallback),
        direct_read=bool(value.direct_read),
        pid=int(value.pid),
    )


class NativeBinding:
    def __init__(self, library_path: Optional[Path] = None) -> None:
        if sys.platform != "win32" or platform.machine().lower() not in {"amd64", "x86_64"}:
            raise UnsupportedPlatformError("tdx-ths-linker supports Windows x64 only")
        path = library_path or Path(__file__).resolve().parent / "_native" / "onclaw-tdx-ths.dll"
        if not path.is_file():
            raise NativeLoadError(f"Bundled native DLL was not found: {path}")
        try:
            self._dll = ctypes.CDLL(str(path))
        except OSError as error:
            raise NativeLoadError(f"Failed to load native DLL {path}: {error}") from error
        self._configure()
        actual = int(self._dll.onclaw_tdx_ths_abi_version())
        if actual != ABI_VERSION:
            raise AbiMismatchError(f"Native ABI {actual} does not match Python ABI {ABI_VERSION}")
        self.abi_version = actual

    def _configure(self) -> None:
        dll = self._dll
        dll.onclaw_tdx_ths_abi_version.argtypes = []
        dll.onclaw_tdx_ths_abi_version.restype = ctypes.c_uint32
        dll.onclaw_tdx_ths_create.argtypes = []
        dll.onclaw_tdx_ths_create.restype = ctypes.c_void_p
        dll.onclaw_tdx_ths_destroy.argtypes = [ctypes.c_void_p]
        dll.onclaw_tdx_ths_destroy.restype = None
        dll.onclaw_tdx_ths_initialize.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint32,
            ctypes.POINTER(_InitResultV1),
        ]
        dll.onclaw_tdx_ths_initialize.restype = ctypes.c_int
        dll.onclaw_tdx_ths_get_state.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(_RuntimeStateV1),
        ]
        dll.onclaw_tdx_ths_get_state.restype = ctypes.c_int
        dll.onclaw_tdx_ths_set_code.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.POINTER(_CommandResultV1),
        ]
        dll.onclaw_tdx_ths_set_code.restype = ctypes.c_int

    @staticmethod
    def _check(status: int, operation: str) -> None:
        if status != 0:
            raise NativeCallError(f"Native {operation} failed with ABI status {status}")

    def create(self) -> int:
        handle = self._dll.onclaw_tdx_ths_create()
        if not handle:
            raise NativeCallError("Native runtime allocation failed")
        return int(handle)

    def destroy(self, handle: int) -> None:
        self._dll.onclaw_tdx_ths_destroy(ctypes.c_void_p(handle))

    def initialize(self, handle: int, license_accepted: bool) -> InitResult:
        result = _InitResultV1(struct_size=ctypes.sizeof(_InitResultV1))
        status = self._dll.onclaw_tdx_ths_initialize(
            ctypes.c_void_p(handle), int(license_accepted), ctypes.byref(result)
        )
        self._check(status, "initialize")
        return InitResult(
            ok=bool(result.ok),
            license_state=_text(result.license_state),
            runtime_health=_text(result.runtime_health),
            message=_text(result.message),
        )

    def get_state(self, handle: int) -> RuntimeState:
        result = _RuntimeStateV1(struct_size=ctypes.sizeof(_RuntimeStateV1))
        status = self._dll.onclaw_tdx_ths_get_state(ctypes.c_void_p(handle), ctypes.byref(result))
        self._check(status, "get_state")
        return RuntimeState(
            initialized=bool(result.initialized),
            supported=bool(result.supported),
            runtime_health=_text(result.runtime_health),
            license_state=_text(result.license_state),
            message=_text(result.message),
            ths=_endpoint(result.ths),
            tdx=_endpoint(result.tdx),
        )

    def set_code(self, handle: int, target: str, code: str) -> CommandResult:
        result = _CommandResultV1(struct_size=ctypes.sizeof(_CommandResultV1))
        status = self._dll.onclaw_tdx_ths_set_code(
            ctypes.c_void_p(handle),
            target.encode("ascii"),
            code.encode("ascii"),
            ctypes.byref(result),
        )
        self._check(status, "set_code")
        return CommandResult(
            ok=bool(result.ok),
            accepted=bool(result.accepted),
            status=_text(result.status),
            target=_text(result.target),
            code=_text(result.code),
            message=_text(result.message),
        )
