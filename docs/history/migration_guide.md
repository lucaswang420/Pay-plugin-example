# Pay Plugin Migration Guide

This guide explains how to migrate from the old PayPlugin architecture to the new service-based architecture.

## What Changed?

### Before
```cpp
// Old: Direct Plugin method calls
auto plugin = app().getPlugin<PayPlugin>();
plugin->createPayment(...);
```

### After
```cpp
// New: Service accessor pattern
auto plugin = app().getPlugin<PayPlugin>();
auto service = plugin->paymentService();
service->createPayment(...);
```

## Migration Steps

### 1. Update Controllers

**Old Code:**
```cpp
void PayController::createPayment(...) {
    auto plugin = app().getPlugin<PayPlugin>();
    plugin->createPayment(req, callback);
}
```

**New Code:**
```cpp
void PayController::createPayment(...) {
    auto plugin = app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();
    paymentService->createPayment(request, idempotencyKey, callback);
}
```

### 2. Update Service Method Signatures

**Old Signature:**
```cpp
void createPayment(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback
);
```

**New Signature:**
```cpp
void createPayment(
    const CreatePaymentRequest& request,
    const std::string& idempotencyKey,
    PaymentCallback&& callback
);
```

### 3. Extract Parameters in Controller

**Old:** Plugin extracted HTTP parameters

**New:** Controller extracts parameters and creates request object:

```cpp
CreatePaymentRequest request;
request.orderNo = (*json)["order_no"].asString();
request.amount = (*json)["amount"].asString();
request.userId = req->attributes()->get<int64_t>("user_id");
```

## Method Mapping

| Old Method | New Service | New Method |
|------------|-------------|------------|
| `PayPlugin::createPayment` | `PaymentService` | `createPayment` |
| `PayPlugin::queryOrder` | `PaymentService` | `queryOrder` |
| `PayPlugin::refund` | `RefundService` | `createRefund` |
| `PayPlugin::queryRefund` | `RefundService` | `queryRefund` |
| `PayPlugin::handleWechatCallback` | `CallbackService` | `handlePaymentCallback` |
| `PayPlugin::reconcileSummary` | `PaymentService` | `reconcileSummary` |

## Testing After Migration

1. Run all integration tests
2. Test payment creation flow
3. Test callback processing
4. Verify idempotency behavior
5. Check reconciliation tasks

## Common Issues

### Issue: Compilation errors about missing methods

**Solution**: Update to use service accessor pattern

### Issue: Idempotency key not found

**Solution**: Controllers must extract or generate idempotency key

### Issue: Async callback signature mismatch

**Solution**: Update callback signatures to match Service interfaces

## Rollback Plan

If issues arise:
1. Use git to revert to commit before refactoring
2. Report issues in project issue tracker
3. Review architecture design document

## Next Steps

After migration:
1. Review [architecture_overview.md](architecture_overview.md)
2. Read [testing_guide.md](testing_guide.md)
3. Configure services in [configuration_guide.md](configuration_guide.md)
