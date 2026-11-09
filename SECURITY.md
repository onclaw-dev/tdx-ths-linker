# Security policy

Do not publish exploitable process-memory details, signing keys, code-signing credentials, or privately reported vulnerabilities in a public issue.

The agent binds only to `127.0.0.1`, requires an explicit custom-protocol activation, binds the session to the activating origin, and issues a short-lived bearer token. Consumers must not bypass these controls.

Official Windows releases must be Authenticode-signed. Release signing credentials and any future profile-signing private key must remain outside this repository.

This project interacts with third-party desktop processes. Test against disposable environments first and keep the experimental THS write build separate from the stable build.
