---
name: tdx-ths-linker
description: Inspect TDX/THS desktop state and explicitly link one or both applications to a user-provided six-digit code through the installed tdx-ths-linker driver. Use for TDX/THS diagnostics, current-code reads, or requested desktop code changes; do not use for market quotes or stock recommendations.
---

# TDX THS Linker

Use the installed `tdx-ths-linker` CLI as the only operational boundary. Do not load the DLL directly or reconstruct process-memory behavior.

## Prerequisites

- Confirm the command exists with `tdx-ths-linker --version`.
- If it is missing, report that the user can install it with `py -m pip install tdx-ths-linker`; do not install it without authorization.
- Native commands require prior GPL acceptance through `--accept-license` or `TDX_THS_LINKER_LICENSE_ACCEPTED=1`. Do not invent acceptance. If neither is already configured, ask the user to review and accept the repository license before proceeding.

## Workflow

For diagnostics or a read-only request, run:

```powershell
tdx-ths-linker --json --accept-license doctor
tdx-ths-linker --json --accept-license state
```

For a write, require a user-provided six-digit code and an explicit target. Treat “联动”, “双端”, or “both” as both endpoints only when the user clearly requests it.

```powershell
tdx-ths-linker --json --accept-license set 600825 --target both --wait
```

Use `--wait` for user-facing writes. Report success only when `ok` and `data.confirmed` are both true. If one endpoint is unavailable, rejected, or timed out, name that endpoint and report incomplete linkage.

Never guess a stock code, convert a stock name into a code without the user's confirmation, or claim that command acceptance means the desktop application has switched.

Read [references/driver-contract.md](references/driver-contract.md) when interpreting JSON, exit codes, timeout details, or integrating the driver into another harness.
