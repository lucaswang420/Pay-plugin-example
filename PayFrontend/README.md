# PayFrontend - 支付系统前端管理界面

基于 Vue 3 + Element Plus 的现代化支付管理界面。

## 技术栈

- **框架:** Vue 3 (Composition API)
- **UI组件库:** Element Plus
- **状态管理:** Pinia
- **HTTP客户端:** Axios
- **构建工具:** Vite
- **路由:** Vue Router

## 功能特性

### 1. 订单创建 (Step 1-2)
- 用户认证（API Key）
- 订单信息录入
- 支付渠道选择（支付宝/微信）
- 实时金额验证

### 2. 支付处理 (Step 3)
- 支付宝二维码展示
- 支付状态轮询（5秒间隔，智能错误处理）
- 支付超时检测
- 状态同步功能

### 3. 订单管理
- 订单列表查询
- 订单详情查看
- 状态筛选和搜索
- 从支付宝同步最新状态

### 4. 退款处理 (Step 4-5)
- 退款申请表单
- 退款金额验证
- 退款状态跟踪
- 退款历史查询

## 快速开始

### 环境要求

- Node.js 16+
- npm 或 yarn

### 安装依赖

```bash
npm install
```

### 配置环境变量

创建 `.env.local` 文件：

```env
VITE_DEFAULT_USER_ID=2088722100150330
VITE_DEFAULT_API_KEY=your_api_key_here
VITE_API_BASE_URL=http://localhost:5566
```

### 启动开发服务器

```bash
npm run dev
```

访问 http://localhost:5173

### 构建生产版本

```bash
npm run build
```

## 项目结构

```
PayFrontend/
├── src/
│   ├── views/
│   │   └── StepView.vue        # 主界面（5步流程）
│   ├── components/             # 可复用组件
│   ├── api/
│   │   └── index.js            # API 客户端
│   ├── stores/
│   │   ├── user.js             # 用户认证状态
│   │   └── payment.js          # 支付流程状态
│   ├── utils/
│   │   ├── request.js          # HTTP 请求封装
│   │   └── constants.js        # 常量定义
│   ├── App.vue
│   └── main.js
├── public/                     # 静态资源
├── .env.example                # 环境变量示例
├── .env.local                  # 本地环境变量（不提交）
├── index.html
├── vite.config.js
└── package.json
```

## 主要功能说明

### 1. 用户认证

前端使用 API Key 进行认证，支持两种方式：

1. **环境变量配置** - 通过 `.env.local` 设置默认值
2. **用户手动输入** - 界面上输入 API Key

API Key 存储在 `sessionStorage` 中，刷新页面后需要重新登录。

### 2. 支付流程

#### Step 1: 创建订单
- 输入用户ID（如果有默认值则自动填充）
- 输入订单金额
- 选择支付标题
- 点击"创建订单"按钮

#### Step 2: 选择支付方式
- 选择支付宝或微信支付
- 确认订单信息

#### Step 3: 完成支付
- **支付宝**: 展示二维码，扫描支付
- **微信**: 展示支付链接
- 系统自动轮询支付状态（每5秒）
- 支付成功后自动跳转到下一步

#### Step 4: 申请退款（可选）
- 查看订单详情
- 输入退款金额
- 填写退款原因
- 提交退款申请

#### Step 5: 查看结果
- 查看退款状态
- 查看退款金额
- 可以返回创建新订单

### 3. 订单管理

- **订单列表**: 显示所有订单
- **状态筛选**: 按状态过滤订单
- **订单详情**: 点击查看详细信息
- **同步状态**: 从支付宝同步最新状态

### 4. 轮询机制

支付状态轮询特性：

- **智能错误处理**: 连续失败3次后停止轮询
- **请求去重**: 防止重复请求
- **超时保护**: 避免无限轮询
- **状态同步**: 实时更新支付状态

## API 接口

前端使用的主要 API 接口：

```javascript
// 用户认证
POST /api/auth/metrics

// 创建支付订单
POST /api/payments

// 查询支付状态
GET /api/payments/{payment_no}

// 同步支付宝状态
POST /api/payments/{payment_no}/sync

// 创建退款
POST /api/refunds

// 查询订单列表
GET /api/orders?user_id={userId}&status={status}
```

## 错误处理

### 常见错误及解决方案

1. **请先登录或提供API密钥**
   - 原因: API Key 未设置或已过期
   - 解决: 在 Step 1 输入有效的 API Key

2. **网络请求失败**
   - 原因: 后端服务未启动或网络问题
   - 解决: 检查后端服务是否运行在 5566 端口

3. **支付超时**
   - 原因: 支付宝响应超时或网络问题
   - 解决: 使用"Sync from Alipay"按钮手动同步

4. **轮询失败**
   - 原因: 后端服务异常或网络中断
   - 解决: 系统会自动停止轮询，可手动刷新状态

## 开发建议

### 添加新功能

1. 在 `src/views/StepView.vue` 中添加新的步骤
2. 在 `src/api/index.js` 中添加新的 API 接口
3. 在 `src/stores/` 中添加新的状态管理（如需要）

### 样式定制

- 修改 `src/assets/styles/` 中的样式文件
- Element Plus 主题配置在 `main.js` 中

### 调试技巧

- 打开浏览器控制台查看详细日志
- 使用 Vue DevTools 查看组件状态
- 检查 Network 标签查看 API 请求

## 部署

### 生产环境配置

1. 修改 `.env.production`:
```env
VITE_API_BASE_URL=https://your-api-domain.com
```

2. 构建生产版本:
```bash
npm run build
```

3. 部署 `dist/` 目录到 Web 服务器

### Nginx 配置示例

```nginx
server {
    listen 80;
    server_name your-frontend-domain.com;
    root /path/to/dist;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api {
        proxy_pass http://backend:5566;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

## 浏览器兼容性

- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+

## 许可证

MIT License
