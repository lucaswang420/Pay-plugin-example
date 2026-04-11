# Pay Plugin Architecture Overview

## Overview

The Pay Plugin is a Drogon-based payment backend that integrates with WeChat Pay API. This document describes the architecture after the refactoring.

## Architecture Layers

```
HTTP Request
    ↓
Controllers (HTTP protocol layer)
    ↓
Plugin (initialization layer)
    ↓
Services (business logic layer)
    ↓
Infrastructure (DB, Redis, WeChat API)
```

## Components

### Controllers Layer
- **PayController**: Handles payment and refund HTTP endpoints
- **WechatCallbackController**: Handles WeChat payment callbacks
- **Responsibilities**: HTTP protocol, parameter validation, response formatting

### Plugin Layer
- **PayPlugin**: Initialization and lifecycle management
- **Responsibilities**:
  - Create and initialize services
  - Provide service accessor methods
  - Manage timers (reconciliation, certificate refresh)
  - Handle configuration

### Services Layer
- **PaymentService**: Payment creation, query, reconciliation
- **RefundService**: Refund creation, query
- **CallbackService**: Callback verification and processing
- **ReconciliationService**: Scheduled reconciliation tasks
- **IdempotencyService**: Idempotency management (Redis + DB)

### Infrastructure Layer
- **Database**: PostgreSQL (orders, payments, refunds, callbacks, ledger)
- **Cache**: Redis (idempotency cache)
- **External API**: WeChat Pay API

## Design Principles

1. **Separation of Concerns**: Each layer has clear responsibilities
2. **Dependency Injection**: Plugin injects dependencies into services
3. **Async Callbacks**: All I/O operations use async callbacks
4. **Idempotency**: All state-changing operations support idempotency
5. **Transaction Management**: Services manage transactions internally

## Data Flow

### Payment Creation
1. Controller receives HTTP request
2. Extracts parameters and user ID
3. Calls PaymentService.createPayment()
4. Service checks idempotency
5. Calls WeChat Pay API
6. Updates database (order, payment, ledger)
7. Returns result to Controller
8. Controller formats HTTP response

### Callback Processing
1. WeChat sends callback to Controller
2. Controller extracts signature and body
3. Calls CallbackService.handlePaymentCallback()
4. Service verifies signature
5. Decrypts callback data
6. Updates order status
7. Returns success to WeChat

## Comparison with OAuth2 Plugin

| Aspect | OAuth2 Plugin | Pay Plugin |
|--------|---------------|------------|
| Services | AuthService, CleanupService | PaymentService, RefundService, CallbackService, ReconciliationService, IdempotencyService |
| Plugin Size | ~450 lines | ~200 lines |
| Business Logic | In services | In services |
| Callback Handling | OAuth2 callbacks | WeChat payment callbacks |
| Timers | Token cleanup | Reconciliation, certificate refresh |

## Migration from Old Architecture

Before refactoring:
- PayPlugin.cc: 3718 lines (all business logic)
- Controllers called Plugin methods directly

After refactoring:
- PayPlugin.cc: ~200 lines (initialization only)
- Services contain business logic
- Controllers access services via Plugin

See [migration_guide.md](migration_guide.md) for detailed migration steps.
