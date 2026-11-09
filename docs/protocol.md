# Local agent protocol v1

The Windows agent listens on `127.0.0.1:17654`. It never binds to a public interface.

## Pairing

1. The browser creates a cryptographically random 32-byte nonce.
2. A user gesture opens `onclaw-tdx-ths://activate?v=1&nonce=<nonce>&origin=<origin>`.
3. The agent binds the pending nonce to that exact origin.
4. The browser calls `POST /v1/session/claim` with the nonce.
5. The agent consumes the nonce and returns a bearer token valid for ten minutes.

HTTPS origins and HTTP loopback development origins are accepted. Every business request must carry the exact paired `Origin` and bearer token.

## Endpoints

| Method | Path | Session | Purpose |
| --- | --- | --- | --- |
| `GET` | `/v1/health` | No | Protocol, version, and capabilities |
| `POST` | `/v1/session/claim` | Pairing nonce | Create a local session |
| `POST` | `/v1/initialize` | Yes | Confirm noncommercial license and initialize runtime |
| `GET` | `/v1/state` | Yes | Read TDX and THS state |
| `POST` | `/v1/set-code` | Yes | Queue a six-digit stock code |

The agent reads and writes both TDX and THS. THS writes use the process-memory command path and require compatibility validation against the installed THS version.
