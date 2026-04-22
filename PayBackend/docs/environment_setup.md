# 环境变量配置指南

## 概述

本项目使用环境变量来管理敏感配置信息，确保 config.json 可以安全提交到 Git 仓库，而敏感数据保留在本地。

## 文件说明

- **config.json** - 配置文件（可提交到 Git）
  - 使用占位符格式：`__env_var:VARIABLE_NAME__`
  - 不包含真实敏感数据

- **.env.example** - 环境变量模板（可提交到 Git）
  - 包含所有可配置的环境变量
  - 所有值都是占位符/示例
  - 团队成员可以复制此文件作为起点

- **.env** - 本地环境变量文件（不提交到 Git）
  - 包含真实的敏感配置
  - 在 .gitignore 中，不会被提交
  - 每个开发者/环境都有自己的副本

## 快速开始

### 1. 复制模板文件

```bash
cd PayBackend
cp .env.example .env
```

### 2. 编辑 .env 文件

根据你的环境填入实际值：

```bash
# Windows
notepad .env

# Linux/Mac
nano .env
```

### 3. 启动服务

```bash
./build/Release/PayServer.exe
```

程序会自动：
1. 加载 .env 文件中的环境变量
2. 替换 config.json 中的占位符
3. 使用实际配置初始化服务

## 环境变量列表

### 支付宝沙箱配置

| 环境变量 | 说明 | 获取方式 |
|---------|------|---------|
| `ALIPAY_SANDBOX_APP_ID` | 沙箱应用ID | [支付宝开放平台](https://open.alipay.com/) |
| `ALIPAY_SANDBOX_SELLER_ID` | 沙箱卖家ID | 沙箱环境页面 |
| `ALIPAY_SANDBOX_PRIVATE_KEY_PATH` | 应用私钥路径 | 密钥生成器生成 |
| `ALIPAY_SANDBOX_PUBLIC_KEY_PATH` | 支付宝公钥路径 | 沙箱环境下载 |
| `ALIPAY_SANDBOX_GATEWAY_URL` | 沙箱网关地址 | 默认值即可 |

**获取步骤：**
1. 访问 https://open.alipay.com/ 注册账号
2. 进入"研发服务" → "沙箱环境"
3. 获取沙箱 AppID 和密钥
4. 下载密钥生成器生成 RSA2 密钥对
5. 上传公钥到沙箱，下载支付宝公钥

### 微信支付配置

| 环境变量 | 说明 | 获取方式 |
|---------|------|---------|
| `WECHAT_PAY_APP_ID` | 微信应用ID | [微信支付商户平台](https://pay.weixin.qq.com/) |
| `WECHAT_PAY_MCH_ID` | 微信商户号 | 商户平台 |
| `WECHAT_PAY_SERIAL_NO` | 证书序列号 | 商户平台下载 |
| `WECHAT_PAY_API_V3_KEY` | API v3 密钥 | 商户平台设置 |
| `WECHAT_PAY_PRIVATE_KEY_PATH` | 商户私钥路径 | 商户平台下载 |
| `WECHAT_PAY_PLATFORM_CERT_PATH` | 平台证书路径 | API下载 |
| `WECHAT_PAY_NOTIFY_URL` | 支付回调URL | 你的服务器地址 |

**注意：** 微信支付需要企业资质和营业执照。

### 数据库配置

| 环境变量 | 说明 | 示例 |
|---------|------|------|
| `POSTGRES_PASSWORD` | PostgreSQL密码 | 使用强密码 |

### Redis 配置

| 环境变量 | 说明 | 示例 |
|---------|------|------|
| `REDIS_PASSWORD` | Redis密码 | 使用强密码 |

### API 密钥配置

| 环境变量 | 说明 | 格式 |
|---------|------|------|
| `PAY_API_KEYS` | API密钥列表 | 逗号分隔：key1,key2,key3 |

## 不同环境配置

### 开发环境

```bash
# .env
ALIPAY_SANDBOX_APP_ID=dev_app_id
POSTGRES_PASSWORD=dev_password
```

### 测试环境

```bash
# .env.test
ALIPAY_SANDBOX_APP_ID=test_app_id
POSTGRES_PASSWORD=test_password
```

### 生产环境

方式 1：使用 .env 文件
```bash
# .env.production
ALIPAY_SANDBOX_APP_ID=prod_app_id
POSTGRES_PASSWORD=<strong_password>
```

方式 2：使用系统环境变量
```bash
export ALIPAY_SANDBOX_APP_ID=prod_app_id
export POSTGRES_PASSWORD=<strong_password>
./PayServer.exe
```

## 安全最佳实践

1. **永远不要提交 .env 文件**
   - .gitignore 已配置忽略 .env
   - 提交前检查：`git status`

2. **使用强密码**
   - 数据库和 Redis 密码至少 16 位
   - 包含大小写字母、数字、特殊字符

3. **定期轮换密钥**
   - API 密钥定期更换
   - 数据库密码定期更新

4. **限制 .env 文件权限**
   ```bash
   # Linux/Mac
   chmod 600 .env
   
   # Windows
   # 右键 → 属性 → 安全 → 编辑权限
   ```

5. **不同环境使用不同密钥**
   - 开发、测试、生产环境密钥隔离
   - 不要跨环境共享密钥

## 故障排查

### 问题：服务启动失败，提示配置缺失

**解决方案：**
1. 检查 .env 文件是否存在
2. 确认 .env 文件格式正确（key=value）
3. 验证必需的环境变量已设置

### 问题：环境变量未生效

**解决方案：**
1. 检查环境变量名称是否正确（区分大小写）
2. 确认 .env 文件在程序启动目录
3. 查看启动日志中的 "Environment variables loaded" 消息

### 问题：config.json 占位符未替换

**解决方案：**
1. 确认占位符格式：`__env_var:VAR_NAME__`
2. 检查环境变量是否已设置
3. 查看日志中的 "Configuration placeholders replaced" 消息

## 示例

### 完整的 .env 文件示例

```bash
# 支付宝沙箱
ALIPAY_SANDBOX_APP_ID=your-app-id
ALIPAY_SANDBOX_SELLER_ID=your-seller-id
ALIPAY_SANDBOX_PRIVATE_KEY_PATH=./certs/alipay_app_private_key.pem
ALIPAY_SANDBOX_PUBLIC_KEY_PATH=./certs/alipay_public_key.pem
ALIPAY_SANDBOX_GATEWAY_URL=https://openapi-sandbox.dl.alipaydev.com/gateway.do

# 数据库
POSTGRES_PASSWORD=MyStr0ngP@ssw0rd!2024

# Redis
REDIS_PASSWORD=MyStr0ngR3disP@ssw0rd!2024

# API 密钥
PAY_API_KEYS=dev-key-12345,test-key-67890,admin-key-abcde
```

## 参考链接

- [支付宝沙箱环境](https://opendocs.alipay.com/common/02kkv7)
- [微信支付接入指南](https://pay.weixin.qq.com/wiki/doc/apiv3/open/pay/chapter2_8_2.shtml)
- [PostgreSQL 密码安全](https://www.postgresql.org/docs/current/password-security.html)
- [Redis 安全配置](https://redis.io/topics/security)
