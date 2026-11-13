import argparse
from dataclasses import asdict

from tdx_ths_linker import Client

parser = argparse.ArgumentParser()
parser.add_argument("--accept-license", action="store_true")
parser.add_argument("--code")
parser.add_argument("--target", choices=("tdx", "ths"))
args = parser.parse_args()

if not args.accept_license:
    raise SystemExit("--accept-license is required")
if bool(args.code) != bool(args.target):
    raise SystemExit("--code and --target must be supplied together")

with Client(license_accepted=True) as client:
    print(asdict(client.get_state()))
    if args.code:
        print(asdict(client.set_code(args.target, args.code)))
