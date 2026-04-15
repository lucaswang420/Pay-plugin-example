---
name: cpp-test-service
description: 将 Drogon 插件测试从旧插件 API 迁移到新的基于 Service 的架构
user-invocable: false
---

## Service API 迁移模式

将 Drogon 插件测试迁移到 Service API 时，遵循以下模式：

### 1. 替换直接插件调用为 Service 实例

**旧模式（插件 API）：**
```cpp
plugin->createPayment(request);
plugin->refund(request);
plugin->handleWechatCallback(body, signature);
plugin->queryOrder(orderNo);
plugin->reconcileSummary(startTime, endTime);
```

**新模式（Service API）：**
```cpp
paymentService->createPayment(request);
refundService->createRefund(request);
callbackService->handleCallback(body, signature);
paymentService->queryOrder(orderNo);
reconciliationService->reconcileSummary(startTime, endTime);
```

### 2. 使用 MockServiceFactory 而不是 MockPlugin

**旧模式：**
```cpp
auto mockPlugin = std::make_shared<MockPayPlugin>();
```

**新模式：**
```cpp
auto mockFactory = std::make_shared<MockServiceFactory>();
auto paymentService = mockFactory->getPaymentService();
auto refundService = mockFactory->getRefundService();
auto callbackService = mockFactory->getCallbackService();
```

### 3. 保持测试意图不变

**重要原则：**
- 保持测试名称不变
- 保持所有断言不变
- 保持边缘情况覆盖
- 只更新实现调用

### 4. 迁移后运行特定测试

```bash
# 从 PayBackend/ 目录运行
./build/Release/test_payplugin.exe --gtest_filter="*TestName*"

# 或运行特定测试套件
./build/Release/test_payplugin.exe --gtest_filter="*CreatePayment*"
./build/Release/test_payplugin.exe --gtest_filter="*Refund*"
./build/Release/test_payplugin.exe --gtest_filter="*Callback*"
```

## 迁移映射表

| 旧插件 API | 新 Service API | Service 类 |
|-----------|---------------|-----------|
| `plugin->createPayment()` | `paymentService->createPayment()` | PaymentService |
| `plugin->refund()` | `refundService->createRefund()` | RefundService |
| `plugin->handleWechatCallback()` | `callbackService->handleCallback()` | CallbackService |
| `plugin->queryOrder()` | `paymentService->queryOrder()` | PaymentService |
| `plugin->refundQuery()` | `refundService->queryRefund()` | RefundService |
| `plugin->reconcileSummary()` | `reconciliationService->reconcileSummary()` | ReconciliationService |

## 已完成的迁移示例

参考以下已迁移的测试文件：

1. **PayBackend/test/CreatePaymentIntegrationTest.cc**
   - 从 `plugin->createPayment()` 迁移到 `paymentService->createPayment()`
   - 使用 MockServiceFactory 设置测试环境

2. **PayBackend/test/QueryOrderTest.cc**
   - 从 `plugin->queryOrder()` 迁移到 `paymentService->queryOrder()`
   - 保持所有查询逻辑断言

3. **PayBackend/test/RefundQueryTest.cc**
   - 从 `plugin->refund()` 和 `plugin->refundQuery()` 迁移到 `refundService`
   - 保持退款流程验证

## 常见迁移模式

### 模式 1：Service 实例获取

```cpp
// 在测试设置中
void SetUp() override {
    mockFactory = std::make_shared<MockServiceFactory>();
    paymentService = mockFactory->getPaymentService();
    refundService = mockFactory->getRefundService();

    // 配置 mock 行为
    EXPECT_CALL(*paymentService, createPayment(_))
        .WillOnce(Return(expectedResponse));
}
```

### 模式 2：异步回调处理

```cpp
// 旧模式
plugin->handleWechatCallback(body, signature, [](const drogon::HttpResponsePtr& resp) {
    // 处理响应
});

// 新模式
callbackService->handleCallback(body, signature)
    .then([](const drogon::HttpResponsePtr& resp) {
        // 处理响应
    });
```

### 模式 3：错误处理

```cpp
// 保持错误检查逻辑不变
auto response = paymentService->createPayment(request);
ASSERT_NE(response, nullptr);
EXPECT_EQ(response->getStatusCode(), drogon::k200OK);
```

## 迁移检查清单

在完成测试迁移后，验证：

- [ ] 所有 `plugin->*` 调用已替换为 Service 调用
- [ ] MockPlugin 替换为 MockServiceFactory
- [ ] 测试名称保持不变
- [ ] 所有断言保留
- [ ] 测试编译通过
- [ ] 测试运行通过
- [ ] 边缘情况仍被覆盖

## 故障排除

### 问题：编译错误 - 找不到 PaymentService

**解决：** 确保包含正确的头文件：
```cpp
#include "services/PaymentService.h"
#include "services/RefundService.h"
#include "services/CallbackService.h"
```

### 问题：Mock 匹配失败

**解决：** 检查 mock 期望的参数匹配器：
```cpp
// 使用正确的参数匹配器
EXPECT_CALL(*paymentService, createPayment(_))
    .WillOnce(Return(response));

// 或使用具体值
EXPECT_CALL(*paymentService, createPayment(RequestMatcher(req)))
    .WillOnce(Return(response));
```

### 问题：测试超时

**解决：** Service API 可能使用异步模式，添加适当的等待：
```cpp
// 使用同步等待或回调
std::promise<drogon::HttpResponsePtr> p;
auto f = p.get_future();
callbackService->handleCallback(body, signature)
    .then([&p](const auto& resp) {
        p.set_value(resp);
    });
auto response = f.get();
```

## 相关文件

- 测试文件：`PayBackend/test/*.cc`
- Service 定义：`PayBackend/services/*.h`
- Mock 定义：检查测试文件中的 mock 类定义
- 构建脚本：`scripts/build.bat`
