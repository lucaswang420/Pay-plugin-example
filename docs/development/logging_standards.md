# Drogon Payment Plugin - Logging Standards

**Version:** 1.0
**Date:** 2026-04-29
**Framework:** Drogon (trantor::Logger)

---

## 📋 Log Levels Overview

Drogon uses trantor::Logger with the following log levels (in order of verbosity):

| Level | Purpose | Production | Development | Example |
|-------|---------|------------|-------------|---------|
| **TRACE** | Most detailed debugging | ❌ Off | ✅ On | Method entry/exit, detailed steps |
| **DEBUG** | Debugging information | ❌ Off | ✅ On | Variable values, intermediate steps |
| **INFO** | Important business operations | ✅ On | ✅ On | Order created, payment succeeded |
| **WARN** | Recoverable exceptional situations | ✅ On | ✅ On | Retry, degradation, fallback |
| **ERROR** | Error conditions | ✅ On | ✅ On | Database errors, API failures |

---

## 🎯 Logging Standards by Level

### TRACE - 最详细的调试信息

**Usage:** Only in development environment for deep debugging

**When to use:**
- Method entry/exit points
- Detailed execution flow
- Complex algorithm steps
- Performance timing data

**Examples:**
```cpp
// ✅ Good: Method entry/exit
LOG_TRACE << "[PaymentService] createPayment() called with order_no=" << request.orderNo;
LOG_TRACE << "[PaymentService] createPayment() returning payment_no=" << paymentNo;

// ✅ Good: Detailed execution flow
LOG_TRACE << "[IdempotencyService] Checking idempotency key: " << idempotencyKey;
LOG_TRACE << "[IdempotencyService] Idempotency key found in cache, returning snapshot";
LOG_TRACE << "[IdempotencyService] Idempotency key not found, proceeding with request";

// ❌ Avoid: Too detailed in production
LOG_TRACE << "Variable x = " << x << ", y = " << y << ", z = " << z;
```

### DEBUG - 调试信息

**Usage:** Development and troubleshooting

**When to use:**
- Variable values (non-sensitive)
- Intermediate calculation results
- Database query details
- API request/response details (sanitized)

**Examples:**
```cpp
// ✅ Good: Channel selection logic
LOG_DEBUG << "Creating payment for channel: " << request.channel << ", order: " << request.orderNo;

// ✅ Good: Database operation details
LOG_DEBUG << "Executing SQL: " << sql;
LOG_DEBUG << "Query returned " << results.size() << " rows";

// ✅ Good: API call details (sanitized)
LOG_DEBUG << "Calling Alipay API with method: " << method;
LOG_DEBUG << "Alipay response status: " << statusCode;

// ❌ Avoid: Sensitive information
LOG_DEBUG << "API Key: " << apiKey;  // Don't log credentials!
LOG_DEBUG << "Password: " << password;  // Don't log secrets!
```

### INFO - 重要业务操作

**Usage:** Always on in production, track business-critical operations

**When to use:**
- Order creation/completion
- Payment success/failure
- Refund initiation/completion
- Service startup/shutdown
- Configuration changes
- Business milestones

**Examples:**
```cpp
// ✅ Good: Business operations
LOG_INFO << "Order created: order_no=" << orderNo << ", user_id=" << userId << ", amount=" << amount;
LOG_INFO << "Payment succeeded: payment_no=" << paymentNo << ", order_no=" << orderNo << ", channel=" << channel;
LOG_INFO << "Refund completed: refund_no=" << refundNo << ", order_no=" << orderNo << ", amount=" << amount;
LOG_INFO << "PaymentService initialized with Alipay and WeChat clients";

// ✅ Good: Service lifecycle
LOG_INFO << "Starting reconciliation process for " << pendingOrders << " orders";
LOG_INFO << "Reconciliation completed: " << successCount << " updated, " << failureCount << " failed";

// ❌ Avoid: Too detailed for INFO
LOG_INFO << "Processing item 1 of 100";  // Use DEBUG instead
LOG_INFO << "Variable x = " << x;  // Use DEBUG instead
```

### WARN - 可恢复的异常情况

**Usage:** Situations that require attention but don't prevent operation

**When to use:**
- Retried operations (successful retry)
- Fallback to default behavior
- Missing optional data
- Performance degradation
- Unusual but acceptable values

**Examples:**
```cpp
// ✅ Good: Successful retry
LOG_WARN << "Database connection lost, retrying... (attempt " << retryCount << "/3)";
LOG_WARN << "Alipay API timeout, using cached status for order " << orderNo;

// ✅ Good: Fallback behavior
LOG_WARN << "Redis unavailable, falling back to database idempotency check";
LOG_WARN << "Optional field 'notify_url' not provided, using default";

// ✅ Good: Unusual but acceptable
LOG_WARN << "Payment amount mismatch: expected " << expectedAmount << ", got " << actualAmount;
LOG_WARN << "Unknown trade_status from Alipay: " << tradeStatus << ", treating as pending";

// ❌ Avoid: Critical errors in WARN
LOG_WARN << "Database connection failed completely";  // Use ERROR instead
LOG_WARN << "Payment processing failed";  // Use ERROR instead
```

### ERROR - 错误情况

**Usage:** Error conditions that affect functionality

**When to use:**
- Database operation failures
- API call failures (after retries)
- Invalid input data
- Configuration errors
- Resource exhaustion
- Exceptions and errors

**Examples:**
```cpp
// ✅ Good: Database errors
LOG_ERROR << "Failed to insert order record: " << e.base().what();
LOG_ERROR << "Payment record not found for payment_no: " << paymentNo;

// ✅ Good: API failures
LOG_ERROR << "Alipay API request failed after 3 retries: " << error;
LOG_ERROR << "WeChat Pay callback verification failed: " << errorMsg;

// ✅ Good: Invalid input
LOG_ERROR << "Invalid refund amount: " << amount << " exceeds paid amount: " << paidAmount;
LOG_ERROR << "Missing required field: order_no";

// ✅ Good: Configuration errors
LOG_ERROR << "Failed to load Alipay private key from: " << privateKeyPath;
LOG_ERROR << "Database connection failed: " << connectionConfig;

// ❌ Avoid: Expected failures in ERROR
LOG_ERROR << "User not found";  // This might be expected, use DEBUG or INFO
LOG_ERROR << "No records found";  // Use DEBUG for empty results
```

---

## 🔒 Security and Privacy

### Sensitive Data Handling

**Never log:**
- API keys, secrets, tokens
- Passwords, PINs
- Complete credit card numbers
- Personal identification information (PII)
- User session IDs
- Cryptographic keys

**Always sanitize:**
- Request/response payloads (remove sensitive fields)
- User data (anonymize or hash)
- Error messages (don't expose internal details)

**Examples:**
```cpp
// ❌ BAD: Logging sensitive data
LOG_DEBUG << "API Request: " << fullRequestWithApiKey;  // Contains API key!
LOG_DEBUG << "User credit card: " << creditCardNumber;  // PII!

// ✅ GOOD: Sanitized logging
LOG_DEBUG << "API Request endpoint: " << endpoint << ", method: " << method;
LOG_DEBUG << "Payment amount: " << amount << ", currency: " << currency;

// ✅ GOOD: Masked sensitive data
LOG_DEBUG << "User ID: " << userId << ", API Key: ****" << apiKey.substr(apiKey.length() - 4);
```

---

## 📝 Log Message Format

### Standard Format

```
[ServiceName] Operation: key1=value1, key2=value2, additional_context
```

### Components

1. **Service/Component Name** - In brackets at start
2. **Operation** - Brief description of what's happening
3. **Key-Value Pairs** - Important context data
4. **Additional Context** - Error messages, status, etc.

### Examples

```cpp
// ✅ Good: Structured log
LOG_INFO << "[PaymentService] Order created: order_no=" << orderNo 
         << ", user_id=" << userId << ", amount=" << amount 
         << ", channel=" << channel;

LOG_ERROR << "[RefundService] Refund failed: refund_no=" << refundNo 
          << ", order_no=" << orderNo << ", error=" << errorMsg;

LOG_WARN << "[AlipayClient] Timeout retry: order_no=" << orderNo 
         << ", attempt=" << retryCount << "/3";

// ❌ Bad: Unstructured log
LOG_INFO << "Order created";  // Missing context
LOG_ERROR << "Something went wrong";  // No details
```

---

## 🚀 Performance Considerations

### Logging Best Practices

1. **Lazy Evaluation:** Use conditionals for expensive operations

```cpp
// ✅ Good: Conditional logging
if (LOG_LEVEL >= trantor::Logger::kDebug) {
    std::string expensiveString = buildExpensiveString();
    LOG_DEBUG << "Debug info: " << expensiveString;
}

// ✅ Good: Direct logging for simple cases
LOG_DEBUG << "Simple debug info: " << simpleValue;
```

2. **Avoid String Concatenation in Hot Paths:**

```cpp
// ❌ Bad: Always constructs string
LOG_DEBUG << "Info: " << obj.toString();  // toString() always called

// ✅ Good: Uses stream operator
LOG_DEBUG << "Info: " << obj;  // Only constructed if logging enabled
```

3. **Batch Logging:** Consider batching log writes in high-frequency scenarios

---

## 🔧 Configuration Examples

### Development Environment

```json
{
  "log": {
    "log_path": "./logs/",
    "logfile_base_name": "dev",
    "log_level": "DEBUG",
    "log_size_limit": 100000000,
    "display_local_time": true
  }
}
```

### Production Environment

```json
{
  "log": {
    "log_path": "/var/log/payplugin/",
    "logfile_base_name": "production",
    "log_level": "INFO",
    "log_size_limit": 500000000,
    "max_files": 10,
    "display_local_time": false
  }
}
```

### Testing/CI Environment

```json
{
  "log": {
    "log_path": "",
    "logfile_base_name": "",
    "log_level": "WARN",
    "display_local_time": false
  }
}
```

---

## 📊 Monitoring and Alerting

### Key Metrics to Monitor

1. **ERROR Count:** High error rate indicates problems
2. **WARN vs INFO Ratio:** Many WARNs may indicate systemic issues
3. **Log Volume:** Sudden spikes may indicate problems
4. **Specific Error Patterns:** Recurring errors need investigation

### Alerting Thresholds

- **Critical:** > 100 ERROR logs/minute
- **Warning:** > 1000 WARN logs/minute
- **Info:** Log volume > 10MB/minute

---

## 🧪 Testing Guidelines

### Unit Tests

- Mock logger interface for testing
- Verify correct log level usage
- Test log message format

### Integration Tests

- Verify logging in different environments
- Test log rotation
- Validate sensitive data sanitization

---

## ✅ Checklist

Before committing code with logging:

- [ ] Appropriate log level used
- [ ] No sensitive data logged
- [ ] Structured log format followed
- [ ] Performance impact considered
- [ ] Error messages are actionable
- [ ] Service name included
- [ ] Context data provided
- [ ] Production log level tested

---

## 📚 References

- [Drogon Configuration](https://github.com/drogonframework/drogon/wiki/ENG/ENG-11-Configuration-File)
- [Trantor Logger](https://github.com/an-tao/trantor)
- [PayPlugin Architecture](../architecture/overview.md)

---

**Maintained by:** Payment Plugin Development Team
**Last Updated:** 2026-04-29
**Next Review:** 2026-05-29
