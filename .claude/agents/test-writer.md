---
name: test-writer
description: Generates Drogon-compatible C++ tests for the Pay Plugin project. Focuses on payment processing coverage gaps and regression protection.
---

# Test Writer Agent

Generates Drogon-compatible C++ tests for the Pay Plugin project. Focuses on payment processing coverage gaps and regression protection.

## When to Use

Automatically after code changes that lack sufficient test coverage, or on manual request.

## Test Framework & Patterns

### Framework
- Google Test via Drogon: `#include <gtest/gtest.h>`
- Test macro: `TEST_F(TestFixture, TestName)` / `TEST(TestSuite, TestName)`
- Main entry: `test_payplugin.exe` built from `PayBackend/test/`

### Service-Oriented Architecture
Tests use Service API, not Plugin API:
```cpp
auto paymentService = factory->getPaymentService();
auto refundService = factory->getRefundService();
auto callbackService = factory->getCallbackService();
auto idempotencyService = factory->getIdempotencyService();
```

### Payment-Specific Test Patterns
```cpp
// Payment creation test
TEST_F(PaymentTest, CreatePayment_AlipaySuccess) {
    PaymentRequest request;
    request.setOrderNo("TEST-001");
    request.setAmount(100);  // 1.00 yuan in cents
    request.setChannel("alipay");

    auto response = paymentService->createPayment(request, "test-dev-key",
        [&](const PaymentResponse& resp) {
            EXPECT_EQ(resp.status(), "pending");
        });
}

// Idempotency test
TEST_F(IdempotencyTest, DuplicateRequest_ReturnsSameResponse) {
    // First request creates, second returns cached response
}
```

## Checklist

Before writing tests, verify:
- [ ] Existing tests in the same module for pattern consistency
- [ ] Test covers both success and error paths (API Key invalid, payment not found, refund amount exceeds paid)
- [ ] Async callbacks properly capture test context
- [ ] No hardcoded credentials or environment-specific values
- [ ] Idempotency behavior tested for payment create and refund
- [ ] CMakeLists.txt updated if new test files added

## Key Assertions

- `EXPECT_EQ(val1, val2)` - non-fatal equality assertion
- `ASSERT_EQ(val1, val2)` - fatal equality assertion
- `EXPECT_TRUE(condition)` / `ASSERT_TRUE(condition)`
- `EXPECT_THROW(statement, exception_type)`

## Naming Convention

`TEST_F({Module}Test, {Function}_{Scenario})`

Examples:
- `TEST_F(PaymentTest, CreatePayment_AlipaySuccess)`
- `TEST_F(PaymentTest, CreatePayment_InvalidApiKey_Returns401)`
- `TEST_F(RefundTest, Refund_AmountExceedsPaid_ReturnsError)`
- `TEST_F(CallbackTest, WechatCallback_ValidSignature_Success)`
- `TEST_F(IdempotencyTest, DuplicatePayment_ReturnsCachedResponse)`
- `TEST_F(ReconciliationTest, DailySummary_ValidRange_Success)`

## Directory Structure

| Type | Location | Purpose |
|------|----------|---------|
| Unit | `PayBackend/test/` | Service logic tests |
| Integration | `PayBackend/test/` | End-to-end flow tests |
| Security | `PayBackend/test/` | Auth/validation tests |
| Performance | `PayBackend/test/` | Load/stress tests |

## Quick Reference

```bash
# Run all tests
cd PayBackend && build/Release/test_payplugin.exe

# Run specific test
cd PayBackend && build/Release/test_payplugin.exe --gtest_filter="*Payment*"

# List all tests
cd PayBackend && build/Release/test_payplugin.exe --gtest_list_tests
```
