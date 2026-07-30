# Logging Optimization Plan

**Date:** 2026-04-29
**Goal:** Optimize logging in payment system for proper production/debug control
**Scope:** PayBackend services, plugins, controllers

---

## 🔍 Current State Analysis

### Issues Identified

1. **Log Level Misuse**
   - Too many DEBUG logs that should be INFO (business operations)
   - Some ERROR logs that should be WARN (recoverable situations)
   - Missing INFO logs for critical business operations

2. **Sensitive Information Exposure**
   - Full request payloads logged (may contain sensitive data)
   - Detailed error messages with internal system details

3. **Inconsistent Formatting**
   - Mix of structured and unstructured logs
   - Missing service names in some logs
   - Inconsistent key-value format

4. **Performance Concerns**
   - Expensive string operations in DEBUG logs
   - Detailed logging in hot paths

### Current Log Usage Statistics

**PaymentService.cc:** 45 log statements
- DEBUG: 25 (56%)
- INFO: 2 (4%)
- WARN: 1 (2%)
- ERROR: 17 (38%)

**AlipaySandboxClient.cc:** 27 log statements
- DEBUG: 14 (52%)
- INFO: 2 (7%)
- ERROR: 11 (41%)

**RefundService.cc:** 14 log statements
- DEBUG: 2 (14%)
- INFO: 1 (7%)
- WARN: 2 (14%)
- ERROR: 9 (64%)

---

## 🎯 Optimization Strategy

### Phase 1: Critical Business Operations (INFO)

**Files:** PaymentService.cc, RefundService.cc, CallbackService.cc

**Changes:**
1. Upgrade critical operation logs to INFO:
   - Order creation/completion
   - Payment success/failure
   - Refund initiation/completion
   - Service lifecycle events

2. Add missing INFO logs for:
   - Order status changes
   - Payment method selection
   - Reconciliation results

### Phase 2: Error Handling (ERROR/WARN)

**Files:** All service files

**Changes:**
1. Convert recoverable errors to WARN:
   - Successful retries
   - Fallback behaviors
   - Missing optional data

2. Ensure ERROR is used for:
   - Database operation failures
   - API call failures (after retries)
   - Configuration errors
   - Critical business logic failures

### Phase 3: Debug Information (DEBUG/TRACE)

**Files:** All service files

**Changes:**
1. Keep DEBUG for:
   - Variable values
   - Intermediate steps
   - Database queries
   - API call details (sanitized)

2. Consider adding TRACE for:
   - Method entry/exit
   - Detailed execution flow
   - Complex algorithm steps

### Phase 4: Sensitive Data Protection

**Files:** AlipaySandboxClient.cc, WechatPayClient.cc

**Changes:**
1. Remove or sanitize:
   - Full request payloads
   - API keys and secrets
   - Complete error messages
   - Internal system details

2. Add masking for:
   - User IDs (partial)
   - Order numbers (partial)
   - Transaction IDs (partial)

---

## 📋 Detailed File Changes

### PaymentService.cc

**Current Issues:**
- Line 275: INFO for order insert ✅ (keep)
- Line 290: INFO for payment insert ✅ (keep)
- Lines 482-489: DEBUG for payment client calls ⚠️ (consider INFO for channel selection)
- Lines 552, 598, 613: DEBUG for order creation ⚠️ (upgrade to INFO)
- Lines 680, 729, 748: DEBUG for query operations ⚠️ (some should be INFO)
- Lines 845, 1066: INFO for sync operations ✅ (keep)

**Proposed Changes:**
```cpp
// Line 552: Upgrade to INFO (business operation)
- LOG_DEBUG << "Creating QR payment, channel: " << channel << ", order: " << orderNo;
+ LOG_INFO << "[PaymentService] Creating QR payment: channel=" << channel 
+          << ", order_no=" << orderNo;

// Line 598: Upgrade to INFO (database operation)
- LOG_DEBUG << "Saving order to database: " << orderNo;
+ LOG_INFO << "[PaymentService] Saving order to database: order_no=" << orderNo;

// Line 613: Upgrade to INFO (confirmation)
- LOG_DEBUG << "Order saved to database: " << orderNo
+ LOG_INFO << "[PaymentService] Order saved successfully: order_no=" << orderNo
```

### AlipaySandboxClient.cc

**Current Issues:**
- Lines 232-233: DEBUG logging full request details ⚠️ (security concern)
- Line 244: INFO for API calls ✅ (good)
- Lines 347-348, 412-413: DEBUG for signing details ⚠️ (remove or simplify)
- Many ERROR logs for exceptions ✅ (appropriate)

**Proposed Changes:**
```cpp
// Lines 232-233: Remove sensitive details
- LOG_DEBUG << "Alipay request params: " << commonParams.toStyledString();
- LOG_DEBUG << "Sign data: " << signData;
+ LOG_DEBUG << "[AlipayClient] Request: method=" << method 
+           << ", app_id=" << appId_ << ", timestamp=" << timestamp;

// Lines 347-348: Simplify signing logs
- LOG_DEBUG << "=== SIGN FUNCTION CALLED ===";
- LOG_DEBUG << "Data to sign: " << data;
+ LOG_DEBUG << "[AlipayClient] Generating signature for " << data.length() << " bytes";

// Line 244: Keep INFO (good)
LOG_INFO << "Alipay request: " << method << ", URL: " << gatewayUrl_; ✅
```

### RefundService.cc

**Current Issues:**
- Line 550: DEBUG for refund channel selection ⚠️ (upgrade to INFO)
- Lines 772, 983: WARN for unusual situations ✅ (good)
- Many ERROR logs for failures ✅ (appropriate)

**Proposed Changes:**
```cpp
// Line 550: Upgrade to INFO (business operation)
- LOG_DEBUG << "Refund for order " << orderNo << " using channel: " << channel;
+ LOG_INFO << "[RefundService] Processing refund: order_no=" << orderNo 
+          << ", channel=" << channel;

// Consider adding INFO for completion
+ LOG_INFO << "[RefundService] Refund completed: refund_no=" << refundNo 
+          << ", order_no=" << orderNo << ", amount=" << amount;
```

---

## 🔧 Implementation Tasks

### Task 1: Update PaymentService.cc logging
**Priority:** HIGH
**Files:** 1
**Estimated changes:** 15 log statements

- [ ] Upgrade order creation logs to INFO (3 statements)
- [ ] Upgrade payment success logs to INFO (2 statements)
- [ ] Add structured format to all logs
- [ ] Sensitive data review
- [ ] Test with production log level

### Task 2: Update AlipaySandboxClient.cc logging
**Priority:** HIGH
**Files:** 1
**Estimated changes:** 12 log statements

- [ ] Remove sensitive data from DEBUG logs (5 statements)
- [ ] Simplify signing debug logs (3 statements)
- [ ] Add structured format
- [ ] Ensure no credentials in logs
- [ ] Test with production log level

### Task 3: Update RefundService.cc logging
**Priority:** MEDIUM
**Files:** 1
**Estimated changes:** 8 log statements

- [ ] Upgrade refund processing to INFO (2 statements)
- [ ] Add refund completion log (1 statement)
- [ ] Add structured format
- [ ] Review error handling logs
- [ ] Test with production log level

### Task 4: Update other service files
**Priority:** MEDIUM
**Files:** CallbackService.cc, IdempotencyService.cc, ReconciliationService.cc
**Estimated changes:** 20 log statements

- [ ] Review and standardize log levels
- [ ] Add structured format
- [ ] Remove sensitive data
- [ ] Test with production log level

### Task 5: Update controller and filter logging
**Priority:** LOW
**Files:** PayController.cc, AlipayCallbackController.cc, PayAuthFilter.cc
**Estimated changes:** 10 log statements

- [ ] Standardize request logging
- [ ] Add structured format
- [ ] Review authentication logs
- [ ] Test with production log level

---

## 🧪 Testing Strategy

### 1. Development Environment Testing

**Log Level:** DEBUG
**Purpose:** Verify all logs work correctly
**Tests:**
- Create payment order
- Process refund
- Query order status
- Simulate API errors
- Verify log content and format

### 2. Production Environment Testing

**Log Level:** INFO
**Purpose:** Verify production logs are appropriate
**Tests:**
- Verify only important operations logged
- Check no sensitive data in logs
- Verify error logs are actionable
- Monitor log volume and performance

### 3. Performance Testing

**Purpose:** Ensure logging doesn't impact performance
**Tests:**
- Measure request latency with/without logging
- Test log rotation
- Monitor disk usage
- Check for hot path bottlenecks

---

## 📊 Success Criteria

### Quantitative Metrics

1. **Log Level Distribution:**
   - TRACE: 0-5% (optional detailed debugging)
   - DEBUG: 20-30% (development troubleshooting)
   - INFO: 40-50% (business operations)
   - WARN: 10-15% (recoverable issues)
   - ERROR: 5-10% (error conditions)

2. **Security:**
   - 0 instances of sensitive data in logs
   - 100% sanitization of payloads

3. **Performance:**
   - < 5% overhead from logging
   - No logging in critical hot paths

### Qualitative Metrics

1. **Operability:**
   - Clear, actionable error messages
   - Easy to trace request flow
   - Simple to diagnose issues

2. **Maintainability:**
   - Consistent log format
   - Self-documenting log messages
   - Easy to add new logs

---

## 🔄 Rollout Plan

### Phase 1: Core Services (Week 1)
- PaymentService.cc
- RefundService.cc
- AlipaySandboxClient.cc

### Phase 2: Supporting Services (Week 2)
- CallbackService.cc
- IdempotencyService.cc
- ReconciliationService.cc

### Phase 3: Controllers and Filters (Week 3)
- PayController.cc
- AlipayCallbackController.cc
- PayAuthFilter.cc

### Phase 4: Validation (Week 4)
- Review all changes
- Update documentation
- Create monitoring dashboards
- Team training

---

## 📝 Documentation Updates

1. **Update logging_standards.md** with final standards
2. **Add logging guide** to operations manual
3. **Create runbook** for common log analysis tasks
4. **Update on-call procedures** with log monitoring

---

## ✅ Approval Checklist

- [ ] Standards document reviewed
- [ ] Implementation plan approved
- [ ] Test cases defined
- [ ] Rollback plan prepared
- [ ] Team training scheduled
- [ ] Monitoring dashboards ready

---

**Owner:** Backend Development Team
**Review Date:** 2026-05-06
**Target Completion:** 2026-05-27
