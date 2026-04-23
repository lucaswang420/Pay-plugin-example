# 测试更新进度跟踪

**创建时间：** 2026-04-13
**目标：** 方案B - 质量优先（4-6周，测试覆盖率≥80%）
**当前阶段：** 阶段1 - 核心集成测试

---

## 📊 总体进度

| 测试文件 | 旧API调用数 | 已更新 | 状态 | 预计时间 |
|---------|-----------|-------|------|---------|
| **RefundQueryTest.cc** | 19 | 0 | ⏳ 进行中 | 4-6h |
| **CreatePaymentIntegrationTest.cc** | 4 | 0 | ⏳ 待开始 | 1-2h |
| **WechatCallbackIntegrationTest.cc** | 40 | 0 | ⏳ 待开始 | 3-4h |
| **ReconcileSummaryTest.cc** | 少量 | 0 | ⏳ 待开始 | 1h |
| **总计** | **~63** | **0** | **⏳ 0%** | **8-13h** |

---

## 🔄 API更新模式

### 旧API → 新API 映射

#### 1. refund() → RefundService::createRefund()

**旧代码：**
```cpp
Json::Value payload;
payload["order_no"] = orderNo;
payload["payment_no"] = paymentNo;
payload["amount"] = amount;
payload["reason"] = "Test reason";  // 可选
payload["notify_url"] = "https://notify.refund";  // 可选
payload["funds_account"] = "AVAILABLE";  // 可选
const std::string body = pay::utils::toJsonString(payload);

auto req = drogon::HttpRequest::newHttpRequest();
req->setMethod(drogon::Post);
req->setBody(body);
req->addHeader("Idempotency-Key", idempotencyKey);

std::promise<drogon::HttpResponsePtr> promise;
plugin.refund(
    req,
    [&promise](const drogon::HttpResponsePtr &resp) {
        promise.set_value(resp);
    });

auto future = promise.get_future();
const auto resp = future.get();
CHECK(resp->statusCode() == drogon::k200OK);
auto respJson = resp->getJsonObject();
CHECK((*respJson)["refund_no"].asString() == refundNo);
```

**新代码：**
```cpp
CreateRefundRequest request;
request.orderNo = orderNo;
request.paymentNo = paymentNo;
request.amount = amount;
request.refundNo = "";  // 留空自动生成
request.reason = "Test reason";  // 可选
request.notifyUrl = "https://notify.refund";  // 可选
request.fundsAccount = "AVAILABLE";  // 可选

std::promise<Json::Value> resultPromise;
std::promise<std::error_code> errorPromise;

auto refundService = plugin.refundService();
refundService->createRefund(
    request,
    idempotencyKey,
    [&resultPromise, &errorPromise](const Json::Value& result, const std::error_code& error) {
        resultPromise.set_value(result);
        errorPromise.set_value(error);
    });

auto resultFuture = resultPromise.get_future();
auto errorFuture = errorPromise.get_future();

const auto error = errorFuture.get();
CHECK(!error);
CHECK(error.message().empty());

const auto result = resultFuture.get();
CHECK(result.isMember("data"));
CHECK(result["data"]["refund_no"].asString() == refundNo);
```

**关键变化：**
1. ✅ 不再需要构造 `HttpRequest`
2. ✅ 使用 `CreateRefundRequest` 结构体
3. ✅ Idempotency-Key 作为单独参数传递
4. ✅ 回调返回 `Json::Value` 和 `std::error_code`
5. ✅ 响应数据在 `result["data"]` 中

---

#### 2. createPayment() → PaymentService::createPayment()

**旧代码：**
```cpp
auto req = drogon::HttpRequest::newHttpRequest();
req->setMethod(drogon::Post);
req->setBody(body);
req->addHeader("Idempotency-Key", idempotencyKey);

plugin.createPayment(req, callback);
```

**新代码：**
```cpp
CreatePaymentRequest request;
request.userId = "10001";
request.amount = "9.99";
request.currency = "CNY";
request.title = "Test Order";

auto paymentService = plugin.paymentService();
paymentService->createPayment(
    request,
    idempotencyKey,
    callback
);
```

---

#### 3. handleWechatCallback() → CallbackService::handlePaymentCallback()

**旧代码：**
```cpp
plugin.handleWechatCallback(
    req,
    [&promise](const drogon::HttpResponsePtr &resp) {
        promise.set_value(resp);
    });
```

**新代码：**
```cpp
auto callbackService = plugin.callbackService();
callbackService->handlePaymentCallback(
    req->body(),
    req->getHeader("Wechatpay-Signature"),
    req->getHeader("Wechatpay-Timestamp"),
    req->getHeader("Wechatpay-Nonce"),
    req->getHeader("Wechatpay-Serial"),
    [&resultPromise, &errorPromise](const Json::Value& result, const std::error_code& error) {
        resultPromise.set_value(result);
        errorPromise.set_value(error);
    }
);
```

---

## 📋 RefundQueryTest.cc 更新清单

### 需要更新的测试（plugin.refund()调用）

| 行号 | 测试名称 | payload字段 | 优先级 | 状态 |
|------|---------|-----------|-------|------|
| 439 | PayPlugin_Refund_IdempotencyConflict | order_no, amount | P0 | ⏳ |
| 512 | PayPlugin_Refund_IdempotencySnapshot | order_no, amount | P0 | ⏳ |
| 598 | PayPlugin_Refund_NoWechatClient | order_no, payment_no, amount | P0 | ⏳ |
| 786 | PayPlugin_Refund_WechatError | order_no, payment_no, amount | P0 | ⏳ |
| 786 | PayPlugin_Refund_WechatPayloadExtras | order_no, payment_no, amount, reason, notify_url, funds_account | P0 | ✅ 完成 |
| 939 | PayPlugin_Refund_WechatSuccess | order_no, payment_no, amount | P0 | ⏳ |
| 1093 | PayPlugin_Refund_WechatProcessing | order_no, payment_no, amount | P0 | ⏳ |
| 1256 | PayPlugin_Refund_PartialRefund | order_no, payment_no, amount | P1 | ⏳ |
| 1273 | PayPlugin_Refund_MultipleRefunds | order_no, payment_no, amount | P1 | ⏳ |
| 1431 | PayPlugin_Refund_OrderNotFound | order_no, payment_no, amount | P1 | ⏳ |
| 1449 | PayPlugin_Refund_PaymentNotFound | order_no, payment_no, amount | P1 | ⏳ |
| 1619 | PayPlugin_Refund_AmountMismatch | order_no, payment_no, amount | P1 | ⏳ |
| 1739 | PayPlugin_Refund_CurrencyMismatch | order_no, payment_no, amount | P1 | ⏳ |
| 1842 | PayPlugin_Refund_OrderNotPaid | order_no, payment_no, amount | P1 | ⏳ |
| 1969 | PayPlugin_Refund_RefundExceedsPayment | order_no, payment_no, amount | P1 | ⏳ |
| 2110 | PayPlugin_Refund_InProgressAlready | order_no, payment_no, amount | P1 | ⏳ |
| 2254 | PayPlugin_Refund_InvalidAmountFormat | order_no, payment_no, amount | P1 | ⏳ |
| 2310 | PayPlugin_Refund_MissingOrderNo | order_no | P2 | ⏳ |
| 2371 | PayPlugin_Refund_MissingPaymentNo | payment_no, amount | P2 | ⏳ |
| 2416 | PayPlugin_Refund_MissingAmount | order_no, payment_no | P2 | ⏳ |

**总计：** 19个测试

---

## ✅ 完成的更新

### RefundQueryTest.cc

暂无

### CreatePaymentIntegrationTest.cc

暂无

### WechatCallbackIntegrationTest.cc

暂无

---

## 🚀 执行计划

### Week 1: 核心测试（P0优先级）

**Day 1-2: RefundQueryTest.cc (P0测试)**
- [ ] PayPlugin_Refund_IdempotencyConflict
- [ ] PayPlugin_Refund_IdempotencySnapshot
- [ ] PayPlugin_Refund_NoWechatClient
- [ ] PayPlugin_Refund_WechatError
- [ ] PayPlugin_Refund_WechatSuccess
- [ ] PayPlugin_Refund_WechatProcessing

**Day 3: CreatePaymentIntegrationTest.cc**
- [ ] 所有4个createPayment测试

**Day 4-5: 编译验证和修复**
- [ ] 编译所有测试
- [ ] 修复编译错误
- [ ] 运行测试验证

### Week 2: 边缘情况测试（P1优先级）

**Day 1-2: RefundQueryTest.cc (P1测试)**
- [ ] 剩余13个refund测试

**Day 3-5: WechatCallbackIntegrationTest.cc (P0测试)**
- [ ] 优先处理支付成功回调测试
- [ ] 然后处理退款成功回调测试

### Week 3: 回调和边缘情况（P1-P2）

**Day 1-3: WechatCallbackIntegrationTest.cc (继续)**
- [ ] 完成所有40个回调测试

**Day 4-5: ReconcileSummaryTest.cc**
- [ ] 更新对账测试

---

## 🔧 实用技巧

### 1. 快速定位测试

```bash
# 查找所有旧API调用
grep -n "plugin\.refund(" test/RefundQueryTest.cc
grep -n "plugin\.createPayment(" test/CreatePaymentIntegrationTest.cc
grep -n "plugin\.handleWechatCallback(" test/WechatCallbackIntegrationTest.cc
```

### 2. 批量编译检查

```bash
cd PayBackend
scripts/build.bat 2>&1 | grep "error C2039"
```

### 3. 运行单个测试

```bash
./build/test/Release/PayBackendTests --gtest_filter="PayPlugin_Refund_IdempotencyConflict"
```

---

## 📝 更新模板

### 标准更新步骤

1. **读取旧测试代码**
2. **提取payload字段**
3. **构造CreateRefundRequest**
4. **替换API调用**
5. **更新断言**
6. **编译验证**
7. **运行测试**

### 常见问题

**Q: payload字段不完整怎么办？**
A: 只需要提供必需字段（order_no, payment_no, amount），其他字段留空或使用默认值。

**Q: 如何处理Idempotency-Key？**
A: 从HTTP请求头提取，作为createRefund的第二个参数。

**Q: 响应格式变化了？**
A: 旧API响应在根级别，新API响应在`result["data"]`中。

**Q: 如何处理错误？**
A: 新API通过`std::error_code`返回错误，检查`!error`判断成功。

---

## 🎯 里程碑

- [ ] **M1: Refund P0测试完成** (Day 2)
- [ ] **M2: CreatePayment测试完成** (Day 3)
- [ ] **M3: 编译零错误** (Day 5)
- [ ] **M4: 所有refund测试完成** (Week 2)
- [ ] **M5: 所有createPayment测试完成** (Week 2)
- [ ] **M6: 所有callback测试完成** (Week 3)
- [ ] **M7: 阶段1完成** (Week 3)

---

**文档更新频率：** 每完成一个测试更新一次
**下次更新：** 完成第一个refund测试后
