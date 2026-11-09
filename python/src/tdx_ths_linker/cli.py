import argparse
import json
import os
import platform
import sys
import time
from dataclasses import asdict
from typing import Any, Callable, Dict, Iterable, Optional, Sequence

from . import __version__
from .client import Client
from .exceptions import (
    AbiMismatchError,
    NativeCallError,
    NativeLoadError,
    UnsupportedPlatformError,
)

SCHEMA_VERSION = 1
EXIT_OK = 0
EXIT_USAGE = 2
EXIT_UNAVAILABLE = 3
EXIT_REJECTED = 4
EXIT_TIMEOUT = 5
EXIT_NATIVE = 6
EXIT_UNEXPECTED = 10


class DriverError(Exception):
    def __init__(
        self,
        exit_code: int,
        code: str,
        message: str,
        details: Optional[Dict[str, Any]] = None,
    ) -> None:
        super().__init__(message)
        self.exit_code = exit_code
        self.code = code
        self.message = message
        self.details = details


class DriverArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise DriverError(EXIT_USAGE, "invalid_usage", message)


def _stock_code(value: str) -> str:
    if len(value) != 6 or not value.isascii() or not value.isdigit():
        raise argparse.ArgumentTypeError("code must be six ASCII digits")
    return value


def _positive(value: str) -> float:
    try:
        parsed = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("value must be a number") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be greater than zero")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = DriverArgumentParser(
        prog="tdx-ths-linker",
        description="Read and link TDX/THS desktop state through the native Windows driver.",
    )
    parser.add_argument("--json", action="store_true", help="emit one schema-versioned JSON object")
    parser.add_argument(
        "--accept-license",
        action="store_true",
        help="confirm acceptance of the bundled GPL-3.0-only license",
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    commands = parser.add_subparsers(dest="command", required=True)

    for name, help_text in (
        ("doctor", "check the native driver and report endpoint availability"),
        ("state", "read current TDX and THS state"),
    ):
        command_parser = commands.add_parser(name, help=help_text)
        command_parser.add_argument("--timeout", type=_positive, default=5.0)
        command_parser.add_argument("--poll-interval", type=_positive, default=0.25)

    set_parser = commands.add_parser("set", help="set a six-digit code on an explicit target")
    set_parser.add_argument("code", type=_stock_code)
    set_parser.add_argument("--target", choices=("tdx", "ths", "both"), required=True)
    set_parser.add_argument("--wait", action="store_true", help="wait until all requested targets match")
    set_parser.add_argument("--timeout", type=_positive, default=10.0)
    set_parser.add_argument("--poll-interval", type=_positive, default=0.25)
    return parser


def _license_accepted(args: argparse.Namespace) -> bool:
    value = os.environ.get("TDX_THS_LINKER_LICENSE_ACCEPTED", "").strip().lower()
    return bool(args.accept_license or value in {"1", "true", "yes"})


def _endpoint_data(endpoint: Any) -> Dict[str, Any]:
    return asdict(endpoint)


def _state_data(state: Any) -> Dict[str, Any]:
    return asdict(state)


def _targets(value: str) -> Sequence[str]:
    return ("tdx", "ths") if value == "both" else (value,)


def _require_available(state: Any, targets: Iterable[str]) -> None:
    unavailable = [target for target in targets if not getattr(state, target).available]
    if unavailable:
        raise DriverError(
            EXIT_UNAVAILABLE,
            "target_unavailable",
            "Requested desktop target is unavailable",
            {
                "targets": unavailable,
                "state": {target: _endpoint_data(getattr(state, target)) for target in unavailable},
            },
        )


def _doctor(init: Any, state: Any) -> Dict[str, Any]:
    return {
        "packageVersion": __version__,
        "schemaVersion": SCHEMA_VERSION,
        "pythonVersion": platform.python_version(),
        "platform": sys.platform,
        "architecture": platform.machine(),
        "initialize": asdict(init),
        "runtime": {
            "initialized": state.initialized,
            "supported": state.supported,
            "runtimeHealth": state.runtime_health,
            "licenseState": state.license_state,
        },
        "endpoints": {
            "tdx": _endpoint_data(state.tdx),
            "ths": _endpoint_data(state.ths),
        },
    }


def _wait_for_initial_scan(
    client: Any,
    timeout: float,
    poll_interval: float,
    sleep: Callable[[float], None],
    monotonic: Callable[[], float],
) -> Any:
    deadline = monotonic() + timeout
    while True:
        state = client.get_state()
        if state.runtime_health != "initializing":
            return state
        now = monotonic()
        if now >= deadline:
            return state
        sleep(min(poll_interval, max(0.0, deadline - now)))


def _set_code(
    client: Any,
    args: argparse.Namespace,
    sleep: Callable[[float], None],
    monotonic: Callable[[], float],
) -> Dict[str, Any]:
    targets = _targets(args.target)
    before = _wait_for_initial_scan(
        client, min(args.timeout, 5.0), args.poll_interval, sleep, monotonic
    )
    _require_available(before, targets)
    commands = {target: asdict(client.set_code(target, args.code)) for target in targets}
    rejected = [target for target, result in commands.items() if not result["accepted"]]
    if rejected:
        raise DriverError(
            EXIT_REJECTED,
            "command_rejected",
            "One or more targets rejected the command",
            {"targets": rejected, "commands": commands},
        )
    result: Dict[str, Any] = {
        "code": args.code,
        "targets": list(targets),
        "commands": commands,
        "confirmed": False,
    }
    if not args.wait:
        return result

    deadline = monotonic() + args.timeout
    last_state = before
    while True:
        last_state = client.get_state()
        confirmed = [target for target in targets if getattr(last_state, target).code == args.code]
        if len(confirmed) == len(targets):
            result["confirmed"] = True
            result["state"] = {target: _endpoint_data(getattr(last_state, target)) for target in targets}
            return result
        if monotonic() >= deadline:
            raise DriverError(
                EXIT_TIMEOUT,
                "confirmation_timeout",
                "Timed out waiting for requested targets to report the code",
                {
                    "code": args.code,
                    "confirmedTargets": confirmed,
                    "state": {target: _endpoint_data(getattr(last_state, target)) for target in targets},
                },
            )
        sleep(min(args.poll_interval, max(0.0, deadline - monotonic())))


def _success(command: str, data: Dict[str, Any]) -> Dict[str, Any]:
    return {"schemaVersion": SCHEMA_VERSION, "ok": True, "command": command, "data": data}


def _failure(command: str, error: DriverError) -> Dict[str, Any]:
    value: Dict[str, Any] = {"code": error.code, "message": error.message}
    if error.details is not None:
        value["details"] = error.details
    return {"schemaVersion": SCHEMA_VERSION, "ok": False, "command": command, "error": value}


def _emit(value: Dict[str, Any], json_mode: bool) -> None:
    if json_mode:
        print(json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True))
        return
    if value["ok"]:
        print(json.dumps(value["data"], ensure_ascii=False, indent=2, sort_keys=True))
    else:
        print(f"{value['error']['code']}: {value['error']['message']}", file=sys.stderr)


def main(
    argv: Optional[Sequence[str]] = None,
    *,
    client_factory: Callable[..., Any] = Client,
    sleep: Callable[[float], None] = time.sleep,
    monotonic: Callable[[], float] = time.monotonic,
) -> int:
    parser = build_parser()
    raw = list(argv) if argv is not None else sys.argv[1:]
    json_mode = "--json" in raw
    command = next((value for value in raw if value in {"doctor", "state", "set"}), "unknown")
    try:
        args = parser.parse_args(raw)
        command = args.command
        json_mode = args.json
        if not _license_accepted(args):
            raise DriverError(
                EXIT_USAGE,
                "license_not_accepted",
                "Pass --accept-license or set TDX_THS_LINKER_LICENSE_ACCEPTED=1",
            )
        with client_factory() as client:
            init = client.initialize(True)
            if not init.ok:
                raise DriverError(EXIT_NATIVE, "initialize_failed", init.message, asdict(init))
            if command == "doctor":
                state = _wait_for_initial_scan(
                    client, args.timeout, args.poll_interval, sleep, monotonic
                )
                data = _doctor(init, state)
            elif command == "state":
                state = _wait_for_initial_scan(
                    client, args.timeout, args.poll_interval, sleep, monotonic
                )
                data = _state_data(state)
            else:
                data = _set_code(client, args, sleep, monotonic)
        _emit(_success(command, data), json_mode)
        return EXIT_OK
    except DriverError as error:
        _emit(_failure(command, error), json_mode)
        return error.exit_code
    except (UnsupportedPlatformError, NativeLoadError, AbiMismatchError, NativeCallError) as error:
        driver_error = DriverError(EXIT_NATIVE, "native_error", str(error))
        _emit(_failure(command, driver_error), json_mode)
        return driver_error.exit_code
    except Exception as error:
        driver_error = DriverError(EXIT_UNEXPECTED, "unexpected_error", str(error))
        _emit(_failure(command, driver_error), json_mode)
        return driver_error.exit_code


def entrypoint() -> None:
    raise SystemExit(main())
