# drogon-pay Architecture Overview

## Overview

`drogon-pay` is a **reusable payment plugin library** for the
[Drogon](https://github.com/drogonframework/drogon) framework. It packages the
complete payment stack (create / QR code / query / refund / callback signature
verification / reconciliation / idempotency) as a standard `drogon::Plugin`,
shipped as a Conan package (`static-library`) with the CMake target
`DrogonPay::DrogonPay`. Host applications integrate via
`find_package(DrogonPay)` plus a `config.json` plugin block — no business code
is copied. New payment channels (WeChat Pay and Alipay are built in) plug in by
implementing the `PaymentChannel` SPI.

## Repository Structure

```
├── libs/drogon-pay/          # ★ the library — the only published artifact
│   ├── include/drogon_pay/   #   public API (4 headers):
│   │                         #   PayPlugin.h / PaymentChannel.h /
│   │                         #   ChannelRegistry.h / PayErrorCategory.h
│   └── src/                  #   internals (not installed):
│       ├── handlers/         #   HTTP handlers, registered programmatically
│       ├── services/         #   business logic layer
│       ├── channels/         #   built-in WechatChannel / AlipayChannel
│       ├── models/           #   Drogon ORM models
│       └── utils/            #   config loading, crypto, helpers
├── examples/pay-server/      # example host app (.env, CORS, healthz, Docker)
├── examples/pay-admin/       # Vue 3 admin console for the example host
├── tests/                    # DROGON_TEST unit/integration tests (ctest)
├── test_package/             # Conan consumer-side end-to-end verification
├── sql/                      # PostgreSQL migration scripts 000-004
└── cmake/                    # find_package export templates
```

**Architecture guards (one-way dependency direction):**

- The library never depends on anything under `examples/`.
- Public headers (`include/drogon_pay/`) never leak `src/` internals.
- The service layer depends only on the `PaymentChannel` SPI and never
  includes concrete channel headers.

## Runtime Architecture

```
host app ──find_package(DrogonPay)──▶ DrogonPay::DrogonPay (STATIC)
config.json plugins: [PayPlugin] ──▶ channel assembly → service wiring → programmatic routes
                                        │
                          ChannelRegistry (frozen at startup, lock-free at runtime)
                                        │
                    ┌───────────────────┼──────────────────┐
              WechatChannel       AlipayChannel      custom host channels
              (built-in SPI impl) (built-in SPI impl) (via registerFactory)
```

### Layers

```
HTTP Request
    ↓
Handlers (HTTP protocol layer — programmatic routes under base_path)
    ↓
PayPlugin (assembly & lifecycle layer)
    ↓
Services (business logic layer)
    ↓
PaymentChannel SPI ──▶ concrete channels (WeChat / Alipay / custom)
    ↓
Infrastructure (PostgreSQL, Redis, provider HTTP APIs)
```

### PayPlugin (assembly & lifecycle)

- Reads the plugin config block (`base_path`, `db_client`, `channels`, ...).
- Assembles enabled channels through `ChannelRegistry`, then **freezes** the
  registry — registration is startup-only, lookups are lock-free at runtime.
- Constructs services and injects their dependencies.
- Registers all HTTP routes **programmatically** (no `ADD_METHOD_TO` static
  registration, so no symbols are lost when linking the static library); the
  route prefix `base_path` is configurable (default `/api/pay`).
- Manages timers: scheduled reconciliation (dedicated worker thread) and
  WeChat platform-certificate hot refresh (atomic snapshot swap).

### Channel SPI

- `drogon_pay::PaymentChannel` is the abstract interface every payment
  provider implements (create/query/refund/verify-callback...).
- `ChannelRegistry` maps channel names to factories; hosts add custom
  channels via `registerFactory` before the registry freezes.
- Unknown channels fail explicitly with `CHANNEL_NOT_AVAILABLE` — there is no
  implicit fallback.
- Built-in channels reuse one `HttpClient` per IO loop
  (`IOThreadStorage` + keep-alive).

### Services Layer

- **PaymentService**: payment creation, query, order listing
- **RefundService**: refund creation, query
- **CallbackService**: callback verification, state machine, ledger writes
- **ReconciliationService**: scheduled reconciliation tasks
- **IdempotencyService**: idempotency keys (Redis optional, database fallback)

### Infrastructure Layer

- **Database**: PostgreSQL (orders, payments, refunds, callbacks, ledger) —
  schema in [sql/](../../sql/)
- **Cache**: Redis (optional idempotency cache)
- **External APIs**: WeChat Pay / Alipay endpoints, called through channels

## Design Principles

1. **Library-first**: the plugin is the product; hosts stay thin
   (host concerns like `.env`, CORS, and health probes live in
   `examples/pay-server`).
2. **Separation of concerns**: handlers ↔ services ↔ channels each have a
   single responsibility.
3. **Dependency injection**: `PayPlugin` wires services and channels at
   startup.
4. **Explicit failure**: unknown channels and misconfiguration fail fast at
   startup or with typed error codes (`PayErrorCategory`).
5. **Async everywhere**: all I/O uses Drogon async callbacks.
6. **Idempotency**: every state-changing operation supports idempotency keys.

## Data Flow

### Payment Creation

1. Handler receives the HTTP request (auth filter validates API key + scope).
2. Parameters are validated and passed to `PaymentService::createPayment()`.
3. Service checks the idempotency key (Redis, falling back to DB).
4. Service resolves the channel from `ChannelRegistry` and calls the SPI.
5. Channel calls the provider API (per-IO-loop HttpClient).
6. Service persists order / payment / ledger rows.
7. Handler formats the HTTP response.

### Callback Processing

1. Provider POSTs the callback to the channel-specific callback route.
2. Handler extracts signature headers and raw body.
3. `CallbackService` verifies the signature via the channel SPI and decrypts
   the payload.
4. The callback state machine transitions the order and writes the ledger.
5. The provider-specific acknowledgment is returned.

## Version Compatibility

Static library + C++ ABI make the Drogon version a hard constraint: the
library and the host must use the **same Drogon version** (currently pinned to
1.9.13, C++17). Drogon upgrades ship as minor releases of this library.

## Further Reading

- [Host Integration Guide](../development/plugin_integration.md) — five-step
  setup, configuration keys, route table, custom channel development, and the
  v1.0 breaking-change mapping
- [Deployment Guide](../deployment/deployment_guide.md)
- Historical refactoring records: [docs/history/](../history/)
