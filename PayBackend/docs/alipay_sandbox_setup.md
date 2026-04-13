# 支付宝沙箱环境配置指南

## 为什么使用支付宝沙箱？

**支付宝沙箱环境**是支付宝提供的免费开发测试环境，具有以下优势：

- ✅ **个人可注册** - 无需营业执照
- ✅ **完全免费** - 无需任何费用
- ✅ **功能完整** - 支持所有支付 API
- ✅ **真实模拟** - 模拟完整支付流程
- ✅ **易于测试** - 不需要真实资金

---

## 前置条件

1. 支付宝账号（个人账号即可）
2. 开发者平台账号（免费注册）

---

## 步骤 1：注册支付宝开放平台

### 1.1 访问支付宝开放平台

打开浏览器访问：https://open.alipay.com/

### 1.2 注册/登录

- 如果已有支付宝账号，直接登录
- 如果没有，点击"免费注册"创建账号

### 1.3 进入控制台

登录后，点击顶部导航栏的"控制台"

---

## 步骤 2：进入沙箱环境

### 2.1 找到沙箱入口

在控制台左侧菜单中，找到：

```
研发服务 -> 沙箱环境
```

或者直接访问：https://openhome.alipay.com/platform/appDaily.htm

### 2.2 沙箱环境说明

进入后你会看到：

- **沙箱应用信息**
- **沙箱商家账号**
- **沙箱买家账号**
- **密钥工具**

---

## 步骤 3：获取沙箱参数

### 3.1 沙箱应用信息

在沙箱环境页面，记录以下信息：

```
APPID：2021000000000000（示例）
支付宝网关：https://openapi.alipaydev.com/gateway.do
```

### 3.2 沙箱账号信息

记录商家账号和买家账号（用于测试）：

**商家账号（卖家）：**
```
商家邮箱：sandbox_seller@example.com
商家UID：2088102146225135（示例）
登录密码：111111
支付密码：111111
```

**买家账号：**
```
买家邮箱：sandbox_buyer@example.com
买家UID：2088102146225135（示例）
登录密码：111111
支付密码：111111
余额：999999.00元
```

**注意：** 这些账号只能在沙箱环境使用，无法用于真实支付。

---

## 步骤 4：生成密钥

### 4.1 下载密钥生成工具

在沙箱环境页面，找到"密钥工具"部分，下载：

- **Windows:** 密钥生成工具--windows.zip
- **Mac:** 密钥生成工具-mac.zip
- **Linux:** 使用 OpenSSL 命令

### 4.2 生成应用私钥和公钥

#### Windows/Mac 用户：

1. 解压下载的密钥工具
2. 运行"RSA签名验签工具.bat"
3. 选择密钥长度：**RSA2 (2048位)**
4. 点击"生成私钥"和"生成公钥"
5. 保存生成的密钥文件

#### Linux 用户（使用 OpenSSL）：

```bash
# 生成私钥
openssl genrsa -out app_private_key.pem 2048

# 提取公钥
openssl rsa -in app_private_key.pem -pubout -out app_public_key.pem

# 转换为 PKCS#8 格式（支付宝要求）
openssl pkcs8 -topk8 -inform PEM -in app_private_key.pem -outform PEM -nocrypt -out app_private_key_pkcs8.pem
```

### 4.3 保存密钥文件

创建证书目录并保存密钥：

```bash
mkdir -p PayBackend/certs/alipay

# 应用私钥（用于签名）
cp app_private_key_pkcs8.pem PayBackend/certs/alipay/app_private_key.pem

# 应用公钥（稍后上传到沙箱环境）
cp app_public_key.pem PayBackend/certs/alipay/app_public_key.pem
```

---

## 步骤 5：上传公钥到沙箱环境

### 5.1 复制应用公钥

打开 `PayBackend/certs/alipay/app_public_key.pem`，复制全部内容（包括 -----BEGIN PUBLIC KEY----- 和 -----END PUBLIC KEY-----）

### 5.2 上传到沙箱

1. 在沙箱环境页面，找到"设置应用公钥"部分
2. 将复制的公钥粘贴到输入框
3. 点击"保存"

### 5.3 查看支付宝公钥

保存后，页面会显示"查看支付宝公钥"按钮，点击查看并复制支付宝公钥。

将支付宝公钥保存为文件：

```bash
cat > PayBackend/certs/alipay/alipay_public_key.pem << 'EOF'
-----BEGIN PUBLIC KEY-----
你复制的支付宝公钥内容
-----END PUBLIC KEY-----
