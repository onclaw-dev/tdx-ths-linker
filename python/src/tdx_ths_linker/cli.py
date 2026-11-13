import argparse
import json
import os
import sys
import time
from dataclasses import asdict

from . import __version__
from .bootstrap import ensure_agent
from .client import Client


def _accepted(args) -> bool:
    value = os.environ.get("TDX_THS_LINKER_LICENSE_ACCEPTED", "").lower()
    return args.accept_license or value in {"1", "true", "yes"}


def build_parser():
    parser = argparse.ArgumentParser(prog="tdx-ths-linker-python")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--accept-license", action="store_true")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ensure-agent")
    commands.add_parser("doctor")
    commands.add_parser("state")
    set_parser = commands.add_parser("set")
    set_parser.add_argument("code")
    set_parser.add_argument("--target", choices=("tdx", "ths", "both"), required=True)
    set_parser.add_argument("--wait", action="store_true")
    set_parser.add_argument("--timeout", type=float, default=10.0)
    return parser


def run(argv=None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "ensure-agent":
            data = {"agentVersion": ensure_agent()}
        else:
            if not _accepted(args):
                raise ValueError("Pass --accept-license or set TDX_THS_LINKER_LICENSE_ACCEPTED=1")
            with Client(license_accepted=True) as client:
                state = client.get_state()
                if args.command == "doctor":
                    data = {
                        "packageVersion": __version__,
                        "runtime": {
                            "initialized": state.initialized,
                            "supported": state.supported,
                            "runtimeHealth": state.runtime_health,
                        },
                        "endpoints": {"tdx": asdict(state.tdx), "ths": asdict(state.ths)},
                    }
                elif args.command == "state":
                    data = asdict(state)
                else:
                    if len(args.code) != 6 or not args.code.isascii() or not args.code.isdigit():
                        raise ValueError("code must be six ASCII digits")
                    targets = ("tdx", "ths") if args.target == "both" else (args.target,)
                    unavailable = [target for target in targets if not getattr(state, target).available]
                    if unavailable:
                        raise RuntimeError(f"target unavailable: {', '.join(unavailable)}")
                    commands = {target: asdict(client.set_code(target, args.code)) for target in targets}
                    data = {"code": args.code, "targets": list(targets), "commands": commands, "confirmed": False}
                    if args.wait:
                        deadline = time.monotonic() + args.timeout
                        while time.monotonic() < deadline:
                            state = client.get_state()
                            if all(getattr(state, target).code == args.code for target in targets):
                                data["confirmed"] = True
                                data["state"] = {target: asdict(getattr(state, target)) for target in targets}
                                break
                            time.sleep(0.25)
                        if not data["confirmed"]:
                            raise TimeoutError("Timed out waiting for target confirmation")
        envelope = {"schemaVersion": 1, "ok": True, "command": args.command, "data": data}
        print(json.dumps(envelope, ensure_ascii=False) if args.json else json.dumps(envelope, ensure_ascii=False, indent=2))
        return 0
    except Exception as error:
        envelope = {
            "schemaVersion": 1,
            "ok": False,
            "command": getattr(args, "command", "unknown"),
            "error": {"code": "driver_error", "message": str(error)},
        }
        print(json.dumps(envelope, ensure_ascii=False), file=sys.stdout if args.json else sys.stderr)
        return 6


def entrypoint():
    raise SystemExit(run())
