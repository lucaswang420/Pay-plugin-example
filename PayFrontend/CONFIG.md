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
```

### 配置说明

- **VITE_DEFAULT_USER_ID**: 默认用户ID，用于创建订单
- **VITE_DEFAULT_API_KEY**: API密钥，用于后端认证

### 注意事项

⚠️ **重要**：
- `.env.local` 文件已在 `.gitignore` 中，不会被提交到git
- 不要将包含敏感信息的 `.env.local` 提交到代码仓库
- 每个开发者可以有自己的 `.env.local` 配置

### 验证配置

启动前端后，查看页面顶部是否显示：
```
👤 用户ID: 1 | API密钥: test_key_12...
```

如果显示此信息，说明配置成功。
