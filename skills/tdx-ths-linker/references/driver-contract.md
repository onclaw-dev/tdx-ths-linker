# Driver contract v1

## Commands

```text
npx --no-install tdx-ths-linker [--json] [--accept-license] doctor
npx --no-install tdx-ths-linker [--json] [--accept-license] state
npx --no-install tdx-ths-linker [--json] [--accept-license] set CODE --target tdx|ths|both [--wait] [--timeout SECONDS] [--poll-interval SECONDS]
```

Install the npm package in the current project before invoking it. Commands reuse the single loopback Agent, prefer a registered Web installation, or directly start the packaged bare Agent without registry changes. `CODE` is exactly six ASCII digits. Agent commands require `--accept-license` or environment variable `TDX_THS_LINKER_LICENSE_ACCEPTED=1`.

## JSON envelope

Success:

```json
{
  "schemaVersion": 1,
  "ok": true,
  "command": "state",
  "data": {}
}
```

Failure:

```json
{
  "schemaVersion": 1,
  "ok": false,
  "command": "set",
  "error": {
    "code": "confirmation_timeout",
    "message": "Timed out waiting for requested targets to report the code",
    "details": {}
  }
}
```

In JSON mode stdout contains exactly one envelope. Do not parse human-readable output.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Success |
| `2` | Invalid usage, configuration, or missing license acceptance |
| `3` | Requested target unavailable |
| `4` | Native command rejected |
| `5` | Confirmation timeout |
| `6` | Unsupported platform, Agent launch, connection, or initialization failure |
| `10` | Unexpected driver failure |

## Completion rules

- `commands.<target>.accepted` means the command entered the native processing path.
- `data.confirmed` means all requested targets were subsequently observed with the requested code.
- A `both` write is incomplete unless both `tdx` and `ths` are confirmed.
- Timeout details include `confirmedTargets` and the last observed state for each requested target.
