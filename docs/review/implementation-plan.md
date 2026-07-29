# Pay Plugin 实施计划：缺陷修复 + 测试补全

> **基于**: deep-code-review-report.md (2026-07-29)  
> **目标**: 修复全部 17 项缺陷 + 补全 25 个测试用例，达到 100% 测试覆盖率  
> **预计工期**: 3 周

---

## 概览

| Phase | 内容 | 工作量 | 状态 |
|-------|------|--------|------|
| P1-修复 | MAJOR 缺陷 ×8 | ~5 天 | ⬜ 待开始 |
| P2-修复 | MINOR 缺陷 ×5 (1 项推迟) | ~2 天 | ⬜ 待开始 |
| P3-修复 | NIT 缺陷 ×2 (1 项无需修复) | ~1 天 | ⬜ 待开始 |
| P4-测试 | P0 安全关键测试 ×3 | ~2 天 | ⬜ 待开始 |
| P5-测试 | P1 核心业务测试 ×6 | ~2.5 天 | ⬜ 待开始 |
| P6-测试 | P2 边界条件测试 ×9 | ~3 天 | ⬜ 待开始 |
| P7-测试 | P3 完善性测试 ×6 | ~2 天 | ⬜ 待开始 |
| 推迟 | PaymentService 文件拆分 | 下迭代 | 🔵 技术债务 |

---

## Phase 1: MAJOR 缺陷修复 (8 项)

### 1.1 [MAJOR] A1-1 — createPayment 无事务保护

**影响文件**: `PayBackend/services/PaymentService.cc`  
**问题**: `proceedCreatePayment` 中 PayOrder INSERT 和 PayPayment INSERT 分两次独立调用，payment 写入失败时 order 已遗留。  
**风险**: 孤立订单无支付记录

**修复方案** (✅ 已确认):
1. **缩小范围**: 仅包裹 PayOrder INSERT + PayPayment INSERT 进入事务，渠道调用（微信/支付宝 API）保持在事务外
2. 参照 `RefundService::proceedWithInsert()` 的事务模式，但不包含外部渠道调用
3. 事务内：INSERT PayOrder → INSERT PayPayment → COMMIT；任一步失败则 ROLLBACK
4. `createQRPayment` 中也有同样模式的分步 INSERT，需要一并修改

**涉及测试用例**: P1-11 (DB 事务失败回滚测试)

---

### 1.2 [MAJOR] A1-2 — createQRPayment 初始状态不一致

**影响文件**: `PayBackend/services/PaymentService.cc`、`TECH_SPECS.md`  
**问题**: `/api/pay/create` 初始状态为 `"CREATED"` (line 445)，`/api/qrpay/create` 初始状态为 `"PAYING"` (line 1157)  
**风险**: 对账和状态查询不一致

**修复方案** (✅ 已确认 — 方案 B):
1. 保留两种路径的状态差异化（不统一修改代码）
2. 在 `TECH_SPECS.md` 中新增"订单状态机"章节，明确文档化:
   - `/api/pay/create`: 初始状态 = `CREATED`，支付完成后 → `SUCCESS`/`FAILED`
   - `/api/qrpay/create`: 初始状态 = `PAYING`，支付完成后 → `SUCCESS`/`FAILED`
   - 两种路径的合法状态转换图
3. 在对账和状态查询逻辑中确认两种初始状态均被正确处理

**涉及测试用例**: 无需修改现有测试，需新增状态机一致性文档验证测试 (P3-6)

---

### 1.3 [MAJOR] A1-3 — queryOrder 通道失败返回 code=0

**影响文件**: `PayBackend/services/PaymentService.cc` (line 1256-1409)  
**问题**: 通道查询失败时 `(*sharedCb)(innerResponse, std::error_code())`，HTTP 200 + code=0  
**风险**: 客户端需解析 `data.wechat_query_error` 才能检测降级，易被忽略

**修复方案**:
1. 通道查询失败时返回 `code=1`（带降级数据）而非 `code=0`
2. 对应的 JSON 响应中 `"code": 1` 明确表示"数据可能不完整"
3. 在 `PayController.queryOrder` 中 `mapErrorToHttpStatus` 处理 code=1 为 HTTP 200（仍然是成功响应，但标记为降级）

**涉及测试用例**: P2-15 (通道错误降级测试)

---

### 1.4 [MAJOR] A1-4 — createQRPayment 缺少幂等性保护

**影响文件**: `PayBackend/services/PaymentService.cc` (line 1064-1203)  
**问题**: 与 `createPayment` 不同，`createQRPayment` 完全没有幂等键检查和缓存逻辑  
**风险**: 重复请求创建重复订单

**修复方案**:
1. 在 `createQRPayment` 入口处添加 `idempotencyService_->checkAndSetStatus()` 调用
2. 使用 `makeOnceCallback` 包裹后续异步逻辑
3. 在完成/失败回调中调用 `clearReservation`
4. 幂等键生成策略与 `createPayment` 保持一致（如基于 `app_id + out_trade_no`）

**涉及测试用例**: P1-5 (createQRPayment 幂等性测试)

---

### 1.5 [MAJOR] B1-1 — 微信 refund 通道错误码不一致

**影响文件**: `PayBackend/services/RefundService.cc` (line 1237-1400)  
**问题**: 
| 场景 | 支付宝 | 微信 |
|------|--------|------|
| client not ready | code=1501 + error_code(1501) | **code=0 + error_code()** |
| 业务错误 | code=1502 + error_code(1502) | **code=0 + error_code()** |

**修复方案**:
1. 在微信 refund 的 `invokeRefundChannel` 分支中：
   - `!wechatClient_` → 返回 `code=1501` + `error_code(1501)` + 消息 `"WeChat refund client not configured"`
   - 业务错误 → 返回 `code=1502` + `error_code(1502)` + 消息 `result["message"]`
2. 在 `PayController.refund()` 中确认 `mapErrorToHttpStatus` 能正确处理 code=1501 和 code=1502

**涉及测试用例**: RefundQueryTest 现有用例的预期行为可能需要微调

---

### 1.6 [MAJOR] B1-2 — amount 字符串缺少格式校验

**影响文件**: `PayBackend/controllers/PayController.cc`  
**问题**: Controller 层只做了 `isMember("amount")` 存在性检查，没有格式验证  
**风险**: `"12.345"`, `"1,000.00"`, `"-100"`, `"abc"` 等非法格式会穿透到 Service 层造成解析异常

**修复方案**:
1. 在 `PayController.cc` 中添加 `validateAmount()` 辅助函数:
   ```cpp
   static bool validateAmount(const std::string& amount) {
       static const std::regex pattern(R"(^\d+(\.\d{1,2})?$)");
       return std::regex_match(amount, pattern);
   }
   ```
2. 在以下端点的参数校验阶段调用:
   - `createPayment()` → 校验 `amount`
   - `refund()` → 校验 `amount`
3. 校验失败返回 HTTP 400 + `{"code": 40001, "message": "Invalid amount format"}`

**涉及测试用例**: P2-14 (amount 格式校验测试)

---

### 1.7 [MAJOR] C2-1 — 微信回调 JSON 解析失败静默降级

**影响文件**: `PayBackend/controllers/WechatCallbackController.cc` (line 22-35)  
**问题**: `event_type` 字段缺失或 body 非合法 JSON 时，默认路由到支付回调处理器  
**风险**: 恶意非 JSON body 被当作支付回调处理；合法的退款通知因编码问题被错误路由

**修复方案**:
1. JSON 解析失败 (`body` 非合法 JSON) → 直接返回 HTTP 400 + `"code": 40002, "message": "Invalid JSON body"`
2. `event_type` 字段缺失 → 直接返回 HTTP 400 + `"code": 40003, "message": "Missing event_type"`
3. `event_type` 非预期值（不是 `"TRANSACTION.SUCCESS"` 也不是 `"REFUND.*"`）→ 返回 HTTP 400 + `"code": 40004, "message": "Unknown event_type: xxx"`

**涉及测试用例**: P1-8 (JSON 解析失败测试), P1-9 (event_type 空字符串测试)

---

### 1.8 [MAJOR] C1-2 — 支付宝表单 `+` 号编码问题

**影响文件**: `PayBackend/controllers/AlipayCallbackController.cc` (line 22-37)  
**问题**: `urlDecode` 可能不将 `+` 转为空格  
**风险**: 支付宝使用 `+` 编码空格时，解析结果含字面量 `+` 号

**修复方案**:
1. 在 `urlDecode` 调用前，将值字符串中的 `+` 替换为 `%20`:
   ```cpp
   std::string sanitized = value;
   std::replace(sanitized.begin(), sanitized.end(), '+', ' ');
   std::string decoded = drogon::utils::urlDecode(sanitized);
   ```
   注意：先替换 `+` 再 decode，避免 `%2B`（字面量加号）被误转。

**涉及测试用例**: P0-3 (URL 编码边界、`+` 号处理测试)

---

## Phase 2: MINOR 缺陷修复 (6 项)

### 2.1 [MINOR] A1-5 — PaymentService.cc 巨型文件拆分

**影响文件**: `PayBackend/services/PaymentService.cc` (2667 行)  
**决策** (✅ 已确认): **降级为本次不实施，记入后续技术债务**

**理由**: 
- 拆分涉及大量文件移动、调用方更新、DI 注册变更
- 在 Phase 1 修复完成后进行风险过高
- 列入 `TECH_SPECS.md` 的技术债务清单，下一迭代实施

**后续建议**:
1. 拆分为三个独立服务类: `PaymentCreateService`、`PaymentQueryService`、`PaymentSyncService`
2. 共享工具函数提取到 `PayUtils.h/cc`

**涉及测试用例**: 无（不实施）

---

### 2.2 [MINOR] A1-6 — queryOrderList 使用 RAW SQL

**影响文件**: `PayBackend/services/PaymentService.cc` (line 2530-2590)  
**建议**:
1. 为 LEFT JOIN 编写带命名参数的已验证 SQL 模板常量
2. 确保 `SqlBinder` 的流式绑定覆盖所有参数位置
3. 添加注释说明为何无法使用 ORM（Drogon Mapper 不支持 LEFT JOIN）

---

### 2.3 [MINOR] B1-3 — 退款金额前置检查非原子

**影响文件**: `PayBackend/services/RefundService.cc` (line 688-878)  
**分析**: 当前设计安全——事务中 SUM 检查兜底。  
**建议**: 
1. 将 `proceedWithAmountCheck` 中的 `findByRef` 重复成功退款检查移入事务中
2. 减少独立 DB 查询次数，降低竞态窗口（尽管现在的竞态不影响正确性）

---

### 2.4 [MINOR] C2-3 — CallbackService 缺少 dbClient_ 空指针检查

**影响文件**: `PayBackend/services/CallbackService.cc`  
**修复**:
1. 构造函数中添加:
   ```cpp
   assert(dbClient_ != nullptr);
   assert(config_ != nullptr);
   ```
2. 或添加运行时检查，在初始化失败时抛出 `std::invalid_argument`

---

### 2.5 [MINOR] D2-1 — Prometheus 指标序列化重复

**影响文件**: `PayBackend/controllers/PayMetricsController.cc`, `PayBackend/controllers/MetricsController.cc`  
**修复**:
1. 提取 `PayAuthMetrics::toPrometheus()` 静态方法
2. `PayMetricsController::authMetricsProm()` 和 `MetricsController::buildAuthMetricsProm()` 共用同一实现

---

### 2.6 [MINOR] S-1 — containsKeyConstantTime 不完全恒定时间

**影响文件**: `PayBackend/filters/PayAuthFilter.cc`  
**分析**: 早期返回泄露 key 数量信息，实际攻击价值极低  
**修复**:
1. 改为全程扫描后统一比较:
   ```cpp
   bool found = false;
   for (const auto& validKey : validKeys) {
       if (CRYPTO_memcmp(key.data(), validKey.data(), key.size()) == 0) {
           found = true;
       }
   }
   return found;
   ```

---

## Phase 3: NIT 缺陷修复 (3 项)

### 3.1 [NIT] B1-4 — insertLedgerEntry/storeIdempotencySnapshot 代码重复

**影响文件**: `PayBackend/services/RefundService.cc` (line 46-203), `PayBackend/services/PaymentService.cc` (line 51-208)  
**修复**: 提取到共享工具模块 `PayUtils.h/cc` 或新文件 `services/PaySharedHelpers.h/cc`

---

### 3.2 [NIT] C2-4 — 回调日志潜在敏感信息泄露

**影响文件**: `PayBackend/services/CallbackService.cc` (line 454)  
**修复**: 
1. 将 `LOG_INFO` 降为 `LOG_DEBUG`
2. 或对 body 内容做脱敏处理（mask 金额、银行卡号等字段）

---

### 3.3 [NIT] D1-3 — /health 废弃端点 ✅ Done

**文件**: `PayBackend/controllers/HealthCheckController.cc` (line 162-179)  
**状态**: 已正确实现 — 返回 `Deprecation: true` 和 `Sunset: 2026-08-28` 头  
**无需修复**

---

## Phase 4: P0 安全关键测试 (3 项)

### 4.1 支付宝回调签名验证测试

**测试文件**: 新建 `PayBackend/test/AlipayCallbackIntegrationTest.cc`  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 签名验证成功 | 有效签名 + 有效参数 | HTTP 200, `return_code=SUCCESS` |
| 签名验证失败 | 无效签名 | HTTP 400, `code=50001` |
| 空 sign 参数 | sign="" | HTTP 400, `code=50002` |
| 篡改 sign 参数 | sign 被篡改 | HTTP 400, `code=50001` |
| body 为空 | 空字符串 | HTTP 400 |
| client 未配置 | alipayClient_=nullptr | HTTP 503, `code=50003` |

**覆盖路径**: C1 (POST /api/pay/notify/alipay)

---

### 4.2 支付宝回调表单解析测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 标准 URL 编码 | `key1=val1&key2=val2` | 正确解析 |
| `+` 号作为空格 | `name=hello+world` | 解析为 `hello world` |
| `%20` 作为空格 | `name=hello%20world` | 解析为 `hello world` |
| 空值字段 | `key1=&key2=val` | key1 为空字符串 |
| 非 URL 编码 body | 纯 JSON body | 返回 400 |
| 超长 body | body > 10KB | 正常解析或拒绝 |

**覆盖路径**: C1 (POST /api/pay/notify/alipay)

---

### 4.3 微信回调事件类型路由测试

**覆盖路径**: C2 (POST /api/pay/notify/wechat)

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| `TRANSACTION.SUCCESS` | 正确路由到支付回调 | handlePaymentCallback 被调用 |
| `REFUND.SUCCESS` | 正确路由到退款回调 | handleRefundCallback 被调用 |
| 空 event_type | JSON 无 event_type 字段 | HTTP 400, code=40003 |
| 未知 event_type | `"UNKNOWN.EVENT"` | HTTP 400, code=40004 |
| 非 JSON body | 纯文本 | HTTP 400, code=40002 |
| body 为空 | 空字符串 | HTTP 400, code=40002 |

---

## Phase 5: P1 核心业务测试 (8 项)

### 5.1 createQRPayment 正常流测试

**新建/扩展**: 已有 `CreatePaymentIntegrationTest.cc` 中添加测试用例  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 支付宝 QR 码生成成功 | 有效参数 | HTTP 200, 返回 qr_code_url |
| 微信 QR 码生成成功 | channel=wechat, 有效参数 | HTTP 200, 返回 code_url |
| 金额为最小合法值 | amount="0.01" | HTTP 200 |
| 金额为最大合法值 | amount="9999999.99" | HTTP 200 |
| 缺少 amount | 无 amount 字段 | HTTP 400 |
| 缺少 channel | 无 channel 字段 | HTTP 400 |

---

### 5.2 createQRPayment 幂等性测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 重复请求返回相同结果 | 同一 idempotency_key 两次请求 | 相同响应，无重复订单 |
| 不同 key 创建不同订单 | 不同 idempotency_key | 不同 out_trade_no |
| 幂等冲突时返回已有结果 | cached 结果 | 返回原订单数据 |

---

### 5.3 queryOrderList 参数组合测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 无参数默认分页 | 无参数 | 返回前 N 条 |
| 按 status 筛选 | status="SUCCESS" | 仅返回成功订单 |
| 按 user_id 筛选 | user_id="xxx" | 仅返回该用户订单 |
| offset+limit 分页 | offset=10, limit=5 | 返回第 11-15 条 |
| limit=0 | limit=0 | 返回空数组或报错 |
| limit 超最大值 | limit=10000 | 截断到上限 |
| 多条件组合 | status+user_id+date_range | 正确复合筛选 |

---

### 5.4 queryOrderList 空结果测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 空数据库 | 无数据 | 空数组, total=0 |
| 筛选条件无匹配 | status="NONEXISTENT" | 空数组, total=0 |
| offset 超出范围 | offset=99999 | 空数组 |

---

### 5.5 reconcileSummary 多状态组合测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 空数据 | 无任何订单 | 全部计数为 0 |
| 单状态数据 | 全部 SUCCESS | 对应计数正确 |
| 多状态混合 | SUCCESS + FAILED + PAYING | 各状态计数独立正确 |
| 跨日期范围 | start_date/end_date | 仅返回范围内数据 |
| 无日期参数 | 无 date 参数 | 返回今天数据 |

---

### 5.6 createPayment DB 事务失败回滚测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| PayOrder INSERT 失败 | Mock DB 抛出异常 | 无数据残留 |
| PayPayment INSERT 失败 | Mock DB 在 payment 写入时失败 | PayOrder 已回滚 |
| 事务超时 | 模拟长事务 | 回滚 |

**覆盖**: MAJOR A1-1 修复验证

---

## Phase 6: P2 边界条件测试 (9 项)

### 6.1 readyz 滞后阈值测试

**测试文件**: 扩展 `PayBackend/test/HealthProbeTest.cc`  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 1 次 DB 失败 | 第 1 次 DB 不可达 | HTTP 200, ready |
| 连续 2 次 DB 失败 | DB 连续不可达 | HTTP 200, ready |
| 连续 3 次 DB 失败 | DB 连续不可达（达到阈值） | HTTP 503, not_ready |
| 阈值恢复 | 超过阈值后 DB 恢复 | 下次 HTTP 200, ready |
| Redis 独立阈值 | Redis 失败但 DB 正常 | 独立计数 |

---

### 6.2 健康检查超时测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| DB 查询 1 秒内完成 | 正常 | HTTP 200 |
| DB 查询超过 1 秒 | 模拟慢查询 | failed 数组含 "timeout" |
| Redis PING 超过 1 秒 | 模拟慢 Redis | failed 数组含 "timeout" |
| 两者都超时 | DB + Redis 都慢 | 两个都标记 timeout |

---

### 6.3 amount 格式校验测试

**测试文件**: 新建 `PayBackend/test/AmountValidationTest.cc` 或扩展对应 Controller 测试  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 合法整数 | "100" | 通过 |
| 合法两位小数 | "100.00" | 通过 |
| 合法一位小数 | "0.5" | 通过 |
| 三位小数 | "100.001" | 拒绝, 400 |
| 负数 | "-100" | 拒绝, 400 |
| 非数字 | "abc" | 拒绝, 400 |
| 逗号分隔 | "1,000.00" | 拒绝, 400 |
| 空字符串 | "" | 拒绝, 400 |
| 科学计数法 | "1e5" | 拒绝, 400 |
| 边界值 0 | "0.00" | 通过 |

---

### 6.4 queryOrder 通道降级测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 微信查询成功 | 正常 | 返回完整数据, code=0 |
| 微信查询失败 | Mock wechat API 返回错误 | code=1, data 含 wechat_query_error |
| 支付宝查询失败 | Mock alipay API 返回错误 | code=1, data 含 alipay_query_error |
| 通道超时 | Mock 通道 API 超时 | code=1, 有降级数据 |

**覆盖**: MAJOR A1-3 修复验证

---

### 6.5 并发退款 SUM 正确性测试

**测试文件**: 扩展 `PayBackend/test/RefundQueryTest.cc`  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 并发 5 个部分退款 | 总金额 100, 各退 20 | 5 个全部成功 |
| 并发 6 个部分退款 | 总金额 100, 各退 20 | 5 个成功, 1 个被拒绝（总额超限） |
| 并发全额退款 | 总金额 100, 各退 100 | 仅 1 个成功, 其余拒绝 |
| FOR UPDATE 行锁验证 | 并发执行 | 无死锁, 无幻读 |

---

### 6.6 idempotency clearReservation 测试

**测试文件**: 扩展 `PayBackend/test/IdempotencyIntegrationTest.cc`  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 正常完成释放 reservation | 支付成功 | key 可重新使用 |
| 异常后释放 reservation | 模拟中途异常 | key 被清除, 不残留 |
| reservation TTL 过期 | 等待 TTL 超时 | key 自动释放 |
| 重复 release | 连续两次 clearReservation | 无异常 |

---

### 6.7 PayAuthFilter Scope 拒绝测试

**测试文件**: 扩展 `PayBackend/test/PayAuthFilterTest.cc`  
**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 无 scope key 访问 refund | API Key 缺少 refund scope | HTTP 403 |
| order_query scope key 访问 refund | scope = order_query | HTTP 403 |
| refund scope key 访问 refund | scope = refund | HTTP 200 |
| 空 scope key 访问无限制端点 | scope="" 访问 /pay/query | HTTP 200 |

---

### 6.8 CORS OPTIONS 预检测试

**测试用例**:

| 用例 | 输入 | 预期 |
|------|------|------|
| 白名单 origin | Origin: https://允许的域名 | Access-Control-Allow-Origin 正确 |
| 非白名单 origin | Origin: https://恶意的域名 | 无 CORS 头或拒绝 |
| 无 Origin 头 | 无 Origin | 不返回 CORS 特定头 |
| OPTIONS 方法 | OPTIONS /api/pay/create | HTTP 200, CORS 头完整 |

---

### 6.9 安全头验证测试

**测试用例**:

| 用例 | 检查项 | 预期 |
|------|--------|------|
| X-Content-Type-Options | nosniff | ✅ 存在 |
| X-Frame-Options | DENY | ✅ 存在 |
| X-XSS-Protection | 1; mode=block | ✅ 存在 |
| Strict-Transport-Security | max-age=... | ✅ 存在 |
| Content-Security-Policy | ... | ✅ 存在 (如配置) |
| Referrer-Policy | ... | ✅ 存在 (如配置) |

---

## Phase 7: P3 完善性测试 (5 项)

### 7.1 OnceCallback 异常安全性测试

**测试文件**: 扩展 `PayBackend/test/OnceCallbackTest.cc`  
**测试用例**: 回调内抛异常、多次调用、并发调用

---

### 7.2 PayErrorCategory 并发测试

**测试文件**: 新建或扩展  
**测试用例**: 并发 `setMessage` 操作、获取未注册错误码

---

### 7.3 PayUtils validateNotifyUrl 测试

**测试用例**: 各种非法 URL（空、无协议、非 https、包含特殊字符）

---

### 7.4 Prometheus 指标格式校验

**测试文件**: 扩展 `PayBackend/test/ControllerMetricsTest.cc`  
**测试用例**: 验证 `/api/pay/metrics/auth.prom` 输出符合 Prometheus exposition format

---

### 7.5 /health 废弃端点 Sunset 头测试

**测试用例**: 验证 `Deprecation: true` 和 `Sunset: 2026-08-28` 头正确返回

---

### 7.6 订单状态机文档一致性验证

**测试文件**: 参考 `TECH_SPECS.md` 状态机文档  
**测试用例**:
- 验证 `/api/pay/create` 创建的订单状态机路径: `CREATED → PAYING → SUCCESS/FAILED`
- 验证 `/api/qrpay/create` 创建的订单状态机路径: `PAYING → SUCCESS/FAILED`
- 验证两种路径都在对账查询 (`reconcileSummary`) 中被正确统计
- 验证两种路径都在订单列表 (`queryOrderList`) 中可被检索

**覆盖**: MAJOR A1-2 修复验证（方案 B — 文档差异化）

---

## 执行顺序建议

```
Week 1                      Week 2                      Week 3
┌─────────────────────┐ ┌─────────────────────┐ ┌─────────────────────┐
│ Phase 1: MAJOR 修复  │ │ Phase 4: P0 测试    │ │ Phase 6: P2 测试    │
│ 1.1  事务保护        │ │ 4.1  支付宝回调签名  │ │ 6.1-6.9 边界测试    │
│ 1.2  状态机文档化    │ │ 4.2  表单解析        │ │                     │
│ 1.3  降级错误码      │ │ 4.3  微信事件路由    │ │                     │
│ 1.4  QR幂等          │ │                     │ │                     │
│ 1.5  退款错误码      │ ├─────────────────────┤ ├─────────────────────┤
│ 1.6  amount 校验     │ │ Phase 5: P1 测试    │ │ Phase 7: P3 测试    │
│ 1.7  微信路由        │ │ 5.1-5.6 核心业务    │ │ 7.1-7.6 完善        │
│ 1.8  支付宝+号       │ │                     │ │                     │
├─────────────────────┤ ├─────────────────────┤ ├─────────────────────┤
│ Phase 2: MINOR 修复  │ │                     │ │ 回归测试             │
│ 2.2  RAW SQL注解     │ │                     │ │                     │
│ 2.3  退款检查事务    │ │                     │ │                     │
│ 2.4  空指针检查      │ │                     │ │                     │
│ 2.5  指标去重        │ │                     │ │                     │
│ 2.6  恒定时间比较    │ │                     │ │                     │
├─────────────────────┤ │                     │ │                     │
│ Phase 3: NIT 修复    │ │                     │ │                     │
│ 3.1  共享工具提取    │ │                     │ │                     │
│ 3.2  日志脱敏        │ │                     │ │                     │
└─────────────────────┘ └─────────────────────┘ └─────────────────────┘
```

**依赖关系**:
- Phase 1 修复完成后才能执行 Phase 4-5 的验证测试
- Phase 6 的测试驱动 Phase 1 修复的验收 (如 6.4 验证 1.3、6.5 验证原有事务设计)
- Phase 7 与 Phase 2-3 修复可并行
- **推迟项**: PaymentService 文件拆分 (2.1) 记入 TECH_SPECS.md 技术债务清单

**单日工作上限**: 不超过 3 个修复项 + 3 个测试用例，确保每个改动有充分的时间和回归验证。

---

## 验收标准

| 指标 | 目标值 |
|------|--------|
| MAJOR 缺陷数 | 0 |
| MINOR 缺陷数 | ≤1 (巨型文件拆分为长期持续优化) |
| NIT 缺陷数 | 0 |
| 所有已有测试通过 | ✅ |
| P0 测试通过率 | 100% |
| P1 测试通过率 | 100% |
| P2 测试通过率 | 100% |
| P3 测试通过率 | 100% |
| 回归测试通过率 | 100%（13 条已有测试） |

---

> **制定日期**: 2026-07-29  
> **审查报告**: docs/review/deep-code-review-report.md
