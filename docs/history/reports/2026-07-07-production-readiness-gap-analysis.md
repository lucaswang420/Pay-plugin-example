# 生产级支付系统差距分析报告

| 项 | 值 |
|---|---|
| 报告日期 | 2026-07-07 |
| 评估范围 | `PayBackend/`(Drogon C++17 支付服务 + Alipay 沙箱 / WeChat Pay v3 集成) |
| 评估方法 | 源码逐行复核 + 对照 PCI DSS v4.0.1 / 业界支付编排系统标准 |
| 评估状态 | 仅静态审计(读源码、配置、测试、文档),未运行渗透测试 |
| 结论 | **不具备生产级支付系统的安全与正确性**。3 个 P0、6 个 P1、多项 P2 缺陷 |

> ⚠️ `README.md:3` 与 `CLAUDE.md` 标注 **"✅ Production Ready"**,本审计判定该标注**不准确**,建议在 P0 修复前撤下该徽标。

---

## 1. 已经做对的部分(确认无问题)

下列项经核实实现正确,修复时不应破坏:

- [x] **幂等性核心设计扎实** — `INSERT ... ON CONFLICT (idempotency_key) DO NOTHING` 原子抢占(`IdempotencyService.cc:99-100`);失败时 `clearReservation` 仅删 `response_snapshot IS NULL` 的行,避免 key poisoning(`:301-303`)。
- [x] **WeChat v3 加密栈正确** — RSA-2048/SHA-256 签名(`WechatPayClient.cc:62`)、平台证书验签(`:156`)、AES-256-GCM 解密含 tag 校验(`:219`),符合官方规范。
- [x] **全程参数化查询,无 SQL 注入** — 所有 `execSqlAsync` 用 `$1..$n` 绑定参数;ORM `Criteria` 使用规范;无字符串拼接 SQL。
- [x] **有对账扫表任务** — `ReconciliationService` 每 300s 扫 `PAYING` 订单与 `REFUND_INIT/REFUNDING` 退款,向渠道查询并回填(`ReconciliationService.cc:105-234`),在 `PayPlugin.cc:82` 启动。
- [x] **PCI 范围天然小** — 仅 QR/redirect 流程,从不触碰 PAN/CVV(grep `PAN|card_no|cvv|cvc` 零命中),原则上可走 SAQ A。
- [x] **启动安全校验** — `StartupValidator::validate` 对 `PAY_DB_PASSWORD`/`PAY_API_KEY` 缺失或仍为占位符时 `exit(1)`(`StartupValidator.cc:50-59`);`/readyz` 初始为 not-ready,首次探针通过 DB+Redis 才转 ready。
- [x] **私钥/证书文件已 gitignore** — `PayBackend/certs/` 未被版本控制追踪。
- [x] **WeChat 平台证书自动轮换** — 每 12h 刷新(`PayPlugin.cc:191`),且下载有节流。
- [x] **`OnceCallback` 防止异步回调二次调用**(`utils/OnceCallback.h`),全代码库普遍使用。
- [x] **Dockerfile 以非 root 运行**(`Dockerfile:65`)。

---

## 2. P0 — 阻断生产的严重缺陷(上线前必须修复)

### P0-1 支付宝回调签名从未校验 → 可伪造支付成功 ⚠️ 资金损失

| | |
|---|---|
| 位置 | `PayBackend/controllers/AlipayCallbackController.cc:10-92` |
| 根因 | `notify()` 直接解析表单、调用 `syncOrderStatusFromAlipay` 改库,**全程无 `verifyCallback` 调用**。`AlipaySandboxClient::verifyCallback`(`AlipaySandboxClient.cc:185`)存在但**从未被任何路径调用**。 |
| 触发条件 | 该端点**无 auth filter**(`AlipayCallbackController.h:12`)。任何能访问 `/api/pay/notify/alipay` 的人 POST 一个 `trade_status=TRADE_SUCCESS` 即可把订单标记为已支付,触发账本写入、满足退款前置条件。 |
| 业界标准 | 所有支付回调必须**先验签再改任何状态**(Stripe / Adyen / WeChat v3 / Alipay 官方文档均强制)。 |
| 影响 | **直接资金/状态完整性妥协**,独立构成 PCI 12.2/6.5 控制失效。 |

- [ ] 修复:在 `notify()` 开头、任何 DB 写入之前调用 `alipayClient->verifyCallback(params)`,验签失败返回 `failure` 且不触碰数据库。

### P0-2 退款超额发放竞态 → 可退款超过实付金额

| | |
|---|---|
| 位置 | `PayBackend/services/RefundService.cc:790-848`(SUM 检查)→ `:927-930`(非原子插入+渠道调用) |
| 根因 | 先 `SUM(amount)` 判断 `refunded + new <= total`,再在**另一非原子步骤**插入新退款行并调用渠道。全仓库 grep `for update` / `pg_advisory` / `lock in share` **零命中**——无行锁、无唯一约束。 |
| 触发条件 | 两个并发退款请求(金额不同)都能通过 SUM 检查并各自执行。`proceedWithInProgressCheck`(`:711`)**仅在金额完全相同时**拦截。 |
| 业界标准 | 退款用 `SELECT ... FOR UPDATE` 锁支付行 + `(order_no, payment_no, refund_no)` 唯一约束;或乐观锁 `UPDATE ... WHERE status = expected AND refunded + new <= total`。 |
| 影响 | 退款总额可超过实付金额。 |

- [ ] 修复:退款流程加行锁 + 唯一约束 + CAS 更新;先写复现测试(两个并发退款断言总退款 ≤ 实付)再修。

### P0-3 配置键不匹配 → WeChat 渠道与幂等 TTL 在生产"静默失效"

| | |
|---|---|
| 位置 | `PayBackend/plugins/PayPlugin.cc:30`(`config["wechat"]`)、`:60`(`config["idempotency`) |
| 根因 | 代码读 `config["wechat"]`,但 `config.json:202` 的键是 **`wechat_pay`** → `WechatPayClient` 拿到空 config,`isConfigured()` 为 false;代码读 `config["idempotency"]`,但 `config.json:198` 的键是 **`idempotency_ttl_seconds`** → TTL 永远回退到硬编码 `604800`。 |
| 后果 | 即便其他代码写对,生产环境里 WeChat 渠道根本跑不起来(支付/对账全报 "missing appid/mchid/notify_url"),且只有 WARN 级日志,极易被忽略。 |
| 影响 | 整个 WeChat 支付链路功能失效 + 配置不可调。 |

- [ ] 修复:对齐键名(`config["wechat_pay"]` / `config["idempotency_ttl_seconds"]`),并在 `initAndStart` 加启动断言(配置缺失/未配置时 fail-fast 而非静默继续)。

---

## 3. P1 — 高严重性缺陷

### P1-1 回调无重放保护(无时间窗口 / nonce 缓存)

| | |
|---|---|
| 位置 | `CallbackService.cc:111`、`WechatPayClient.cc:653-691`;P0-1 的支付宝回调同理 |
| 根因 | WeChat 验签正确,但**不比较 timestamp 与当前时间**、**无 nonce 缓存**。截获一个合法回调可无限重放。 |
| 业界标准 | WeChat 官方要求拒绝 5 分钟外的 timestamp。当前仅靠 notify id 幂等兜底,但每次重放仍打 DB。 |

- [ ] 修复:两端回调在验签后加 `|now - timestamp| ≤ 300s` 检查 + Redis nonce 缓存(TTL 略大于窗口)。

### P1-2 API Key 比较非恒定时间 → 时序攻击

| | |
|---|---|
| 位置 | `PayBackend/filters/PayAuthFilter.cc:142`(`std::find` / `std::string::operator==`,首字节即短路) |
| 影响 | 攻击者可逐字节探测出合法 key。 |

- [ ] 修复:用 `CRYPTO_memcmp` 在等长缓冲区上比较。

### P1-3 SSRF:创建支付时 `notify_url` 未校验

| | |
|---|---|
| 位置 | `PayBackend/services/PaymentService.cc:387-402`(对比 `RefundService.cc:371-390` **有**校验 scheme/长度) |
| 影响 | 请求方传入的 `notify_url` 直接送进渠道请求,可用于内网探测 / 把回调路由到攻击者服务器。 |

- [ ] 修复:`PaymentService` 复用 `RefundService` 的 `notify_url` 校验(scheme allowlist + host allowlist + 私网/loopback 阻断)。

### P1-4 Scope 校验完全失效(dead code)

| | |
|---|---|
| 位置 | `PayBackend/filters/PayAuthFilter.cc:56-72`(`resolveScope`)、`:154`(`!scope.empty()` 短路) |
| 根因 | `resolveScope` 匹配 `/pay/refund` 等,但真实路由是 **`/api/pay/...`** → 恒返回 `{}` → scope 检查被跳过。Hodor 限流器 URL 模式有同样的 `/pay` vs `/api/pay` 错配,子限额(create=20、refund=10)永远不触发。 |
| 影响 | **任意合法 key 都能退款/对账**;`admin`/`reconcile` scope 声明了但从不强制。 |

- [ ] 修复:`resolveScope` 路由前缀改为 `/api/pay/...`;修 Hodor URL 模式同步。

### P1-5 无 HTTPS / 安全头配置了却从不生效

| | |
|---|---|
| 位置 | `config.json:6`(`"https": false`)、`deploy/security_headers_config.json`(定义了 HSTS/CSP/X-Frame-Options 等)、`main.cc:66-84`(`registerPostHandlingAdvice` **只加了 CORS 头**) |
| 根因 | `security_headers_config.json` 从未被任何 `.cc/.h` 引用(grep 验证)。API key 明文传输,除非有反向代理。 |
| 业界标准 | 支付系统必须 TLS 1.2+,HSTS preload,CSP,严格 CORS。 |

- [ ] 修复:TLS 终结于反向代理并在文档写明;把 `security_headers_config.json` 真正接入 `registerPostHandlingAdvice`。

### P1-6 所有告警永久 "no data"(可观测性形同虚设)

| | |
|---|---|
| 位置 | `PayBackend/deploy/alerts.yml` vs `PayBackend/filters/PayAuthMetrics.cc` |
| 根因 | `alerts.yml` 引用 `http_request_duration_seconds_bucket`、`http_requests_total`、`payment_success_total`、`refund_attempts_total`、`db_connections_active`、`redis_connections_active` 等;但**全仓库 `counter(`/`gauge(`/`histogram(` 零命中**——应用只发了 4 个 auth 计数器。 |
| 影响 | **9 条告警规则全部永不触发**,on-call 形同虚设。无 RED 指标、无业务计数、无直方图、无 DB/Redis 池监控、无结构化日志、无 request-id/trace 传播。 |

- [ ] 修复:实现告警已引用的指标(RED + 业务计数 + DB/Redis 池 gauge + 直方图),或重写 `alerts.yml` 对齐实际指标;补结构化 JSON 日志与 trace 传播。

---

## 4. P2 — 功能 / 合规 / 工程差距(对照业界生产级支付编排系统)

### 4.1 安全

- [ ] **单全局 API key**,无 per-merchant key、无 key-id、无轮换/吊销/过期(`PayAuthFilter.cc:87-117`)。
- [ ] **硬编码弱测试 key** `test_key_123456` 带 `admin/refund/reconcile` 全权限(`config.json:237`)。
- [ ] **无密钥管理**:无 KMS/Vault/HSM。
- [ ] **私钥每次请求重新读盘+解析**(`AlipaySandboxClient.cc:414`、`WechatPayClient.cc:31`),性能差且有 TOCTOU;应在构造时加载并缓存 `EVP_PKEY*`。
- [ ] **无暴力破解锁定**,Hodor 仅 IP 维度且 `trust_ips` 含 `127.0.0.1`。
- [ ] **`/metrics` 未鉴权**(`MetricsController.h:11`)。
- [ ] **`amount` 为原始字符串**,控制器层无类型/范围校验(`PayController.cc:71`)。
- [ ] **下载的 WeChat 平台证书不校验证书链**就使用(`WechatPayClient.cc:495-510`)。

### 4.2 正确性 / 可靠性

- [ ] **无分布式事务 / Outbox / Saga** — `pay_idempotency` 行在主事务**之外**提交(`CallbackService.cc:404`),主事务靠手动 `"COMMIT"` SQL(`:674, 729`)。进程在最后一条语句与 COMMIT 间崩溃 → 状态丢失但幂等记录已落,重试看到"已处理"直接返回 SUCCESS → **数据不一致**。
- [ ] **部分失败未覆盖** — 渠道调用成功但后续 DB 更新失败 → 渠道已有交易、本地仍 `CREATED`,而对账扫表只扫 `PAYING`(`ReconciliationService.cc:118`),`CREATED` 订单永远不被回收。
- [ ] **`pay_ledger` 无 FK 约束**(`001_init_pay_tables.sql:76-86`),账本完整性仅靠应用层。
- [ ] **money 存为 `VARCHAR(32)`** 而非 integer fen / NUMERIC(`001_init_pay_tables.sql:18,34,49,82`);存储层与计算层(`parseAmountToFen` int)混用,退款 SUM 还要 `CAST(amount AS NUMERIC)`(`RefundService.cc:791`)。
- [ ] **幂等 TTL 不在读取时过滤**(`IdempotencyService.cc:113` SELECT 无 `expire_at` 条件),也无 GC job → 表无限增长。
- [ ] **`createQRPayment` 完全绕过幂等性**(`PaymentService.cc:767`),重试会重复创建支付宝预下单。
- [ ] **`generatePaymentNoValue` 用 `mt19937`**(`:28-46`),高并发下可碰撞。
- [ ] **迁移非版本化** — 无 Flyway/Liquibase/golang-migrate,无 `schema_history` 表;`scripts/deploy.sh:115` 遍历 `sql/*.sql` 全部执行,**`000_drop_pay_tables.sql` 会被当成迁移执行 → 每次部署 `DROP ... CASCADE` 全表**。这不是零停机,是零数据。

### 4.3 测试 / CI

- [ ] **Windows / macOS CI 从不跑测试** — 仅 Linux lane 配了 Postgres+Redis + ctest(`.github/workflows/ci-linux.yml`);`ci-windows.yml`/`ci-macos.yml` 只 build 不 test。
- [ ] **无 sanitizer(ASan/UBSan)**;`.clang-tidy` 存在但 CI 从不运行。
- [ ] **无依赖扫描 / Trivy / CodeQL / Gitleaks / SBOM**。
- [ ] **缺关键场景测试**:并发重复支付、部分退款、幂等过期、DB/Redis 宕机、大负载、SQL 注入。
- [ ] **6 处 `sleep_for(50ms)` 定时等待**(如 `CreatePaymentIntegrationTest.cc:170`、`RefundQueryTest.cc:1024, 1184`、`WechatCallbackIntegrationTest.cc:791, 1041, 5263`)→ flaky 风险。
- [ ] **PayFrontend 零测试**。
- [ ] **`Dockerfile` 缺陷**:COPY 不存在的 `config.example.json`(构建失败);runtime 镜像未装 curl 但 HEALTHCHECK 用 curl;把测试二进制打进生产镜像。
- [ ] **systemd 服务名不一致**:deploy.sh 生成 `payserver`,`restore_db.sh`/`restart_service.sh` 引用 `payplugin` → 运维脚本失效。
- [ ] **无 lockfile**(Conan cache 步骤被注释),构建不可复现。

### 4.4 合规 / 运维

- [ ] **无 GDPR/PIPL 数据保留与删除策略**;`pay_callback` 行从不删除。
- [ ] **无 tamper-evident 审计日志**(无 hash 链 / WORM / 外部存证);日志不外发(无 syslog/fluentd/OTLP)。
- [ ] **无 KYC/AML/制裁筛查 hook**;无数据本地化(中国 PIPL)说明。
- [ ] **无 runbook / on-call / 事件响应**;`operations_manual.md` 在 `docs/README.md` 被引用但实际缺失。
- [ ] **无密钥轮换流程**(WeChat 平台证书自动轮换是唯一例外)。

### 4.5 功能性差距(对照 Stripe / Adyen / WeChat v3 生产编排能力)

- [ ] 仅 2 个渠道且支付宝仅沙箱客户端(**无生产 Alipay 客户端**,类名即 `AlipaySandboxClient`)。
- [ ] **无向商户投递的 webhooks**(只能接收渠道回调,不能通知下游商户系统)。
- [ ] 无多商户 / 多租户、无手续费 / 分账、无订阅 / 周期扣款、无 3DS、无汇率 / 多币种、无风控规则引擎、无智能路由 / 降级。
- [ ] **无 retry / backoff / circuit breaker**(全仓库仅幂等注释命中 "retry")。
- [ ] WeChat HTTP **无显式 timeout**(`WechatPayClient.cc:349`)。

---

## 5. 推荐修复路线图(分阶段)

### 阶段 A — P0 安全闸门(预计 1–2 人周,必须先做)
- [ ] P0-1:`AlipayCallbackController::notify` 先验签再改库
- [ ] P0-2:退款加行锁 + 唯一约束 + CAS(先 TDD 写并发复现)
- [ ] P0-3:对齐配置键 + 启动断言防漂移

### 阶段 B — P1 加固(预计 2–3 人周)
- [ ] P1-1:两端回调 timestamp 窗口 + nonce 缓存
- [ ] P1-2:`CRYPTO_memcmp` 替换 key 比较
- [ ] P1-3:`PaymentService` 校验 `notify_url`
- [ ] P1-4:修 `resolveScope` 与 Hodor 路由前缀
- [ ] P1-5:TLS 终结 + 安全头接入
- [ ] P1-6:实现告警引用的指标(或重写 alerts.yml)

### 阶段 C — P2 工程 / 合规(持续)
- [ ] 引入迁移工具(golang-migrate/Flyway),移除 `000_drop` 进生产路径;Outbox 让幂等行与业务同事务
- [ ] CI:三平台跑测试、加 ASan/UBSan lane、跑 `.clang-tidy`、加 Trivy/CodeQL/Gitleaks、生成 SBOM;修 Dockerfile
- [ ] per-merchant key + key-id + 轮换;KMS/Vault;启动时加载并缓存 EVP_PKEY
- [ ] tamper-evident 审计日志 + 结构化 JSON 日志 + request-id/OTLP trace
- [ ] 运维:runbook、统一 systemd 名、密钥轮换流程、数据保留/删除策略

---

## 6. 关键源文件参考(均已逐行核实)

| 缺陷 | 文件:行 |
|---|---|
| P0-1 支付宝回调未验签 | `PayBackend/controllers/AlipayCallbackController.cc:10-92`(未调 `verifyCallback`);未用的验签器 `plugins/AlipaySandboxClient.cc:185` |
| P0-2 退款竞态 | `PayBackend/services/RefundService.cc:790-848`(SUM 检查)→ `:927-930`(非原子插入+调用) |
| P0-3 配置键错配 | `PayBackend/plugins/PayPlugin.cc:30, 60` vs `config.json:202, 198` |
| P1-1 无重放保护 | `PayBackend/services/CallbackService.cc:111`、`plugins/WechatPayClient.cc:653-691` |
| P1-2 时序攻击 | `PayBackend/filters/PayAuthFilter.cc:142` |
| P1-3 SSRF | `PayBackend/services/PaymentService.cc:387-402`(对比 `RefundService.cc:371-390`) |
| P1-4 scope 失效 | `PayBackend/filters/PayAuthFilter.cc:56-72, 154` |
| P1-5 无 TLS/安全头 | `PayBackend/config.json:6`、`deploy/security_headers_config.json`(死配置)、`main.cc:66-84` |
| P1-6 告警无数据 | `PayBackend/deploy/alerts.yml` vs `filters/PayAuthMetrics.cc`(全仓库无 counter/gauge/histogram) |
| 幂等 TTL 不生效 | `PayBackend/services/IdempotencyService.cc:113`(SELECT 无 expire_at 过滤)、`:99`(插入写 expire_at) |
| 幂等行事务外提交 | `PayBackend/services/CallbackService.cc:404`(提交)vs `:674`(手动 COMMIT) |
| 毁灭性迁移 | `PayBackend/sql/000_drop_pay_tables.sql` + `scripts/deploy.sh:115` |
| systemd 名不一致 | `PayBackend/scripts/deploy.sh`(生成 `payserver`)vs `deploy/ops/restore_db.sh`、`restart_service.sh`(引用 `payplugin`) |

---

## 7. 参考资料

- PCI SSC — [FAQ Clarifies New SAQ A Eligibility Criteria for E-Commerce Merchants](https://blog.pcisecuritystandards.org/faq-clarifies-new-saq-a-eligibility-criteria-for-e-commerce-merchants)
- PCI SSC — [PCI DSS v4: What's New with SAQs](https://blog.pcisecuritystandards.org/pci-dss-v4-whats-new-with-self-assessment-questionnaires)
- Hyperproof — [PCI DSS 4.0 Update: New SAQ A Eligibility Criteria](https://hyperproof.io/resource/pci-dss-4-0-update-new-saq-a-eligibility-criteria/)
- Gr4vy — [PCI DSS Compliance and Payment Orchestration](https://gr4vy.com/posts/pci-dss-compliance-and-payment-orchestration-a-strategic-approach-to-security/)
- Digital Applied — [Webhook Reliability: Idempotency & Retries Reference](https://www.digitalapplied.com/blog/webhook-reliability-idempotency-retries-engineering-reference-2026)

---

**报告版本**: v1.0 | **作者**: 差距分析审计 | **复核状态**: 待团队评审

## 7. Known Testing Gaps

### 测试环境 Redis 降级导致重放拦截测试缺失 (Nonce Replay Test Gap)
- **描述**: 当前 CallbackService 中集成的 Nonce 重放缓存机制高度依赖 Redis (`pay_nonce` SETNX)。在单元测试与现有集成测试环境中，由于缺乏真实的 Redis，可能依赖 Fail-Open 退路（已知 harness 问题）。
- **影响**: 没有真正基于 Redis 验证的自动化集成测试覆盖 `replay-reject` (重放拦截) 逻辑，当前可能仅通过 fail-open 进行了间接覆盖。
- **改进建议**: 在自动化 CI 流水线 (docker-compose) 中拉起真实的 Redis 容器，提供有效的连接并关闭测试中的 fail-open 退路后，增加针对重放攻击的验证用例。
