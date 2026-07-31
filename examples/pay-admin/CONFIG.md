# 前端配置说明

## 环境配置

### 开发/测试环境

1. 复制环境变量模板：
```bash
cp .env.example .env.local
```

2. 编辑 `.env.local` 文件，配置以下变量：
```env
VITE_DEFAULT_USER_ID=1
VITE_DEFAULT_API_KEY=test_key_123456
VITE_API_BASE_URL=/api
```

### 配置说明

- **VITE_DEFAULT_USER_ID**: 默认用户 ID，创建支付时的 `user_id` 预填值
- **VITE_DEFAULT_API_KEY**: 默认 API 密钥，应用启动时自动载入（可在顶栏"设置"中覆盖）
- **VITE_API_BASE_URL**: API 前缀，默认 `/api`（开发模式经 Vite 代理转发到
  `http://localhost:5566`）。后端接口前缀由 pay-server 中 PayPlugin 的
  `base_path` 配置决定（默认 `/api/pay`）；宿主若修改 `base_path`，需同步调整此值。

### 注意事项

**重要**：
- `.env.local` 文件已在 `.gitignore` 中，不会被提交到 git
- 不要将包含敏感信息的 `.env.local` 提交到代码仓库
- 每个开发者可以有自己的 `.env.local` 配置

### 验证配置

启动前端后，查看顶栏右侧的状态标签：

- 显示绿色 **API Key 已配置** 即配置成功
- 显示红色 **API Key 未配置** 时，点击旁边的"设置"按钮手动填入

运行时修改的 Key 存放在 `sessionStorage`，关闭标签页后回退到 `.env.local` 默认值。
