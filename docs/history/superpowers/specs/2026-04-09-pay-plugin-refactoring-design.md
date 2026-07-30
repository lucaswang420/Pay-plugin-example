# Pay Plugin 重构设计文档

**日期：** 2026-04-09
**项目：** Pay-plugin-example
**重构类型：** 中等重构（方案 B）
**参考项目：** OAuth2-plugin-example

---

## 1. 概述

### 1.1 重构目标

对 Pay-plugin-example 项目进行中等规模重构，参考 OAuth2-plugin-example 的最佳实践，实现：

1. **代码组织优化**：添加 `services/` 层，分离业务逻辑
2. **目录结构规范化**：添加 `sql/`、完善 `docs/`、清理临时文件
3. **部署就绪**：添加 Docker、CI/CD、监控配置
4. **可维护性提升**：将 4000+ 行的 PayPlugin.cc 拆分为可维护的模块
5. **可测试性增强**：Service 层独立可测，易于单元测试

### 1.2 重构范围

**包含：**
- 创建 `services/` 层（5 个 Service 类）
- 简化 `PayPlugin` 为初始化层（~200 行）
- 更新 Controllers 使用依赖注入
- 添加数据库迁移脚本
- 完善文档（8 个新文档）
- 添加部署配置（Docker、GitHub Actions）

**不包含：**
- 不添加 `storage/` 抽象层（混合方案）
- 不实现多支付提供商抽象（保持微信支付专用）
- 不改变 API 接口（向后兼容）

---

## 2. 架构设计

### 2.1 重构前架构问题

```
Controller → Plugin (4000+ 行，所有逻辑)
                 ↓
         直接访问 DB + Redis + WechatPayClient
```

**问题：**
- PayPlugin.cc 职责过重：支付、退款、回调、对账、定时任务
- 业务逻辑与 Drogon 框架代码耦合
- 难以单元测试（必须启动 Drogon 框架）
- 代码复用性差

### 2.2 重构后架构

```
┌─────────────────────────────────────────────────────────┐
│                    Controllers Layer                     │
│  (PayController, WechatCallbackController, etc.)        │
└────────────────────┬────────────────────────────────────┘
                     │ HTTP 请求路由
                     ↓
┌─────────────────────────────────────────────────────────┐
│                     Plugin Layer                         │
│              (PayPlugin - 初始化层, ~200 行)              │
│  职责：初始化 Services、管理定时器、注册到 Drogon          │
└────────────────────┬────────────────────────────────────┘
                     │ 业务逻辑调用
                     ↓
┌─────────────────────────────────────────────────────────┐
│                    Services Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │PaymentService│  │ RefundService│  │CallbackService│  │
│  ├──────────────┤  ├──────────────┤  ├──────────────┤  │
│  │createPayment │  │  createRefund│  │ verifySignature│ │
│  │ queryOrder   │  │  queryRefund │  │ processCallback│ │
│  │ reconcile    │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                             │
│  ┌──────────────────────────────────┐                    │
│  │  ReconciliationService            │                    │
│  │  (定时任务：对账、状态同步)         │                    │
│  └──────────────────────────────────┘                    │
│                                                             │
│  ┌──────────────────────────────────┐                    │
│  │  IdempotencyService              │                    │
│  │  (幂等性：Redis + DB 双重保障)    │                    │
│  └──────────────────────────────────┘                    │
└────────────────────┬────────────────────────────────────┘
                     │ 数据访问
        ┌────────────┼────────────┐
        ↓            ↓            ↓
   ┌─────────┐  ┌─────────┐  ┌──────────┐
   │   DB    │  │  Redis  │  │WechatPay │
   │(Models) │  │Idempotency│  │  Client  │
   └─────────┘  └─────────┘  └──────────┘
```

### 2.3 调用链路

```
HTTP Request
    ↓
Controller (处理 HTTP 协议层)
    ↓
Service (业务逻辑层) ← 通过 Drogon 依赖注入获取
    ↓
Infrastructure (数据库、微信 API、Redis)
    ↑
Plugin (初始化层：创建 Services 并注册到 Drogon)
```

---

## 3. 目录结构设计

```
Pay-plugin-example/
├── PayBackend/
│   ├── controllers/          # 保持不变
│   │   ├── MetricsController.cc
│   │   ├── PayController.cc
│   │   ├── PayMetricsController.cc
│   │   └── WechatCallbackController.cc
│   │
│   ├── filters/              # 保持不变
│   │   ├── PayAuthFilter.cc
│   │   └── PayAuthMetrics.cc
│   │
│   ├── models/               # 保持不变（ORM 自动生成）
│   │   ├── PayOrder.cc/h
│   │   ├── PayPayment.cc/h
│   │   ├── PayRefund.cc/h
│   │   └── ...
│   │
│   ├── plugins/              # 简化为初始化层
│   │   ├── PayPlugin.cc      # 从 4000+ 行缩减到 ~200 行
│   │   └── PayPlugin.h
│   │
│   ├── services/             # 新增：业务逻辑层
│   │   ├── PaymentService.cc/h        # 支付创建、查询、对账
│   │   ├── RefundService.cc/h         # 退款创建、查询
│   │   ├── CallbackService.cc/h       # 回调验证、处理
│   │   ├── ReconciliationService.cc/h # 定时对账任务
│   │   └── IdempotencyService.cc/h    # 幂等性管理
│   │
│   ├── utils/                # 保持不变
│   │   └── PayUtils.cc/h
│   │
│   ├── sql/                  # 新增：数据库迁移脚本
│   │   ├── 001_init_pay_tables.sql
│   │   └── 002_add_indexes.sql
│   │
│   ├── scripts/              # 扩充
│   │   ├── generate_models.bat
│   │   ├── build.bat         # 从 OAuth2 移植
│   │   └── run_server.bat    # 从 OAuth2 移植
│   │
│   ├── test/                 # 扩充测试
│   │   ├── service/          # 新增：Service 单元测试
│   │   │   ├── PaymentServiceTest.cc
│   │   │   ├── RefundServiceTest.cc
│   │   │   ├── CallbackServiceTest.cc
│   │   │   ├── IdempotencyServiceTest.cc
│   │   │   └── MockHelpers.h
│   │   ├── integration/      # 现有集成测试
│   │   └── test_main.cc
│   │
│   ├── docs/                 # 扩充：从 2 个文件到 ~10 个
│   │   ├── api_reference.md
│   │   ├── architecture_overview.md
│   │   ├── payment_design.md（已存在）
│   │   ├── pay-api-examples.md（已存在）
│   │   ├── configuration_guide.md
│   │   ├── testing_guide.md
│   │   ├── deployment_guide.md
│   │   ├── migration_guide.md
│   │   ├── troubleshooting.md
│   │   └── service_architecture.md
│   │
│   ├── views/                # 保持不变
│   ├── uploads/              # 清理后只保留 uploads/tmp/
│   ├── main.cc
│   ├── CMakeLists.txt        # 更新：添加 services
│   ├── conanfile.txt
│   └── config.json
│
├── .github/                  # 新增：CI/CD 工作流
│   └── workflows/
│       ├── build.yml
│       └── test.yml
│
├── docker-compose.yml        # 新增：开发环境编排
├── Dockerfile                # 新增：生产环境镜像
├── prometheus.yml            # 新增：监控配置
├── README.md                 # 更新
├── LICENSE
└── AGENTS.md
```

---

## 4. Services 层设计

### 4.1 Service 接口定义

#### 4.1.1 PaymentService（支付服务）

**职责：** 处理支付订单的创建、查询、状态同步

**核心接口：**
```cpp
class PaymentService {
public:
    using PaymentCallback = std::function<void(const Json::Value& result, const std::error_code& error)>;

    PaymentService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        drogon::nosql::RedisClientPtr redisClient,
        std::shared_ptr<IdempotencyService> idempotencyService
    );

    // 创建支付订单
    void createPayment(
        const CreatePaymentRequest& request,
        const std::string& idempotencyKey,
        PaymentCallback&& callback
    );

    // 查询订单状态
    void queryOrder(
        const std::string& orderNo,
        PaymentCallback&& callback
    );

    // 从微信同步订单状态（对账使用）
    void syncOrderStatusFromWechat(
        const std::string& orderNo,
        std::function<void(const std::string& status)>&& callback
    );

    // 获取支付汇总（对账使用）
    void reconcileSummary(
        const std::string& date,
        PaymentCallback&& callback
    );

private:
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    drogon::nosql::RedisClientPtr redisClient_;
    std::shared_ptr<IdempotencyService> idempotencyService_;
};
```

#### 4.1.2 RefundService（退款服务）

**职责：** 处理退款订单的创建、查询、状态同步

**核心接口：**
```cpp
class RefundService {
public:
    using RefundCallback = std::function<void(const Json::Value& result, const std::error_code& error)>;

    RefundService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        std::shared_ptr<IdempotencyService> idempotencyService
    );

    // 创建退款
    void createRefund(
        const CreateRefundRequest& request,
        const std::string& idempotencyKey,
        RefundCallback&& callback
    );

    // 查询退款
    void queryRefund(
        const std::string& refundNo,
        RefundCallback&& callback
    );

    // 从微信同步退款状态（对账使用）
    void syncRefundStatusFromWechat(
        const std::string& refundNo,
        std::function<void(const std::string& status)>&& callback
    );

private:
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    std::shared_ptr<IdempotencyService> idempotencyService_;
};
```

#### 4.1.3 CallbackService（回调服务）

**职责：** 处理微信支付/退款回调，验证签名，更新订单状态

**核心接口：**
```cpp
class CallbackService {
public:
    using CallbackResult = std::function<void(const Json::Value& result, const std::error_code& error)>;

    CallbackService(
        std::shared_ptr<WechatPayClient> wechatClient,
        std::shared_ptr<drogon::orm::DbClient> dbClient
    );

    // 验证并处理支付回调
    void handlePaymentCallback(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo,
        CallbackResult&& callback
    );

    // 验证并处理退款回调
    void handleRefundCallback(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo,
        CallbackResult&& callback
    );

private:
    bool verifySignature(
        const std::string& body,
        const std::string& signature,
        const std::string& timestamp,
        const std::string& serialNo
    );

    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
};
```

#### 4.1.4 ReconciliationService（对账服务）

**职责：** 定时对账任务，同步订单状态

**核心接口：**
```cpp
class ReconciliationService {
public:
    ReconciliationService(
        std::shared_ptr<PaymentService> paymentService,
        std::shared_ptr<RefundService> refundService,
        std::shared_ptr<drogon::orm::DbClient> dbClient
    );

    // 启动对账定时器
    void startReconcileTimer();

    // 停止对账定时器
    void stopReconcileTimer();

    // 执行对账任务
    void reconcile(
        std::function<void(int success, int failed)>&& callback
    );

private:
    void syncPendingOrders();
    void syncPendingRefunds();

    std::shared_ptr<PaymentService> paymentService_;
    std::shared_ptr<RefundService> refundService_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    trantor::TimerId reconcileTimerId_;
    int reconcileIntervalSeconds_;
    int reconcileBatchSize_;
};
```

#### 4.1.5 IdempotencyService（幂等性服务）

**职责：** 统一管理幂等性检查，支持 Redis + DB 双重保障

**核心接口：**
```cpp
class IdempotencyService {
public:
    using CheckCallback = std::function<void(bool canProceed, const Json::Value& cachedResult)>;

    IdempotencyService(
        std::shared_ptr<drogon::orm::DbClient> dbClient,
        drogon::nosql::RedisClientPtr redisClient,
        int64_t ttlSeconds = 604800  // 默认 7 天
    );

    // 检查并设置幂等性
    void checkAndSet(
        const std::string& idempotencyKey,
        const std::string& requestHash,
        const Json::Value& request,
        CheckCallback&& callback
    );

    // 更新幂等性结果（业务逻辑执行完成后）
    void updateResult(
        const std::string& idempotencyKey,
        const Json::Value& response
    );

private:
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    drogon::nosql::RedisClientPtr redisClient_;
    int64_t ttlSeconds_;
};
```

### 4.2 Service 依赖注入示例

```cpp
// PaymentService 构造函数
PaymentService(
    std::shared_ptr<WechatPayClient> wechatClient,
    std::shared_ptr<drogon::orm::DbClient> dbClient,
    drogon::nosql::RedisClientPtr redisClient,
    std::shared_ptr<IdempotencyService> idempotencyService
);
```

---

## 5. Plugin 层设计

### 5.1 PayPlugin 简化后结构

**职责：**
- 读取配置文件
- 创建和初始化 Services
- 提供 Service 访问接口
- 启动后台定时任务（对账、证书刷新）
- 清理资源

**代码示例：**
```cpp
class PayPlugin : public drogon::Plugin<PayPlugin> {
public:
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    // Service 访问接口（供 Controller 使用）
    std::shared_ptr<PaymentService> paymentService() { return paymentService_; }
    std::shared_ptr<RefundService> refundService() { return refundService_; }
    std::shared_ptr<CallbackService> callbackService() { return callbackService_; }
    std::shared_ptr<IdempotencyService> idempotencyService() { return idempotencyService_; }

private:
    // Services（存储在 Plugin 中）
    std::shared_ptr<PaymentService> paymentService_;
    std::shared_ptr<RefundService> refundService_;
    std::shared_ptr<CallbackService> callbackService_;
    std::unique_ptr<ReconciliationService> reconciliationService_;
    std::shared_ptr<IdempotencyService> idempotencyService_;

    // 基础设施
    std::shared_ptr<WechatPayClient> wechatClient_;
    std::shared_ptr<drogon::orm::DbClient> dbClient_;
    drogon::nosql::RedisClientPtr redisClient_;

    // 定时器
    trantor::TimerId certRefreshTimerId_;

    void startCertRefreshTimer();
};
```

### 5.2 Plugin 初始化流程

```cpp
void PayPlugin::initAndStart(const Json::Value &config) {
    // 1. 获取基础设施
    dbClient_ = app().getDbClient();
    redisClient_ = app().getRedisClient();

    // 2. 创建 WechatPayClient
    wechatClient_ = std::make_shared<WechatPayClient>(config["wechat"]);

    // 3. 创建 IdempotencyService（基础服务，无依赖）
    idempotencyService_ = std::make_shared<IdempotencyService>(
        dbClient_, redisClient_, config["idempotency"].get("ttl", 604800).asInt64()
    );

    // 4. 创建业务 Services（依赖 IdempotencyService）
    paymentService_ = std::make_shared<PaymentService>(
        wechatClient_, dbClient_, redisClient_, idempotencyService_
    );

    refundService_ = std::make_shared<RefundService>(
        wechatClient_, dbClient_, idempotencyService_
    );

    callbackService_ = std::make_shared<CallbackService>(
        wechatClient_, dbClient_
    );

    // 5. 创建并启动对账服务（依赖 PaymentService 和 RefundService）
    reconciliationService_ = std::make_unique<ReconciliationService>(
        paymentService_, refundService_, dbClient_, config
    );
    reconciliationService_->startReconcileTimer();

    // 6. 启动证书刷新定时器
    startCertRefreshTimer();
}

void PayPlugin::shutdown() {
    // 停止定时器
    if (reconciliationService_) {
        reconciliationService_->stopReconcileTimer();
    }
    // 清理其他资源
}
```

---

## 6. Controller 层更新

### 6.1 Controller 获取 Service

**代码示例：**
```cpp
// PayController.cc
void PayController::createPayment(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback)
{
    // 1. 提取参数
    auto json = req->getJsonObject();
    CreatePaymentRequest request;
    request.amount = (*json)["amount"].asString();
    // ... 其他参数

    int64_t userId = req->attributes()->get<int64_t>("user_id");
    std::string idempotencyKey = req->getHeader("X-Idempotency-Key");

    // 2. 从 Plugin 获取 Service（依赖注入）
    auto plugin = app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();

    // 3. 调用 Service 业务逻辑
    paymentService->createPayment(
        request, idempotencyKey,
        [this, callback](const Json::Value& result, const std::error_code& error) {
            // 4. 构建响应
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error) {
                resp->setStatusCode(k500InternalServerError);
                LOG_ERROR << "Payment creation failed: " << error.message();
            }
            callback(resp);
        }
    );
}
```

---

## 7. 数据流设计

### 7.1 创建支付订单流程

```
Client                      Controller              PaymentService              Infrastructure
  │                              │                          │                          │
  │ POST /api/pay/create         │                          │                          │
  ├─────────────────────────────>│                          │                          │
  │                              │                          │                          │
  │                              │ 1. checkIdempotency()    │                          │
  │                              ├──────────────────────────>│                          │
  │                              │                          │                          │
  │                              │                          │ Redis.get(idempotencyKey)│
  │                              │                          ├──────────────────────────>│
  │                              │                          │<──────────────────────────┤
  │                              │                          │                          │
  │                              │ 2. 不存在，创建订单        │                          │
  │                              │                          │                          │
  │                              │                          │ WechatClient.createOrder()│
  │                              │                          ├──────────────────────────>│
  │                              │                          │<──────────────────────────┤
  │                              │                          │ (prepay_id)               │
  │                              │                          │                          │
  │                              │                          │ 3. DbClient.insert(order) │
  │                              │                          ├──────────────────────────>│
  │                              │                          │<──────────────────────────┤
  │                              │                          │                          │
  │                              │                          │ 4. Redis.set(idempotencyKey, result, TTL)│
  │                              │                          ├──────────────────────────>│
  │                              │                          │                          │
  │                              │ 5. callback(result)      │                          │
  │                              │<─────────────────────────┤                          │
  │                              │                          │                          │
  │ HTTP 200 {code:0, data:{...}}│                          │                          │
  │<─────────────────────────────┤                          │                          │
```

### 7.2 幂等性处理机制

**策略：Redis + DB 双重保障**

1. 第一步：检查 Redis（快速路径）
2. 第二步：检查数据库（防止 Redis 数据丢失）
3. 第三步：执行业务逻辑

### 7.3 回调处理流程

```
WeChat Server                Controller             CallbackService              Infrastructure
  │                               │                       │                          │
  │ POST /api/wechat/callback     │                       │                          │
  ├──────────────────────────────>│                       │                          │
  │ {resource: {cipher_text: ...}}│                       │                          │
  │                               │ 1. verifySignature()  │                          │
  │                               ├──────────────────────>│                          │
  │                               │                       │ WechatClient.verify()    │
  │                               │                       ├──────────────────────────>│
  │                               │                       │<──────────────────────────┤
  │                               │ 2. decryptBody()       │                          │
  │                               ├──────────────────────>│                          │
  │                               │                       │ WechatClient.decrypt()   │
  │                               │                       ├──────────────────────────>│
  │                               │                       │<──────────────────────────┤
  │                               │ 3. updateOrderStatus() │                          │
  │                               ├──────────────────────>│                          │
  │                               │                       │ DbClient.update(...)     │
  │                               │                       ├──────────────────────────>│
  │                               │ 4. insertCallbackLog()│                          │
  │                               ├──────────────────────>│                          │
  │                               │                       │ DbClient.insert(...)     │
  │ HTTP 200 {code: "SUCCESS"}    │                       │                          │
  │<──────────────────────────────┤                       │                          │
```

---

## 8. 错误处理设计

### 8.1 错误分类

```cpp
enum class PaymentError {
    // 业务错误（4xx）
    INVALID_AMOUNT = 1001,        // 金额无效
    ORDER_NOT_FOUND = 1002,       // 订单不存在
    ORDER_ALREADY_PAID = 1003,    // 订单已支付
    IDEMPOTENCY_CONFLICT = 1004,  // 幂等性冲突（请求参数不同）

    // 第三方错误（5xx）
    WECHAT_PAY_ERROR = 2001,      // 微信支付错误
    WECHAT_VERIFY_ERROR = 2002,   // 微信签名验证失败

    // 系统错误（5xx）
    DATABASE_ERROR = 3001,        // 数据库错误
    REDIS_ERROR = 3002,           // Redis 错误
};
```

### 8.2 统一错误响应格式

```cpp
// 成功响应
{
    "code": 0,
    "message": "success",
    "data": { ... }
}

// 错误响应
{
    "code": 1001,
    "message": "Invalid amount: must be greater than 0",
    "error_detail": "Amount: -100 is not valid"
}
```

### 8.3 Service 层错误处理

```cpp
void PaymentService::createPayment(..., callback) {
    try {
        // 参数验证
        if (amount <= 0) {
            Json::Value error;
            error["code"] = (int)PaymentError::INVALID_AMOUNT;
            error["message"] = "Invalid amount: must be greater than 0";
            callback(error, std::make_error_code(std::errc::invalid_argument));
            return;
        }

        // 调用微信 API
        wechatClient_->createOrder(..., [callback](auto result) {
            if (result.hasError()) {
                Json::Value error;
                error["code"] = (int)PaymentError::WECHAT_PAY_ERROR;
                error["message"] = "WeChat Pay API error";
                callback(error, std::make_error_code(std::errc::operation_canceled));
                return;
            }

            // 成功处理
            Json::Value response;
            response["code"] = 0;
            response["data"] = result.data();
            callback(response, std::error_code());
        });

    } catch (const std::exception& e) {
        Json::Value error;
        error["code"] = (int)PaymentError::DATABASE_ERROR;
        error["message"] = "Internal server error";
        callback(error, std::make_error_code(std::errc::io_error));
    }
}
```

---

## 9. 事务管理策略

**原则：** 每个 Service 方法内部独立管理事务，不跨 Service 传递事务对象。

**示例：**
```cpp
void PaymentService::createPayment(..., callback) {
    // Service 内部使用事务
    dbClient_->execSqlAsync(
        "BEGIN",
        [this, request, callback](const Result&) {
            // 1. 创建订单
            // 2. 创建支付记录
            // 3. 插入账本
            // 全部成功，提交事务
            dbClient_->execSqlAsync("COMMIT", [callback](const Result&) {
                Json::Value response;
                response["code"] = 0;
                callback(response, std::error_code());
            });
        },
        [callback](const DrogonDbException& e) {
            // 失败，回滚
            dbClient_->execSqlAsync("ROLLBACK", [](...) {});
            Json::Value error;
            error["code"] = (int)PaymentError::DATABASE_ERROR;
            callback(error, std::make_error_code(std::errc::io_error));
        }
    );
}
```

---

## 10. 测试策略

### 10.1 测试层次

```
┌─────────────────────────────────────────────────────────┐
│                    Integration Tests                     │
│  (现有测试保持：WechatCallbackIntegrationTest, etc.)     │
└─────────────────────────────────────────────────────────┘
                          ↑
┌─────────────────────────────────────────────────────────┐
│                  Service Unit Tests                     │
│  (新增：PaymentServiceTest, RefundServiceTest, etc.)    │
└─────────────────────────────────────────────────────────┘
                          ↑
┌─────────────────────────────────────────────────────────┐
│                    Mock Objects                         │
│  (MockWechatPayClient, MockDbClient, MockRedisClient)  │
└─────────────────────────────────────────────────────────┘
```

### 10.2 测试文件结构

```
test/
├── service/                  # 新增：Service 单元测试
│   ├── PaymentServiceTest.cc
│   ├── RefundServiceTest.cc
│   ├── CallbackServiceTest.cc
│   ├── IdempotencyServiceTest.cc
│   └── MockHelpers.h        # Mock 对象定义
├── integration/              # 现有集成测试（保持不变）
│   ├── WechatCallbackIntegrationTest.cc
│   ├── CreatePaymentIntegrationTest.cc
│   └── ...
└── test_main.cc
```

### 10.3 Service 单元测试示例

```cpp
TEST_F(PaymentServiceTest, CreatePayment_Success) {
    // Arrange
    CreatePaymentRequest request;
    request.amount = "10000";  // 100元
    request.orderNo = "TEST001";

    EXPECT_CALL(*mockWechatClient_, createOrder(_, _))
        .WillOnce([](auto req, auto callback) {
            Json::Value response;
            response["prepay_id"] = "wx_prepay_123";
            callback(response);
        });

    // Act
    service_->createPayment(request, [](const Json::Value& result, const std::error_code& error) {
        // Assert
        ASSERT_FALSE(error);
        EXPECT_EQ(result["code"].asInt(), 0);
        EXPECT_EQ(result["data"]["prepay_id"].asString(), "wx_prepay_123");
    });
}
```

---

## 11. 文档设计

### 11.1 文档结构

```
docs/
├── api_reference.md           # API 接口文档（新增）
├── architecture_overview.md   # 架构总览（新增）
├── payment_design.md          # 支付设计（已存在，保持）
├── pay-api-examples.md        # API 示例（已存在，保持）
├── configuration_guide.md     # 配置指南（新增）
├── testing_guide.md           # 测试指南（新增）
├── deployment_guide.md        # 部署指南（新增）
├── migration_guide.md         # 重构迁移指南（新增）
├── troubleshooting.md         # 故障排查（新增）
└── service_architecture.md    # Service 层设计（新增）
```

### 11.2 关键文档内容

**architecture_overview.md**
- 整体架构图
- 分层说明
- 调用链路
- 与 OAuth2-plugin 的对比

**migration_guide.md**
- 重构步骤
- 代码迁移对照表
- 测试迁移指南
- 常见问题

**testing_guide.md**
- 测试策略
- 如何编写 Service 单元测试
- 如何编写集成测试
- Mock 对象使用指南

---

## 12. 部署配置设计

### 12.1 Docker 配置

**docker-compose.yml（开发环境）**
```yaml
version: '3.8'
services:
  postgres:
    image: postgres:14
    environment:
      POSTGRES_DB: pay_test
      POSTGRES_USER: pay_user
      POSTGRES_PASSWORD: pay_pass
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./PayBackend/sql:/docker-entrypoint-initdb.d

  redis:
    image: redis:7
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data

  prometheus:
    image: prom/prometheus
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana
    ports:
      - "3000:3000"
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin

volumes:
  postgres_data:
  redis_data:
```

**Dockerfile（生产环境）**
```dockerfile
FROM gcc:11 AS builder
WORKDIR /build
COPY . .
RUN ./build.sh

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libpq-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/PayBackend /app/PayBackend
COPY PayBackend/config.json /app/config.json

EXPOSE 5555
CMD ["/app/PayBackend/PayBackend"]
```

### 12.2 GitHub Actions CI/CD

```yaml
# .github/workflows/build.yml
name: Build and Test
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container: gcc:11

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies
      run: |
        apt-get update
        apt-get install -y cmake libpq-dev libssl-dev

    - name: Build
      run: |
        cd PayBackend
        ./build.sh

    - name: Run tests
      run: |
        cd PayBackend/build
        ./PayBackendTest
```

---

## 13. 实施计划

### 13.1 分阶段实施

```
阶段 1: 基础设施准备      [1-2 天]
   ↓
阶段 2: Services 层实现    [2-3 天]
   ↓
阶段 3: Plugin 重构        [1 天]
   ↓
阶段 4: Controller 更新    [1 天]
   ↓
阶段 5: 测试迁移与补充    [1-2 天]
   ↓
阶段 6: 文档与部署配置    [1 天]
   ↓
阶段 7: 验证与优化        [1 天]
```

**总计：8-11 天**

### 13.2 各阶段详细任务

#### **阶段 1：基础设施准备 [1-2 天]**

**任务：**
1. 创建 `services/` 目录
2. 创建 `sql/` 目录，迁移数据库初始化脚本
3. 清理 `uploads/tmp/00-FF` 临时目录
4. 更新 `CMakeLists.txt`，添加 services 到构建
5. 创建 `scripts/build.bat` 和 `scripts/run_server.bat`

**验收标准：**
- 目录结构符合设计
- 项目可正常编译
- 现有功能不受影响

#### **阶段 2：Services 层实现 [2-3 天]**

**任务：**
1. 实现 `IdempotencyService`（基础服务，其他 Service 依赖）
2. 实现 `PaymentService`
3. 实现 `RefundService`
4. 实现 `CallbackService`
5. 实现 `ReconciliationService`

**验收标准：**
- 所有 Service 编译通过
- Service 单元测试通过（新增）
- 业务逻辑与重构前一致

#### **阶段 3：Plugin 重构 [1 天]**

**任务：**
1. 简化 `PayPlugin::initAndStart()`
2. 简化 `PayPlugin::shutdown()`
3. 删除 `PayPlugin` 中的业务逻辑方法
4. 保留必要的生命周期管理代码

**验收标准：**
- PayPlugin.cc 从 4000+ 行缩减到 ~200 行
- Plugin 正确注册所有 Services
- 应用正常启动和关闭

#### **阶段 4：Controller 更新 [1 天]**

**任务：**
1. 更新 `PayController`
2. 更新 `WechatCallbackController`
3. 更新其他 Controllers（如果有）

**验收标准：**
- 所有 Controllers 编译通过
- HTTP 请求正确路由到 Services
- 现有集成测试通过

#### **阶段 5：测试迁移与补充 [1-2 天]**

**任务：**
1. 创建 `test/service/` 目录
2. 编写 Service 单元测试
3. 创建 Mock 对象
4. 验证现有集成测试仍然通过

**验收标准：**
- Service 单元测试覆盖率 > 80%
- 现有集成测试全部通过
- 无回归问题

#### **阶段 6：文档与部署配置 [1 天]**

**任务：**
1. 编写文档（6 个新文档）
2. 创建部署配置
3. 创建 GitHub Actions

**验收标准：**
- 文档完整且准确
- Docker 环境可正常启动
- CI/CD 流程正常运行

#### **阶段 7：验证与优化 [1 天]**

**任务：**
1. 端到端测试
2. 性能测试（对比重构前后）
3. 代码审查和优化
4. 更新 README.md

**验收标准：**
- 所有功能正常
- 性能无明显下降
- 代码符合规范

### 13.3 风险与缓解措施

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 业务逻辑遗漏 | 高 | 逐步迁移，每阶段运行集成测试验证 |
| 编译错误 | 中 | 阶段 1 验证构建系统 |
| 测试覆盖不足 | 中 | 优先编写关键路径测试 |
| 性能下降 | 低 | 阶段 7 性能对比测试 |

---

## 14. 设计原则

### 14.1 关键设计原则

1. **单向依赖**：Controller → Plugin → Service → Infrastructure
2. **Service 独立性**：每个 Service 可独立测试，不依赖 Drogon
3. **依赖注入**：Plugin 创建 Service 时注入 DbClient、RedisClient、WechatPayClient
4. **接口清晰**：每个 Service 提供明确的异步接口（回调方式）
5. **事务边界**：每个 Service 方法内部管理事务
6. **错误分类**：业务错误、第三方错误、系统错误明确区分

### 14.2 设计决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| Service 访问方式 | Plugin 提供访问接口 | Drogon 框架标准模式，符合 Plugin 设计初衷 |
| 支付提供商抽象 | 不添加（保持微信专用） | 开发/演示阶段，优先进度，未来可重构 |
| 幂等性服务 | 独立 IdempotencyService | 复用性高，职责单一 |
| 事务管理 | Service 内部管理 | 简化设计，边界清晰 |
| storage 抽象层 | 不添加 | 混合方案，当前不需要多存储后端 |

---

## 15. 验收标准

### 15.1 功能验收

- [ ] 所有现有功能正常工作
- [ ] 所有集成测试通过
- [ ] Service 单元测试覆盖率 > 80%
- [ ] 无明显性能下降

### 15.2 代码质量验收

- [ ] PayPlugin.cc 从 4000+ 行缩减到 ~200 行
- [ ] 所有 Services 编译通过，无警告
- [ ] 代码符合项目规范
- [ ] 通过 Code Review

### 15.3 文档验收

- [ ] 所有新增文档完整且准确
- [ ] README.md 更新
- [ ] API 文档完整

### 15.4 部署验收

- [ ] Docker 环境可正常启动
- [ ] CI/CD 流程正常运行
- [ ] 监控配置正常工作

---

## 16. 附录

### 16.1 术语表

- **Plugin**：Drogon 插件，负责初始化和生命周期管理
- **Service**：业务逻辑层，处理核心业务
- **Controller**：HTTP 路由层，处理请求/响应
- **幂等性**：相同请求多次执行产生相同结果
- **对账**：定时同步订单状态，确保数据一致性

### 16.2 参考资料

- [Drogon Framework 文档](https://github.com/drogonframework/drogon)
- [OAuth2-plugin-example 项目](D:\work\development\Repos\backend\drogon-plugin\OAuth2-plugin-example)
- [微信支付开发文档](https://pay.weixin.qq.com/wiki/doc/apiv3/index.shtml)

---

**文档版本：** 1.0
**最后更新：** 2026-04-09
**状态：** 待审批
