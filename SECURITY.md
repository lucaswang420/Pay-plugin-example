# Security Policy

`drogon-pay` processes payment data. We take vulnerability reports seriously
and ask you to follow coordinated disclosure.

## Supported Versions

| Version | Drogon | Supported |
|---|---|---|
| 1.0.x | 1.9.13 | ✅ security fixes |
| < 1.0 (pre-release monolith) | — | ❌ not supported |

Only the latest patch release of a supported minor line receives fixes.

## Reporting a Vulnerability

**Do not open a public GitHub issue for security problems.**

- Preferred: use GitHub's **[Private vulnerability reporting](../../security/advisories/new)**
  ("Report a vulnerability" on the repository's Security tab).
- Include: affected version/commit, host Drogon version, payment channel
  involved (wechat / alipay / custom), reproduction steps or PoC, and impact
  assessment.

You can expect an acknowledgement within **72 hours** and a triage decision
within **7 days**. Please allow up to **90 days** for a coordinated fix and
release before public disclosure.

## Scope notes

- Signature verification of channel callbacks (`{base_path}/notify/*`),
  API-key authentication (`checkAuth`), idempotency handling and the ledger
  state machine are the most security-sensitive areas.
- Secrets must never be committed: `certs/`, `.env*` and key patterns are
  git-ignored and CI runs gitleaks on every push/PR
  (`.github/workflows/secrets-scan.yml`). Report any bypass of this gate as
  a vulnerability.
- Vulnerabilities in Drogon itself should be reported upstream to
  [drogonframework/drogon](https://github.com/drogonframework/drogon/security);
  we will ship a bumped dependency in a minor release when needed.
