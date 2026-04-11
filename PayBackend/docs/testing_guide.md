# Pay Plugin Testing Guide

## Test Structure

```
test/
├── service/              # Service unit tests
│   ├── MockHelpers.h
│   ├── IdempotencyServiceTest.cc
│   ├── PaymentServiceTest.cc
│   ├── RefundServiceTest.cc
│   └── CallbackServiceTest.cc
└── integration/          # Integration tests
    ├── WechatCallbackIntegrationTest.cc
    ├── CreatePaymentIntegrationTest.cc
    └── ...
```

## Running Tests

### Unit Tests

```bash
cd PayBackend/build/Release
./PayBackendTest --gtest_filter=*Service*
```

### Integration Tests

```bash
./PayBackendTest
```

### Specific Test

```bash
./PayBackendTest --gtest_filter=*PaymentService.CreatePayment_Success
```

## Writing Service Tests

### Example Test Structure

```cpp
TEST_F(PaymentServiceTest, CreatePayment_Success) {
    // Arrange
    CreatePaymentRequest request;
    request.orderNo = "TEST-ORDER-001";
    request.amount = "10000";
    request.currency = "CNY";
    request.description = "Test payment";
    request.userId = 12345;

    Json::Value wechatResponse;
    wechatResponse["prepay_id"] = "wx_prepay_123";

    EXPECT_CALL(*mockWechatClient_, createOrder(testing::_, testing::_))
        .WillOnce([wechatResponse](const Json::Value& req, auto callback) {
            callback(wechatResponse);
        });

    // Mock database operations
    EXPECT_CALL(*mockDbClient_, execSqlAsync(testing::_, testing::_))
        .Times(testing::AtLeast(3));

    // Act
    service_->createPayment(request, "idempotency-key-001",
        [this](const Json::Value&result, const std::error_code& error) {
            // Assert
            ASSERT_FALSE(error);
            ASSERT_EQ(result["code"].asInt(), 0);
            ASSERT_FALSE(result["data"]["payment_no"].asString().empty());
        }
    );
}
```

## Test Coverage Goals

- Service unit tests: 80%+ coverage
- Integration tests: All critical paths
- Edge cases: Error handling, validation, idempotency
- Performance: Load tests for high-concurrency scenarios

## Mock Objects

### MockWechatPayClient
Simulates WeChat Pay API responses without making actual API calls.

### MockDbClient
Simulates database operations for fast, isolated testing.

### MockRedisClient
Simulates Redis cache operations.

## CI/CD Integration

Tests run automatically on:
- Every pull request
- Every merge to main branch
- Scheduled nightly builds

## Test Data Management

- Use test database schema (separate from production)
- Clean up test data after each test
- Use transactions for rollback
- Mock external API calls
