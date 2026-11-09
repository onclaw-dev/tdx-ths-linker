"""Minimal direct-Python TDX/THS state and write example."""

import argparse
import json
import time
from dataclasses import asdict

from tdx_ths_linker import Client


def parser() -> argparse.ArgumentParser:
    value = argparse.ArgumentParser(description="Read or explicitly update TDX/THS stock codes")
    value.add_argument(
        "--accept-license",
        action="store_true",
        help="confirm acceptance of the package's GPL-3.0-only license",
    )
    value.add_argument("--code", help="optional six-digit code; omit for a read-only run")
    value.add_argument("--target", choices=("tdx", "ths", "both"))
    value.add_argument("--timeout", type=float, default=10.0)
    return value


def main() -> int:
    args = parser().parse_args()
    if not args.accept_license:
        raise SystemExit("Review the repository license, then pass --accept-license explicitly")
    if bool(args.code) != bool(args.target):
        raise SystemExit("--code and --target must be supplied together")
    if args.code and (len(args.code) != 6 or not args.code.isascii() or not args.code.isdigit()):
        raise SystemExit("--code must contain exactly six ASCII digits")
    if args.timeout <= 0:
        raise SystemExit("--timeout must be greater than zero")

    with Client(license_accepted=True) as client:
        readiness_deadline = time.monotonic() + min(args.timeout, 5.0)
        while True:
            before = client.get_state()
            if before.tdx.available and before.ths.available:
                break
            if time.monotonic() >= readiness_deadline:
                break
            time.sleep(0.25)
        output = {"before": asdict(before), "commands": {}}
        if not args.code:
            print(json.dumps(output, ensure_ascii=False, indent=2))
            return 0

        targets = ("tdx", "ths") if args.target == "both" else (args.target,)
        unavailable = [target for target in targets if not getattr(before, target).available]
        if unavailable:
            raise SystemExit(f"Unavailable target(s): {', '.join(unavailable)}")

        for target in targets:
            command = client.set_code(target, args.code)
            output["commands"][target] = asdict(command)
            if not command.accepted:
                raise SystemExit(f"{target} rejected the command: {command.message}")

        deadline = time.monotonic() + args.timeout
        while True:
            after = client.get_state()
            if all(getattr(after, target).code == args.code for target in targets):
                output["confirmed"] = True
                output["after"] = asdict(after)
                print(json.dumps(output, ensure_ascii=False, indent=2))
                return 0
            if time.monotonic() >= deadline:
                output["confirmed"] = False
                output["after"] = asdict(after)
                print(json.dumps(output, ensure_ascii=False, indent=2))
                return 5
            time.sleep(0.25)


if __name__ == "__main__":
    raise SystemExit(main())
