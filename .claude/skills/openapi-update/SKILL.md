---
name: openapi-update
description: 当支付 API 端点发生变化时更新 OpenAPI 规范
---

# OpenAPI 规范更新技能

当支付控制器端点发生变化时更新 OpenAPI 3.0 规范文档。

## 使用方法

- Claude 自动调用：当检测到 `libs/drogon-pay/src/handlers/` 中的路由变更时
- 用户调用：`/openapi-update`

## 工作流程

1. **分析当前控制器**
   - 读取 `libs/drogon-pay/src/handlers/*.cc`
   - 识别所有路由端点和参数

2. **比较现有 OpenAPI 规范**
   - 读取 `openapi.yaml`
   - 检查是否有新的端点
   - 检查是否有参数变更
   - 检查是否有响应格式变更

3. **更新 OpenAPI 规范**
   - 添加新的端点定义
   - 更新现有端点的参数
   - 更新响应模型
   - 确保符合 OpenAPI 3.0 规范

4. **验证规范**
   - 检查 YAML 语法
   - 验证所有引用是否有效
   - 确保端点路径与代码一致

### 验证脚本

```powershell
# 检查 YAML 语法
try {
    $yaml = Get-Content "openapi.yaml" -Raw
    Write-Host "YAML syntax valid"
} catch {
    Write-Host "YAML syntax error: $_"
    exit 1
}

# 检查必需字段
$requiredFields = @("openapi", "info", "paths", "components")
foreach ($field in $requiredFields) {
    if ($yaml -match "$field:") {
        Write-Host "Field '$field' found"
    } else {
        Write-Host "Required field '$field' missing"
        exit 1
    }
}
```

## 需要检查的关键端点

### 支付端点
- `POST /api/v1/payments` - 创建支付
- `GET /api/v1/payments/{id}` - 查询支付
- `POST /api/v1/refunds` - 创建退款
- `GET /api/v1/refunds/{id}` - 查询退款

### 回调端点
- `POST /api/v1/callbacks/alipay` - 支付宝回调
- `POST /api/v1/callbacks/wechat` - 微信支付回调

### 管理端点
- `GET /health` - 健康检查
- `GET /metrics` - Prometheus 指标
- `GET /api/v1/metrics/payments` - 支付统计

### 对账端点
- `POST /api/v1/reconcile` - 触发对账
- `GET /api/v1/reconcile/summary` - 对账摘要

## 输出格式

更新后的 `openapi.yaml` 文件应包含：
- 正确的 OpenAPI 3.0 版本
- 所有端点的完整文档
- 请求参数 schema（含 `X-Api-Key` Header）
- 响应格式定义
- 错误响应示例
- 认证方式说明

## 注意事项

- 保持 YAML 缩进一致（2 个空格）
- 所有端点需要包含描述文字
- 参数需要标注是否必需
- 提供请求和响应示例
- 更新版本号当有重大变更
- 金额字段标注单位为"分"

## 版本控制集成

```bash
# 更新规范后提交到 Git
git add openapi.yaml
git commit -m "docs: update OpenAPI specification for endpoint changes"

# 如果有重大变更，更新 API 版本号
# 在 openapi.yaml 的 info.version 字段中递增版本
```

## 文档同步

```bash
# 确保相关文档也同步更新
# - docs/api_reference.md
# - README.md 中的 API 端点示例
# - 技术文档中的接口描述
```
