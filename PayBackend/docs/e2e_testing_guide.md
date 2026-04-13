# End-to-End Testing Guide

**目的：** 测试真实的 API 端点，验证服务的整体功能
**优势：** 比更新所有集成测试快得多（3-4 小时 vs 9-13 小时）

---

## 🚀 快速开始

### 1. 启动 PayServer

```bash
cd PayBackend
./build/Release/PayServer
```

或者在 Windows 上：
```powershell
cd PayBackend
.\build\Release\PayServer.exe
```

### 2. 运行端到端测试

**Linux/Mac (Bash):**
```bash
cd PayBackend/test
chmod +x e2e_test.sh
./e2e_test.sh
```

**Windows (PowerShell):**
```powershell
cd PayBackend\test
.\e2e_test.ps1
```

### 3. 查看测试结果

测试脚本会：
- ✅ 创建支付
- ✅ 查询订单
- ✅ 创建退款
- ✅ 查询退款
- ✅ 测试认证指标端点
- ✅ 测试 Prometheus 指标端点

并输出详细的测试结果。

---

## 📊 测试覆盖

| 测试场景 | API 端点 | 验证内容 |
|---------|---------|---------|
| 创建支付 | POST /pay/create | order_no, payment_no, status |
| 查询订单 | GET /pay/query | 订单详情, 状态 |
| 创建退款 | POST /pay/refund | refund_no, 状态 |
| 查询退款 | GET /pay/refund/query | 退款详情, 状态 |
| 认证指标 | GET /pay/metrics/auth | 指标数据 |
| Prometheus 指标 | GET /metrics | Prometheus 格式 |

---

## 🔧 配置选项

### 环境变量

**BASE_URL** (可选)
```bash
export BASE_URL="http://localhost:5566"
```
默认: `http://localhost:5566`

**API_KEY** (可选)
```bash
export API_KEY="your-api-key"
```
默认: `test-api-key`

---

## 📝 测试流程

### 1. 创建支付

```bash
curl -X POST http://localhost:5566/pay/create \
  -H "Idempotency-Key: test_idempotency_key" \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "10001",
    "amount": "9.99",
    "currency": "CNY",
    "title": "E2E Test Order"
  }'
```

**预期响应：**
```json
{
  "order_no": "c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82",
  "payment_no": "9abbb3e5-7b23-4a0f-a62d-4d534c2be9c0",
  "status": "PAYING",
  "wechat_response": {
    "code_url": "weixin://wxpay/bizpayurl?pr=xxxx"
  }
}
```

### 2. 查询订单

```bash
curl "http://localhost:5566/pay/query?order_no=c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82" \
  -H "X-Api-Key: test-api-key"
```

**预期响应：**
```json
{
  "code": 0,
  "message": "Query order successful",
  "data": {
    "order_no": "c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82",
    "payment_no": "9abbb3e5-7b23-4a0f-a62d-4d534c2be9c0",
    "status": "PAYING",
    "amount": "9.99",
    "currency": "CNY"
  }
}
```

### 3. 创建退款

```bash
curl -X POST http://localhost:5566/pay/refund \
  -H "Idempotency-Key: test_refund_idempotency_key" \
  -H "X-Api-Key: test-api-key" \
  -H "Content-Type: application/json" \
  -d '{
    "order_no": "c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82",
    "payment_no": "9abbb3e5-7b23-4a0f-a62d-4d534c2be9c0",
    "amount": "9.99"
  }'
```

**预期响应：**
```json
{
  "refund_no": "refund_xxx",
  "order_no": "c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82",
  "payment_no": "9abbb3e5-7b23-4a0f-a62d-4d534c2be9c0",
  "status": "REFUNDING"
}
```

### 4. 查询退款

```bash
curl "http://localhost:5566/pay/refund/query?refund_no=refund_xxx" \
  -H "X-Api-Key: test-api-key"
```

**预期响应：**
```json
{
  "code": 0,
  "message": "Query refund successful",
  "data": {
    "refund_no": "refund_xxx",
    "order_no": "c2d7c8ad-6a4b-4f3f-9a31-32db0a3fcd82",
    "payment_no": "9abbb3e5-7b23-4a0f-a62d-4d534c2be9c0",
    "status": "REFUNDING",
    "amount": "9.99"
  }
}
```

---

## 🐛 故障排除

### 问题：Server is not running

**解决方案：**
```bash
cd PayBackend
./build/Release/PayServer
```

### 问题：Connection refused

**原因：** PayServer 未启动或端口不正确

**解决方案：**
1. 检查 PayServer 是否正在运行
2. 检查 BASE_URL 是否正确
3. 检查防火墙设置

### 问题：401 Unauthorized

**原因：** API Key 不正确

**解决方案：**
```bash
export API_KEY="your-correct-api-key"
```

### 问题：测试失败但服务器运行正常

**可能原因：**
1. 数据库连接问题
2. WeChat Pay 配置问题
3. 服务初始化失败

**调试步骤：**
1. 查看 PayServer 日志
2. 检查数据库连接
3. 验证 WeChat Pay 配置

---

## 🔍 手动测试

如果你想手动测试特定端点：

### 测试创建支付

```bash
curl -X POST http://localhost:5566/pay/create \
  -H "Idempotency-Key: manual_test_$(date +%s)" \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "10001",
    "amount": "19.99",
    "currency": "CNY",
    "title": "Manual Test Order"
  }'
```

### 测试查询订单

```bash
curl "http://localhost:5566/pay/query?order_no=YOUR_ORDER_NO" \
  -H "X-Api-Key: test-api-key"
```

### 测试指标端点

```bash
curl "http://localhost:5566/pay/metrics/auth" \
  -H "X-Api-Key: test-api-key"
```

```bash
curl "http://localhost:5566/metrics"
```

---

## 📈 与集成测试的对比

| 特性 | 端到端测试 | 集成测试 |
|-----|-----------|---------|
| **时间投入** | 3-4 小时 | 9-13 小时 |
| **测试范围** | 真实 API 端点 | 内部服务方法 |
| **维护成本** | 低 | 中 |
| **运行速度** | 快 | 中 |
| **真实性** | 高（HTTP 层） | 中（服务层） |
| **覆盖范围** | 主要用户场景 | 所有边缘情况 |

### 何时使用端到端测试？

✅ **推荐使用：**
- 快速验证服务功能
- 测试真实 API 契约
- 回归测试主要功能
- CI/CD 管道中

❌ **不推荐使用：**
- 需要测试所有边缘情况
- 需要测试内部逻辑
- 需要测试错误处理（应该用集成测试）

---

## 🎯 最佳实践

### 1. 在 CI/CD 中运行

**.github/workflows/e2e-tests.yml:**
```yaml
name: E2E Tests

on: [push, pull_request]

jobs:
  e2e:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Start PayServer
        run: |
          cd PayBackend
          ./build/Release/PayServer &
          sleep 5
      - name: Run E2E Tests
        run: |
          cd PayBackend/test
          ./e2e_test.sh
```

### 2. 定期运行

建议：
- 每次代码合并前
- 每次发布前
- 每天自动运行（夜间构建）

### 3. 监控测试结果

- 记录测试失败历史
- 分析失败趋势
- 识别不稳定测试

---

## 📚 相关文档

- [Pay API Examples](pay-api-examples.md) - API 使用示例
- [TDD Tests Update Final Report](tdd_tests_update_final_report.md) - 测试更新总结
- [Validation Report](validation_report.md) - 重构验证报告

---

## 🎉 总结

端到端测试提供了一种快速、实用的方式来验证服务功能：

✅ **优势：**
- 快速实现（3-4 小时）
- 测试真实场景
- 易于维护
- 适合 CI/CD

⚠️ **局限：**
- 不覆盖所有边缘情况
- 依赖外部服务（数据库、WeChat）
- 需要服务器运行

🎯 **推荐使用场景：**
- 验证主要功能
- 回归测试
- 发布前验证
- CI/CD 集成

**下一步：** 根据项目需求，选择：
- **A.** 继续更新所有集成测试（完整覆盖）
- **B.** 使用端到端测试（当前方案，务实）
- **C.** 混合方案（端到端 + 关键集成测试）

---

**创建时间：** 2026-04-11
**脚本位置：** `test/e2e_test.sh` (Bash), `test/e2e_test.ps1` (PowerShell)
**状态：** ✅ 完成 - 可立即使用
