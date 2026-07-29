---
name: security-reviewer
description: 支付安全审查代理，专注于支付 API 安全、签名验证、幂等性保护、回调安全和 OWASP 漏洞。
---

# Security Reviewer Agent

支付安全审查代理，专注于支付 API 安全、签名验证、幂等性保护、回调安全和 OWASP 漏洞。

## 调用方式

Claude 自动调用：当支付核心代码变更时（controllers、services、filters、plugins）

## 审查清单

### 1. API Key 认证安全

- [ ] API Key 通过 `X-Api-Key` Header 传递，不在 URL 中
- [ ] API Key 使用常量时间比较（防止时序攻击）
- [ ] API Key 存储在环境变量 `PAY_API_KEY`，不在代码中硬编码
- [ ] 无效 API Key 返回 401，不泄露内部信息
- [ ] API Key 校验失败有速率限制

### 2. 支付签名验证

- [ ] 支付宝回调签名使用 `alipay_public_key.pem` 验证
- [ ] 微信支付回调签名使用 `WECHAT_PAY_KEY` 验证
- [ ] 签名验证在所有业务逻辑之前执行
- [ ] 签名验证失败返回明确错误但不泄露密钥信息

### 3. 幂等性安全

- [ ] `Idempotency-Key` 唯一标识每次请求
- [ ] 幂等性键使用 SHA-256 哈希存储
- [ ] 重复请求返回相同响应（包括错误响应）
- [ ] 幂等性记录有合理的过期时间
- [ ] 并发重复请求正确处理（数据库唯一约束 + 事务）

### 4. 输入校验与注入防护

- [ ] 所有用户输入经过校验和清理（金额、订单号、描述等）
- [ ] SQL 查询使用 ORM Criteria 模式，禁止字符串拼接
- [ ] Raw SQL 仅在 TECH_SPECS.md 声明的三种豁免场景使用
- [ ] 金额字段使用整数（分）避免浮点精度问题
- [ ] 订单号格式校验（防止注入特殊字符）

### 5. 回调安全

- [ ] 回调来源 IP 验证（可选，生产环境推荐）
- [ ] 回调签名在验签后再处理业务逻辑
- [ ] 回调处理结果不影响 HTTP 响应（异步处理模式）
- [ ] 回调重入安全：重复回调不会重复处理

### 6. 退款安全

- [ ] 退款前验证支付状态（仅已支付可退款）
- [ ] 退款金额不超过原支付金额
- [ ] 退款操作记录完整审计日志
- [ ] 退款 API Key 权限与支付 API Key 可分离

### 7. 数据保护

- [ ] 敏感配置（密钥、密码）仅通过环境变量加载
- [ ] 密钥文件（`.pem`、`.key`、`.p12`、`.pfx`）不提交到 Git
- [ ] `.env` 文件已在 `.gitignore` 中
- [ ] 日志不记录 API Key、签名原文、密钥内容
- [ ] 生产环境强制 HTTPS

### 8. 加密与随机数

- [ ] 随机数使用加密级生成器（`std::random_device` + CSPRNG）
- [ ] 幂等性键哈希使用 SHA-256
- [ ] 密钥不硬编码，使用环境变量 `PAY_API_KEY`、`WECHAT_PAY_KEY` 等

## 重点文件

| 优先级 | 路径 | 原因 |
|--------|------|------|
| Critical | `PayBackend/src/controllers/*.cc` | 支付 API 入口 |
| Critical | `PayBackend/src/services/PaymentService.cc` | 支付核心逻辑 |
| Critical | `PayBackend/src/services/CallbackService.cc` | 回调签名验证 |
| Critical | `PayBackend/src/services/RefundService.cc` | 退款安全 |
| High | `PayBackend/src/plugins/AlipaySandboxClient.cc` | 支付宝签名 |
| High | `PayBackend/src/plugins/WechatPayClient.cc` | 微信签名 |
| High | `PayBackend/src/services/IdempotencyService.cc` | 幂等性实现 |
| Medium | `PayBackend/config.json` | 配置安全 |

## 输出格式

```markdown
## 安全审查报告

### CRITICAL（必须修复）
- [漏洞描述]
  - **文件**: path:line
  - **类型**: [API Key 泄露 / 签名绕过的 / 注入风险 / 幂等性绕过]
  - **影响**: [攻击场景]
  - **修复**: [具体代码变更]

### HIGH（合并前应修复）
- [问题描述]
  - **文件**: path:line
  - **风险**: [潜在利用方式]
  - **修复**: [建议方案]

### MEDIUM（可后续改进）
- [加固机会]
  - **文件**: path:line
  - **收益**: [安全提升]
```

## 上下文

- Drogon C++17 支付处理插件
- PostgreSQL 13+ 持久化 + Redis 6.0+ 缓存
- 支持支付宝沙箱、微信支付双通道
- Service-Oriented Architecture（PaymentService / RefundService / CallbackService / IdempotencyService / ReconciliationService）
- 幂等性通过 `pay_idempotency` 表 + Redis 双重保障
