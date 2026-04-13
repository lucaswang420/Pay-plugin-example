---
name: drogon-service-api-migration
description: Migrate Drogon plugin tests from old plugin API to new Service-based architecture. Use this skill whenever the user mentions migrating tests, updating to Service API, converting plugin calls, refactoring Drogon tests, or needs to update test code to use PaymentService, RefundService, CallbackService, etc. This skill handles the API migration pattern from plugin.method(req, callback) to service->method(request, key, callback) with Promise/Future for async handling.
compatibility:
  - Requires Drogon Framework C++ codebase
  - Works with Google Test framework
  - Requires existing Service implementations (PaymentService, RefundService, etc.)
---

# Drogon Service API Migration

Migrate Drogon plugin tests from old HTTP request-based API to new type-safe Service architecture with async callback pattern.

## Background

Drogon plugins are evolving from a monolithic API to a service-based architecture:

**Old API (HTTP Request-based):**
```cpp
plugin.createPayment(req, callback);
plugin.refund(req, callback);
plugin.queryOrder(req, callback);
```

**New API (Service-based):**
```cpp
auto service = plugin.paymentService();
CreatePaymentRequest request;
// ... set request fields
service->createPayment(request, idempotencyKey, callback);
```

**Benefits:**
- Type-safe request structures
- Better separation of concerns
- Easier to test
- Consistent error handling with std::error_code

## When to Use This Skill

Use when:
- Migrating existing tests to use new PaymentService, RefundService, CallbackService
- Converting plugin API calls to Service API
- Updating test code after Service architecture refactor
- User mentions "update tests", "migrate to new API", "Service API conversion"
- Test code uses old `plugin.method(req, callback)` pattern

## Migration Pattern

### Step 1: Identify Old API Usage

Search for old API patterns in test files:

```cpp
// Pattern 1: Direct plugin method call
plugin.createPayment(req, callback);

// Pattern 2: With request body parsing
plugin.refund(req, callback);

// Pattern 3: Query methods
plugin.queryOrder(req, callback);
```

### Step 2: Convert to New Service API

**General transformation:**

```cpp
// OLD: HttpRequest-based
auto req = drogon::HttpRequest::newHttpRequest();
req->setMethod(drogon::Post);
req->setBody(body);
req->addHeader("Idempotency-Key", idempotencyKey);

plugin.createPayment(req, [&promise](const drogon::HttpResponsePtr &resp) {
    promise.set_value(resp);
});

// NEW: Service-based with Promise/Future
CreatePaymentRequest request;
request.userId = "10001";
request.amount = "9.99";
request.currency = "CNY";
request.description = "Test Order";
request.channel = "WEB";

std::promise<Json::Value> resultPromise;
std::promise<std::error_code> errorPromise;

auto paymentService = plugin.paymentService();
paymentService->createPayment(
    request,
    idempotencyKey,
    [&resultPromise, &errorPromise](const Json::Value& result, const std::error_code& error) {
        resultPromise.set_value(result);
        errorPromise.set_value(error);
    });

auto errorFuture = errorPromise.get_future();
const auto error = errorFuture.get();

if (error) {
    // Handle error
}

auto resultFuture = resultPromise.get_future();
const auto result = resultFuture.get();
```

### Step 3: Add Required Includes

Ensure test file includes necessary headers:

```cpp
#include <future>           // For std::promise, std::future
#include "services/PaymentService.h"
#include "services/RefundService.h"
#include "services/CallbackService.h"
```

### Step 4: Update Test Assertions

Old API used HttpResponse, new API uses Json::Value:

```cpp
// OLD: Check HTTP response
EXPECT_EQ(resp->getStatusCode(), k200OK);
auto body = resp->getBody();
Json::Value result;
Json::Reader().parse(body, result);
EXPECT_TRUE(result.isMember("data"));

// NEW: Check Json::Value result
EXPECT_TRUE(result.isMember("data"));
EXPECT_FALSE(result.isMember("error"));
```

## Service-Specific Patterns

### PaymentService

**Old:**
```cpp
plugin.createPayment(req, callback);
```

**New:**
```cpp
CreatePaymentRequest request;
request.userId = userId;
request.amount = amount;
request.currency = "CNY";
request.description = description;
request.channel = channel;
// ... other fields

auto paymentService = plugin.paymentService();
paymentService->createPayment(request, idempotencyKey, callback);
```

### RefundService

**Old:**
```cpp
plugin.refund(req, callback);
```

**New:**
```cpp
CreateRefundRequest request;
request.orderNo = orderNo;
request.paymentNo = paymentNo;
request.amount = amount;
request.reason = reason;

auto refundService = plugin.refundService();
refundService->createRefund(request, idempotencyKey, callback);
```

### CallbackService

**Old:**
```cpp
plugin.handleWechatCallback(req, callback);
```

**New:**
```cpp
auto callbackService = plugin.callbackService();
callbackService->handlePaymentCallback(
    req->body(),
    req->getHeader("Wechatpay-Signature"),
    req->getHeader("Wechatpay-Timestamp"),
    req->getHeader("Wechatpay-Nonce"),
    req->getHeader("Wechatpay-Serial"),
    callback
);
```

### Query Services

**Old:**
```cpp
plugin.queryOrder(orderNo, callback);
```

**New:**
```cpp
auto paymentService = plugin.paymentService();
paymentService->queryOrder(orderNo, callback);
```

## Error Handling

New API uses std::error_code:

```cpp
auto errorFuture = errorPromise.get_future();
const auto error = errorFuture.get();

if (error) {
    // Error occurred
    std::cout << "Error: " << error.message() << std::endl;
    // error.value() contains error code
    // error.category() describes error category
}
```

Common error codes:
- `1001-1999`: Payment errors
- `1404`: Not found
- `1409`: Conflict (idempotency)

## Testing After Migration

After migrating tests, verify:

1. **Compilation:**
   ```bash
   cd PayBackend
   cmake --build build --target test_payplugin
   ```

2. **Run specific test:**
   ```bash
   ./build/Release/test_payplugin.exe --gtest_filter="*YourTest*"
   ```

3. **Verify test still passes:**
   - Test logic should be identical
   - Only API calls changed
   - Assertions should still work

## Common Pitfalls

### 1. Forgetting std::promise

**Wrong:**
```cpp
service->createPayment(request, key, [](const Json::Value& result, const std::error_code& error) {
    // Can't access result here - no promise!
});
```

**Right:**
```cpp
std::promise<Json::Value> resultPromise;
service->createPayment(request, key, [&resultPromise](...) {
    resultPromise.set_value(result);
});
auto result = resultPromise.get_future().get();
```

### 2. Not checking errors

**Wrong:**
```cpp
auto result = resultFuture.get();
// Result might be error!
```

**Right:**
```cpp
auto error = errorFuture.get();
if (error) {
    // Handle error first
    return;
}
auto result = resultFuture.get();
```

### 3. Mixing old and new APIs

Don't mix:
```cpp
plugin.createPayment(req1, callback);  // Old
service->createPayment(req2, key, callback);  // New
```

Be consistent - migrate all related calls together.

## Best Practices

1. **Migrate test by test** - Don't try to migrate everything at once
2. **Keep test logic intact** - Only change API calls, not test behavior
3. **Run tests frequently** - Verify after each migration
4. **Commit in small batches** - Easier to revert if something breaks
5. **Add comments for complex migrations** - Help future maintainers

## Example: Complete Test Migration

**Before (Old API):**
```cpp
TEST(PaymentTest, CreatePaymentSuccess) {
    std::string body = R"({
        "user_id": "10001",
        "amount": "9.99",
        "currency": "CNY"
    })";

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/v1/payment");
    req->setBody(body);

    std::promise<drogon::HttpResponsePtr> promise;
    plugin.createPayment(req, [&promise](const drogon::HttpResponsePtr &resp) {
        promise.set_value(resp);
    });

    auto resp = promise.get_future().get();
    EXPECT_EQ(resp->getStatusCode(), k200OK);
}
```

**After (New API):**
```cpp
TEST(PaymentTest, CreatePaymentSuccess) {
    CreatePaymentRequest request;
    request.userId = "10001";
    request.amount = "9.99";
    request.currency = "CNY";
    request.description = "Test Order";
    request.channel = "WEB";

    std::string idempotencyKey = "test_key_" + std::to_string(std::time(nullptr));

    std::promise<Json::Value> resultPromise;
    std::promise<std::error_code> errorPromise;

    auto paymentService = plugin.paymentService();
    paymentService->createPayment(
        request,
        idempotencyKey,
        [&resultPromise, &errorPromise](const Json::Value& result, const std::error_code& error) {
            resultPromise.set_value(result);
            errorPromise.set_value(error);
        });

    auto errorFuture = errorPromise.get_future();
    const auto error = errorFuture.get();
    EXPECT_FALSE(error);

    auto resultFuture = resultPromise.get_future();
    const auto result = resultFuture.get();
    EXPECT_TRUE(result.isMember("data"));
}
```

## Verification Checklist

After migration:
- [ ] Test compiles without errors
- [ ] Test runs successfully
- [ ] Test assertions still pass
- [ ] No old API calls remain
- [ ] Error handling is correct
- [ ] std::promise/future used correctly
- [ ] Required includes added

## Related Skills

- **cpp-project-file-cleanup** - Clean up project after migration
- **cpp-test-refactoring** - General C++ test refactoring
- **drogon-release-build** - Ensure correct build configuration

## References

See project examples:
- `test/QueryOrderTest.cc` - Query API migration
- `test/RefundQueryTest.cc` - Refund API migration
- `test/CreatePaymentIntegrationTest.cc` - Payment API migration
