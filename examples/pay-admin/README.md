# pay-admin — 示例宿主配套管理控制台

基于 Vue 3 + Element Plus 的支付管理控制台，是 [examples/pay-server](../pay-server/)
示例宿主的**配套演示层**：它消费宿主上 drogon-pay 插件注册的 `/api/pay/*` 接口
（即 PayPlugin 默认的 `base_path` 前缀）。

> 定位说明：本目录属于示例/演示层，**不是 drogon-pay 库发布物**，不进 Conan 包，
> 也不阻塞库的 CI。若宿主修改了 PayPlugin 的 `base_path` 配置，需同步调整本前端
> 的 API 前缀（见 [CONFIG.md](CONFIG.md) 与
> [docs/development/plugin_integration.md](../../docs/development/plugin_integration.md)）。

## 技术栈

- **框架:** Vue 3 (Composition API, `<script setup>`)
- **UI 组件库:** Element Plus（unplugin 按需引入）
- **状态管理:** Pinia
- **HTTP 客户端:** Axios（统一拦截器归一化响应）
- **构建工具:** Vite（vendor 手动分包）
- **路由:** Vue Router（懒加载 + 嵌套布局）

## 功能页面

管理台为侧边导航布局（`src/layouts/AdminLayout.vue`），四个业务页面：

| 路由 | 页面 | 说明 |
|---|---|---|
| `/payments/create` | 创建支付 | 渠道选择、金额校验、二维码展示、支付状态自动轮询（指数退避） |
| `/orders` | 订单管理 | 状态筛选 / 用户搜索 / limit+1 探测法翻页（后端无 total 字段） |
| `/orders/:orderNo` | 订单详情 | 实时渠道状态同步、渠道原始响应 JSON 折叠展示、一键发起退款 |
| `/refunds` | 退款管理 | 全额/部分退款、退款进度轮询（REFUND_INIT → REFUNDING → 终态）、历史退款查询 |
| `/dashboard` | 对账与指标 | 对账摘要统计卡（按日期）+ 认证指标卡，30 秒自动刷新 |

顶栏提供 API Key 状态指示与设置对话框（存 `sessionStorage`），无需独立登录页。

## 快速开始

### 环境要求

- Node.js 18+
- 本地运行中的 pay-server（默认 5566 端口）

### 安装与启动

```bash
npm install
cp .env.example .env.local   # 按需修改默认 API Key / 用户 ID
npm run dev                  # http://localhost:5173
```

开发模式下 `/api` 请求经 Vite 代理转发到 `http://localhost:5566`，不涉及 CORS。

### 构建生产版本

```bash
npm run build   # 产物在 dist/，vendor 分包：vue-vendor / element-plus / qrcode
```

## 项目结构

```
pay-admin/
├── src/
│   ├── api/
│   │   ├── http.js             # Axios 实例 + 统一拦截器（鉴权注入、错误归一化、降级标记）
│   │   └── pay.js              # 7 个后端端点函数（全部支持 AbortSignal）
│   ├── composables/
│   │   └── usePolling.js       # 统一轮询原语（指数退避/页面隐藏暂停/自动 abort）
│   ├── config/
│   │   └── channels.js         # 支付渠道配置（新渠道零代码接入）
│   ├── layouts/
│   │   └── AdminLayout.vue     # 侧边导航 + 顶栏（API Key 设置）
│   ├── router/index.js         # 懒加载路由
│   ├── stores/user.js          # API Key / 用户 ID（sessionStorage 持久化）
│   ├── styles/index.css        # 全局 reset + 主题变量
│   ├── utils/
│   │   ├── format.js           # 金额 / 时间格式化
│   │   └── status.js           # 订单与退款状态映射表（与后端词表严格一致）
│   ├── views/
│   │   ├── payments/CreatePaymentView.vue
│   │   ├── orders/OrderListView.vue
│   │   ├── orders/OrderDetailView.vue
│   │   ├── refunds/RefundView.vue
│   │   └── dashboard/DashboardView.vue
│   ├── App.vue                 # ElConfigProvider（中文 locale）
│   └── main.js
├── .env.example
├── vite.config.js              # Element Plus 按需引入 + manualChunks
└── package.json
```

## 后端契约要点

- 响应包裹 `{code, message, data}`；`code` 为 `0` 或 `200` 表示成功，`1` 表示
  渠道查询降级（拦截器透传 data 并附 `degraded: true`），页面据此提示"以本地状态为准"。
- 鉴权失败返回**纯文本** body（401/403/503），拦截器按 HTTP status 归一化中文提示。
- 订单状态词表：`PAYING → PAID / FAILED`；退款状态词表：
  `REFUND_INIT → REFUNDING → REFUND_SUCCESS / REFUND_FAIL`。
- `/api/pay/orders` 无 total 字段，列表采用 limit+1 探测法实现上一页/下一页。
- 支付宝退款状态以数据库快照为准（后端仅微信渠道实时刷新退款状态）。

## 四大场景操作指引

1. **创建支付并轮询到终态**：创建支付 → 展示二维码（支付宝取 `qr_code`，微信取
   `code_url`）→ 自动轮询 `/pay/query`（3s 起指数退避至 15s，页面隐藏时暂停）→
   支付成功显示成功页，可跳转订单详情。
2. **订单管理**：列表按状态 / 用户 ID 筛选，点击行进入详情；详情页实时调
   `/pay/query` 同步渠道状态，渠道原始响应折叠展示。
3. **退款**：订单详情"发起退款"按钮预填订单号与金额（全额），或手动填写部分退款；
   提交后自动轮询退款进度，支持输入 refund_no 查询历史退款。
4. **对账与指标**：Dashboard 选日期查看对账摘要（滞留支付/退款统计），下排为
   认证指标计数（missing_key / invalid_key / scope_denied / not_configured）。

## 错误处理

| 现象 | 原因 | 解决 |
|---|---|---|
| 提示 "API Key 缺失或无效" | 未设置或 Key 错误 | 顶栏"设置"填入有效 Key（默认 `test_key_123456`） |
| 提示 "权限不足（scope）" | Key 的 scope 不含该操作 | 检查 pay-server `config.json` 的 `api_key_default_scopes` |
| 提示 "网络异常" | pay-server 未启动 | 确认后端运行在 5566 端口 |
| 轮询转手动刷新 | 达到轮询次数上限（60 次） | 点击"手动刷新"按钮继续查询 |

## 部署

构建后将 `dist/` 部署到任意静态服务器，`/api` 需反向代理到 pay-server：

```nginx
server {
    listen 80;
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

## 许可证

MIT License
