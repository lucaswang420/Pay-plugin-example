# 完整支付系统前端实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展现有支付系统前端，实现完整的订单管理、退款申请和状态查询功能。

**Architecture:** 保持单一 StepView.vue 组件架构，通过 stepNumber prop 渲染不同步骤；扩展 Pinia store 管理订单列表和退款信息；API 层封装后端接口调用。

**Tech Stack:** Vue 3 (Composition API), Pinia, Vue Router, Axios, qrcode.js

---

## 文件结构

### 需要修改的文件
- `PayFrontend/src/views/StepView.vue` - 扩展为6个步骤
- `PayFrontend/src/stores/order.js` - 扩展状态管理
- `PayFrontend/src/api/payment.js` - 扩展 API 接口

### 文件职责
- **StepView.vue**: 单一组件，根据 stepNumber 渲染不同步骤 UI，处理所有用户交互
- **order.js**: 管理当前订单、订单列表、退款信息、筛选条件
- **payment.js**: 封装所有后端 API 调用

---

## Task 1: 扩展 API 层

**Files:**
- Modify: `PayFrontend/src/api/payment.js`

- [ ] **Step 1: 添加订单列表查询接口**

```javascript
// 在 paymentApi 对象中添加新方法
queryOrderList(params) {
  console.log('[API] Querying order list:', params)
  return apiClient.get('/pay/orders', { params })
}
```

- [ ] **Step 2: 验证语法**

打开浏览器控制台，确认没有语法错误。

- [ ] **Step 3: 提交变更**

```bash
cd PayFrontend
git add src/api/payment.js
git commit -m "feat(api): 添加订单列表查询接口"
```

---

## Task 2: 扩展 Store - 订单列表管理

**Files:**
- Modify: `PayFrontend/src/stores/order.js`

- [ ] **Step 1: 添加订单列表相关状态**

在 `defineStore` 回调中，在现有状态后添加：

```javascript
// 订单列表
const orders = ref([])
const orderFilter = ref('all')
```

- [ ] **Step 2: 添加订单列表管理 actions**

在 return 语句前添加：

```javascript
function setOrders(ordersData) {
  orders.value = ordersData
}

function updateOrderInList(updatedOrder) {
  const index = orders.value.findIndex(o => o.order_no === updatedOrder.order_no)
  if (index !== -1) {
    orders.value[index] = updatedOrder
  }
}

function setOrderFilter(filter) {
  orderFilter.value = filter
}
```

- [ ] **Step 3: 导出新增的状态和 actions**

在 return 对象中添加：

```javascript
return {
  // ... 现有导出
  orders,
  orderFilter,
  setOrders,
  updateOrderInList,
  setOrderFilter,
  // ...
}
```

- [ ] **Step 4: 提交变更**

```bash
git add src/stores/order.js
git commit -m "feat(store): 添加订单列表状态管理"
```

---

## Task 3: 扩展 Store - 退款管理

**Files:**
- Modify: `PayFrontend/src/stores/order.js`

- [ ] **Step 1: 添加退款相关状态**

在订单列表状态后添加：

```javascript
// 退款信息
const refundInfo = ref(null)

// 当前查看的订单（Step 6）
const currentOrder = ref(null)
```

- [ ] **Step 2: 添加退款管理 actions**

在 return 语句前添加：

```javascript
function setRefundInfo(info) {
  refundInfo.value = info
}

function clearRefundInfo() {
  refundInfo.value = null
}

function setCurrentOrder(order) {
  currentOrder.value = order
}

function clearCurrentOrder() {
  currentOrder.value = null
}
```

- [ ] **Step 3: 导出新增的状态和 actions**

在 return 对象中添加：

```javascript
return {
  // ... 现有导出
  refundInfo,
  currentOrder,
  setRefundInfo,
  clearRefundInfo,
  setCurrentOrder,
  clearCurrentOrder
}
```

- [ ] **Step 4: 提交变更**

```bash
git add src/stores/order.js
git commit -m "feat(store): 添加退款和当前订单状态管理"
```

---

## Task 4: 优化 Step 1 - 创建订单

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 修改 handleCreateOrder 函数**

找到 `handleCreateOrder` 函数，修改成功后的逻辑：

```javascript
async function handleCreateOrder() {
  loading.value = true
  error.value = ''

  try {
    // 创建订单
    const response = await paymentApi.createOrder({
      order_no: "ORDER-" + Date.now(),
      amount: String(form.value.amount),
      description: form.value.description,
      channel: "alipay",
      user_id: form.value.userId
    })

    const orderData = response.data || response
    const alipayData = orderData.alipay_response || {}

    // 保存订单信息
    orderStore.setOrder({
      orderId: orderData.order_no || alipayData.out_trade_no,
      productName: form.value.productName,
      amount: form.value.amount,
      description: form.value.description,
      status: orderData.status || 'PENDING',
      tradeNo: alipayData.trade_no || '',
      paymentNo: orderData.payment_no || ''
    })

    // 立即生成二维码
    await generateQRCode()

    // 自动跳转到 Step 2
    router.push('/step/2')

  } catch (err) {
    error.value = err.message || 'Failed to create order'
  } finally {
    loading.value = false
  }
}
```

- [ ] **Step 2: 提取 generateQRCode 函数**

在 `handleCreateOrder` 函数后添加：

```javascript
async function generateQRCode() {
  try {
    const response = await paymentApi.createQRPayment({
      order_no: orderStore.orderId,
      amount: orderStore.amount,
      channel: 'alipay',
      user_id: 1,
      subject: orderStore.productName || 'Payment'
    })

    if (response.data && response.data.qr_code) {
      qrCodeUrl.value = response.data.qr_code
    } else {
      throw new Error('No QR code in response')
    }
  } catch (err) {
    error.value = err.message || 'Failed to generate QR code'
    throw err
  }
}
```

- [ ] **Step 3: 更新表单按钮文案**

找到 Step 1 的提交按钮，修改：

```html
<button type="submit" :disabled="loading">
  {{ loading ? 'Creating...' : 'Create Order & Generate QR Code' }}
</button>
```

- [ ] **Step 4: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step1): 创建订单后自动生成二维码并跳转"
```

---

## Task 5: 优化 Step 2 - 支付成功展示

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 添加支付成功展示 UI**

在 Step 2 的模板中，二维码容器后添加：

```vue
<!-- 支付成功提示 -->
<div v-if="orderStore.status === 'PAID'" class="success-message">
  <div class="success-icon">✓</div>
  <h3>Payment Completed Successfully!</h3>
  <div class="success-details">
    <div class="detail-row">
      <span class="detail-label">Order No:</span>
      <span class="detail-value">{{ orderStore.orderId }}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Trade No:</span>
      <span class="detail-value">{{ orderStore.tradeNo }}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Amount:</span>
      <span class="detail-value amount">¥{{ orderStore.amount }}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Payment Time:</span>
      <span class="detail-value">{{ paymentTime }}</span>
    </div>
  </div>
</div>
```

- [ ] **Step 2: 添加支付成功操作按钮**

修改 Step 2 的按钮组：

```vue
<div class="button-group">
  <button
    type="button"
    @click="handleCheckPayment"
    :disabled="loading || orderStore.status === 'PAID'"
    v-if="orderStore.status !== 'PAID'"
  >
    {{ loading ? 'Checking...' : 'Check Payment Status' }}
  </button>

  <button
    type="button"
    @click="handleViewOrderDetail"
    class="primary"
    v-if="orderStore.status === 'PAID'"
  >
    View Order Details
  </button>

  <button
    type="button"
    @click="handleGoToRefund"
    class="warning"
    v-if="orderStore.status === 'PAID'"
  >
    Apply for Refund
  </button>

  <button type="button" @click="handleCreateNewOrder" class="secondary">
    Create New Order
  </button>
</div>
```

- [ ] **Step 3: 添加支付时间响应式变量**

在 script setup 中添加：

```javascript
const paymentTime = ref('')
```

- [ ] **Step 4: 在轮询成功回调中设置支付时间**

修改 `startPolling` 函数中的成功处理：

```javascript
if (response.data.status === 'PAID') {
  stopPolling()
  paymentTime.value = new Date().toLocaleString('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }).replace(/\//g, '-')
  console.log('[POLLING] Payment completed!')
}
```

- [ ] **Step 5: 添加操作按钮处理函数**

在 script setup 中添加：

```javascript
function handleViewOrderDetail() {
  orderStore.setCurrentOrder({
    order_no: orderStore.orderId,
    productName: orderStore.productName,
    amount: orderStore.amount,
    description: orderStore.description,
    status: orderStore.status,
    tradeNo: orderStore.tradeNo,
    createdAt: orderStore.createdAt
  })
  router.push('/step/6')
}

function handleGoToRefund() {
  orderStore.setCurrentOrder({
    order_no: orderStore.orderId,
    productName: orderStore.productName,
    amount: orderStore.amount,
    tradeNo: orderStore.tradeNo
  })
  router.push('/step/4')
}

function handleCreateNewOrder() {
  stopPolling()
  orderStore.clear()
  orderStore.clearCurrentOrder()
  qrCodeUrl.value = ''
  paymentTime.value = ''
  form.value = {
    productName: '',
    amount: '',
    description: '',
    userId: 1
  }
  router.push('/step/1')
}
```

- [ ] **Step 6: 添加成功消息样式**

在 style 部分添加：

```css
.success-icon {
  font-size: 48px;
  color: #28a745;
  margin-bottom: 16px;
}

.success-details {
  margin-top: 20px;
  text-align: left;
}

.detail-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #e0e0e0;
}

.detail-row:last-child {
  border-bottom: none;
}

.detail-label {
  font-weight: bold;
  color: #666666;
}

.detail-value {
  color: #000000;
}

.detail-value.amount {
  font-size: 18px;
  font-weight: bold;
  color: #0066cc;
}

button.warning {
  background-color: #ffc107;
  color: #000000;
}

button.warning:hover:not(:disabled) {
  background-color: #e0a800;
}

button.primary {
  background-color: #28a745;
  color: #ffffff;
}

button.primary:hover:not(:disabled) {
  background-color: #218838;
}
```

- [ ] **Step 7: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step2): 添加支付成功展示和操作按钮"
```

---

## Task 6: 实现 Step 3 - 订单列表

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 添加 Step 3 模板结构**

在 StepView 模板中，Step 2 后添加：

```vue
<!-- Step 3: 订单列表 -->
<div v-else-if="stepNumber === '3'">
  <h2>My Orders</h2>

  <!-- 筛选栏 -->
  <div class="filter-bar">
    <div class="filter-group">
      <label>Status Filter:</label>
      <select v-model="orderStore.orderFilter" @change="handleFilterChange">
        <option value="all">All Orders</option>
        <option value="PENDING">Pending Payment</option>
        <option value="PAID">Paid</option>
        <option value="REFUNDED">Refunded</option>
        <option value="FAILED">Failed</option>
      </select>
    </div>

    <button @click="handleRefreshOrders" :disabled="loading" class="refresh-btn">
      {{ loading ? 'Refreshing...' : 'Refresh' }}
    </button>
  </div>

  <!-- 订单列表 -->
  <div v-if="loading && !orderStore.orders.length" class="loading-message">
    Loading orders...
  </div>

  <div v-else-if="error" class="error-message">{{ error }}</div>

  <div v-else-if="!orderStore.orders.length" class="empty-message">
    <p>No orders found. Create your first order!</p>
    <button @click="handleCreateNewOrder" class="primary">Create Order</button>
  </div>

  <div v-else class="order-list">
    <div
      v-for="order in orderStore.orders"
      :key="order.order_no"
      class="order-card"
      :class="getStatusClass(order.status)"
    >
      <div class="order-card-header">
        <div class="order-no">{{ order.order_no }}</div>
        <span :class="['status-badge', getStatusClass(order.status)]">
          {{ formatStatus(order.status) }}
        </span>
      </div>

      <div class="order-card-body">
        <div class="order-info">
          <div class="info-row">
            <span class="label">Product:</span>
            <span class="value">{{ order.title || 'N/A' }}</span>
          </div>
          <div class="info-row">
            <span class="label">Amount:</span>
            <span class="value amount">¥{{ order.amount }}</span>
          </div>
          <div class="info-row" v-if="order.trade_no">
            <span class="label">Trade No:</span>
            <span class="value">{{ order.trade_no }}</span>
          </div>
          <div class="info-row">
            <span class="label">Created:</span>
            <span class="value">{{ formatDate(order.created_at) }}</span>
          </div>
        </div>

        <div class="order-actions">
          <button
            @click="handleViewOrderDetailFromList(order)"
            class="action-btn primary"
          >
            View Details
          </button>
          <button
            v-if="order.status === 'PAID'"
            @click="handleGoToRefundFromList(order)"
            class="action-btn warning"
          >
            Refund
          </button>
        </div>
      </div>
    </div>
  </div>

  <div class="button-group">
    <button @click="handleCreateNewOrder" class="secondary">
      Create New Order
    </button>
  </div>
</div>
```

- [ ] **Step 2: 添加订单列表响应式变量**

在 script setup 中添加：

```javascript
const listLoading = ref(false)
const listError = ref('')
```

- [ ] **Step 3: 添加订单列表处理函数**

```javascript
async function loadOrders() {
  listLoading.value = true
  listError.value = ''

  try {
    const params = {
      user_id: 1, // 默认用户ID
      status: orderStore.orderFilter
    }

    const response = await paymentApi.queryOrderList(params)
    orderStore.setOrders(response.data.orders || [])
  } catch (err) {
    listError.value = err.message || 'Failed to load orders'
  } finally {
    listLoading.value = false
  }
}

function handleFilterChange() {
  loadOrders()
}

function handleRefreshOrders() {
  loadOrders()
}

function handleViewOrderDetailFromList(order) {
  orderStore.setCurrentOrder(order)
  router.push('/step/6')
}

function handleGoToRefundFromList(order) {
  orderStore.setCurrentOrder(order)
  router.push('/step/4')
}

function formatDate(dateStr) {
  if (!dateStr) return 'N/A'
  const date = new Date(dateStr)
  return date.toLocaleString('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }).replace(/\//g, '-')
}
```

- [ ] **Step 4: 在 onMounted 中加载订单列表**

修改 `onMounted` 函数：

```javascript
onMounted(async () => {
  // 原有的 Step 3 挂载逻辑
  if (props.stepNumber === '2' && qrCodeUrl.value) {
    await nextTick()
    renderQRCode()
    startPolling()
  }

  // 新增：Step 3 加载订单列表
  if (props.stepNumber === '3') {
    loadOrders()
  }
})
```

- [ ] **Step 5: 在 watch(stepNumber) 中添加 Step 3 逻辑**

修改现有的 watch：

```javascript
watch(() => props.stepNumber, async (newStep) => {
  // 原有逻辑
  if (newStep !== '2') {
    stopPolling()
  } else if (newStep === '2' && qrCodeUrl.value) {
    await renderQRCode()
    startPolling()
  }

  // 新增：Step 3 加载订单列表
  if (newStep === '3') {
    loadOrders()
  }
})
```

- [ ] **Step 6: 添加订单列表样式**

在 style 部分添加：

```css
.filter-bar {
  background-color: #ffffff;
  padding: 20px;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  margin-bottom: 20px;
  display: flex;
  gap: 16px;
  align-items: center;
}

.filter-group {
  display: flex;
  align-items: center;
  gap: 8px;
}

.filter-group label {
  font-weight: bold;
  color: #666666;
}

.filter-group select {
  padding: 8px 12px;
  border: 1px solid #e0e0e0;
  border-radius: 4px;
  font-size: 14px;
}

.refresh-btn {
  padding: 8px 16px;
  background-color: #0066cc;
  color: #ffffff;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.refresh-btn:hover:not(:disabled) {
  background-color: #0052a3;
}

.loading-message,
.empty-message {
  text-align: center;
  padding: 40px;
  color: #666666;
  background-color: #ffffff;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  margin-bottom: 20px;
}

.order-list {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.order-card {
  background-color: #ffffff;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  overflow: hidden;
  transition: box-shadow 0.2s;
}

.order-card:hover {
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.order-card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px;
  border-bottom: 1px solid #f0f0f0;
  background-color: #fafafa;
}

.order-no {
  font-weight: bold;
  color: #000000;
  font-size: 16px;
}

.order-card-body {
  padding: 16px;
}

.order-info {
  margin-bottom: 16px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #f0f0f0;
}

.info-row:last-child {
  border-bottom: none;
}

.info-row .label {
  font-weight: bold;
  color: #666666;
}

.info-row .value {
  color: #000000;
}

.info-row .value.amount {
  font-size: 16px;
  font-weight: bold;
  color: #0066cc;
}

.order-actions {
  display: flex;
  gap: 8px;
}

.action-btn {
  flex: 1;
  padding: 8px 16px;
  border: none;
  border-radius: 4px;
  font-size: 14px;
  font-weight: bold;
  cursor: pointer;
  transition: all 0.2s;
}

.action-btn.primary {
  background-color: #0066cc;
  color: #ffffff;
}

.action-btn.primary:hover {
  background-color: #0052a3;
}

.action-btn.warning {
  background-color: #ffc107;
  color: #000000;
}

.action-btn.warning:hover {
  background-color: #e0a800;
}
```

- [ ] **Step 7: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step3): 实现订单列表功能"
```

---

## Task 7: 实现 Step 4 - 退款申请

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 添加 Step 4 模板结构**

在 Step 3 后添加：

```vue
<!-- Step 4: 退款申请 -->
<div v-else-if="stepNumber === '4'">
  <h2>Apply for Refund</h2>

  <!-- 订单信息预览 -->
  <div v-if="orderStore.currentOrder" class="order-preview">
    <h3>Order Information</h3>
    <div class="preview-row">
      <span class="label">Order No:</span>
      <span class="value">{{ orderStore.currentOrder.order_no }}</span>
    </div>
    <div class="preview-row">
      <span class="label">Product:</span>
      <span class="value">{{ orderStore.currentOrder.productName || orderStore.currentOrder.title }}</span>
    </div>
    <div class="preview-row">
      <span class="label">Paid Amount:</span>
      <span class="value amount">¥{{ orderStore.currentOrder.amount }}</span>
    </div>
    <div class="preview-row" v-if="orderStore.currentOrder.tradeNo">
      <span class="label">Trade No:</span>
      <span class="value">{{ orderStore.currentOrder.tradeNo }}</span>
    </div>
  </div>

  <!-- 退款表单 -->
  <form @submit.prevent="handleSubmitRefund" class="refund-form">
    <div class="form-group">
      <label for="refundAmount">Refund Amount (¥) *</label>
      <input
        id="refundAmount"
        v-model="refundForm.amount"
        type="number"
        step="0.01"
        min="0.01"
        readonly
        required
      />
      <small class="form-hint">
        Full refund amount: ¥{{ orderStore.currentOrder?.amount }}
      </small>
    </div>

    <div class="form-group">
      <label for="refundReason">Refund Reason *</label>
      <select
        id="refundReason"
        v-model="refundForm.reason"
        required
      >
        <option value="">Please select a reason</option>
        <option value="不想要了">Don't want it anymore</option>
        <option value="商品质量问题">Product quality issue</option>
        <option value="发货太慢">Shipping too slow</option>
        <option value="买错了">Bought by mistake</option>
        <option value="其他">Other</option>
      </select>
    </div>

    <div v-if="error" class="error-message">{{ error }}</div>
    <div v-if="success" class="success-message">{{ success }}</div>

    <div class="button-group">
      <button type="submit" :disabled="loading || !refundForm.reason">
        {{ loading ? 'Processing...' : 'Submit Refund Request' }}
      </button>
      <button type="button" @click="handleCancelRefund" class="secondary">
        Cancel
      </button>
    </div>
  </form>

  <!-- 退款须知 -->
  <div class="refund-notice">
    <h4>Refund Policy:</h4>
    <ul>
      <li>Refund amount cannot exceed the paid amount</li>
      <li>Refund processing time: 1-3 business days</li>
      <li>Partial refunds are supported</li>
      <li>Once submitted, refund requests cannot be cancelled</li>
    </ul>
  </div>
</div>
```

- [ ] **Step 2: 添加退款表单响应式数据**

在 script setup 中添加：

```javascript
const refundForm = ref({
  amount: '',
  reason: ''
})
const refundSuccess = ref('')
```

- [ ] **Step 3: 添加退款处理函数**

```javascript
async function handleSubmitRefund() {
  loading.value = true
  error.value = ''
  refundSuccess.value = ''

  try {
    const response = await paymentApi.refund({
      order_no: orderStore.currentOrder.order_no,
      refund_amount: String(refundForm.value.amount),
      refund_reason: refundForm.value.reason
    })

    // 保存退款信息
    orderStore.setRefundInfo(response.data)

    refundSuccess.value = 'Refund request submitted successfully!'
    
    // 延迟跳转到 Step 5
    setTimeout(() => {
      router.push('/step/5')
    }, 1500)

  } catch (err) {
    error.value = err.message || 'Failed to submit refund request'
  } finally {
    loading.value = false
  }
}

function handleCancelRefund() {
  router.push('/step/3')
}
```

- [ ] **Step 4: 在 watch(stepNumber) 中添加 Step 4 逻辑**

```javascript
watch(() => props.stepNumber, async (newStep) => {
  // ... 现有逻辑

  // Step 4: 初始化退款表单
  if (newStep === '4' && orderStore.currentOrder) {
    refundForm.value = {
      amount: orderStore.currentOrder.amount,
      reason: ''
    }
  }
})
```

- [ ] **Step 5: 添加退款表单样式**

```css
.order-preview {
  background-color: #ffffff;
  padding: 20px;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  margin-bottom: 20px;
}

.order-preview h3 {
  margin-top: 0;
  margin-bottom: 16px;
  color: #000000;
}

.preview-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #f0f0f0;
}

.preview-row:last-child {
  border-bottom: none;
}

.preview-row .label {
  font-weight: bold;
  color: #666666;
}

.preview-row .value {
  color: #000000;
}

.preview-row .value.amount {
  font-size: 18px;
  font-weight: bold;
  color: #0066cc;
}

.refund-form {
  background-color: #ffffff;
  padding: 24px;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  margin-bottom: 20px;
}

.form-hint {
  display: block;
  margin-top: 4px;
  color: #666666;
  font-size: 12px;
}

.refund-notice {
  background-color: #fff3cd;
  padding: 16px;
  border: 1px solid #ffc107;
  border-radius: 8px;
}

.refund-notice h4 {
  margin-top: 0;
  margin-bottom: 12px;
  color: #856404;
}

.refund-notice ul {
  margin: 0;
  padding-left: 20px;
  color: #856404;
}

.refund-notice li {
  margin-bottom: 8px;
}
```

- [ ] **Step 6: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step4): 实现退款申请功能"
```

---

## Task 8: 实现 Step 5 - 退款状态查询

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 添加 Step 5 模板结构**

在 Step 4 后添加：

```vue
<!-- Step 5: 查询退款状态 -->
<div v-else-if="stepNumber === '5'">
  <h2>Refund Status</h2>

  <!-- 退款信息卡片 -->
  <div v-if="orderStore.refundInfo" class="refund-info-card">
    <div class="info-section">
      <h3>Refund Information</h3>
      <div class="info-grid">
        <div class="info-item">
          <span class="label">Refund No:</span>
          <span class="value">{{ orderStore.refundInfo.refund_no }}</span>
        </div>
        <div class="info-item">
          <span class="label">Order No:</span>
          <span class="value">{{ orderStore.refundInfo.order_no }}</span>
        </div>
        <div class="info-item">
          <span class="label">Refund Amount:</span>
          <span class="value amount">¥{{ orderStore.refundInfo.refund_amount }}</span>
        </div>
        <div class="info-item">
          <span class="label">Status:</span>
          <span :class="['status-badge', getRefundStatusClass(orderStore.refundInfo.status)]">
            {{ formatRefundStatus(orderStore.refundInfo.status) }}
          </span>
        </div>
        <div class="info-item" v-if="orderStore.refundInfo.refund_reason">
          <span class="label">Reason:</span>
          <span class="value">{{ orderStore.refundInfo.refund_reason }}</span>
        </div>
        <div class="info-item" v-if="orderStore.refundInfo.channel_refund_no">
          <span class="label">Channel Refund No:</span>
          <span class="value">{{ orderStore.refundInfo.channel_refund_no }}</span>
        </div>
        <div class="info-item" v-if="orderStore.refundInfo.created_at">
          <span class="label">Applied At:</span>
          <span class="value">{{ formatDate(orderStore.refundInfo.created_at) }}</span>
        </div>
        <div class="info-item" v-if="orderStore.refundInfo.refunded_at">
          <span class="label">Refunded At:</span>
          <span class="value">{{ formatDate(orderStore.refundInfo.refunded_at) }}</span>
        </div>
      </div>
    </div>

    <!-- 渠道响应信息 -->
    <div v-if="orderStore.refundInfo.channel_response" class="info-section">
      <h3>Channel Response</h3>
      <pre class="json-display">{{ JSON.stringify(orderStore.refundInfo.channel_response, null, 2) }}</pre>
    </div>
  </div>

  <!-- 查询表单（直接进入时显示） -->
  <div v-else class="refund-query-form">
    <div class="form-group">
      <label for="refundOrderNo">Order No / Refund No *</label>
      <input
        id="refundOrderNo"
        v-model="queryForm.orderNo"
        type="text"
        required
        placeholder="Enter order no or refund no"
      />
    </div>

    <div v-if="error" class="error-message">{{ error }}</div>

    <div class="button-group">
      <button @click="handleQueryRefund" :disabled="loading || !queryForm.orderNo">
        {{ loading ? 'Querying...' : 'Query Refund Status' }}
      </button>
      <button @click="handleBackToOrders" class="secondary">
        Back
      </button>
    </div>
  </div>

  <!-- 操作按钮 -->
  <div v-if="orderStore.refundInfo" class="button-group">
    <button @click="handleRefreshRefund" :disabled="loading">
      {{ loading ? 'Refreshing...' : 'Refresh Status' }}
    </button>
    <button @click="handleBackToOrders" class="secondary">
      Back to Orders
    </button>
  </div>
</div>
```

- [ ] **Step 2: 添加退款查询响应式数据**

```javascript
const queryForm = ref({
  orderNo: ''
})
```

- [ ] **Step 3: 添加退款查询处理函数**

```javascript
async function handleQueryRefund() {
  loading.value = true
  error.value = ''

  try {
    const response = await paymentApi.queryRefund(queryForm.value.orderNo)
    orderStore.setRefundInfo(response.data)
  } catch (err) {
    error.value = err.message || 'Failed to query refund status'
  } finally {
    loading.value = false
  }
}

async function handleRefreshRefund() {
  if (orderStore.refundInfo?.order_no) {
    await handleQueryRefund()
  }
}

function handleBackToOrders() {
  router.push('/step/3')
}

function formatRefundStatus(status) {
  const statusMap = {
    'PENDING': 'Processing',
    'SUCCESS': 'Refunded',
    'FAILED': 'Failed',
    'UNKNOWN': 'Unknown'
  }
  return statusMap[status] || status
}

function getRefundStatusClass(status) {
  switch (status) {
    case 'SUCCESS': return 'status-refunded'
    case 'FAILED': return 'status-failed'
    case 'PENDING': return 'status-paying'
    default: return 'status-pending'
  }
}
```

- [ ] **Step 4: 在 watch(stepNumber) 中添加 Step 5 逻辑**

```javascript
watch(() => props.stepNumber, async (newStep) => {
  // ... 现有逻辑

  // Step 5: 如果有退款信息，自动查询最新状态
  if (newStep === '5' && orderStore.refundInfo?.order_no) {
    handleQueryRefund()
  }
})
```

- [ ] **Step 5: 添加退款信息卡片样式**

```css
.refund-info-card {
  background-color: #ffffff;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 20px;
}

.info-section {
  padding: 20px;
  border-bottom: 1px solid #f0f0f0;
}

.info-section:last-child {
  border-bottom: none;
}

.info-section h3 {
  margin-top: 0;
  margin-bottom: 16px;
  color: #000000;
}

.info-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

@media (max-width: 600px) {
  .info-grid {
    grid-template-columns: 1fr;
  }
}

.info-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.info-item .label {
  font-weight: bold;
  color: #666666;
  font-size: 12px;
}

.info-item .value {
  color: #000000;
  font-size: 14px;
}

.info-item .value.amount {
  font-size: 18px;
  font-weight: bold;
  color: #0066cc;
}

.json-display {
  background-color: #f5f5f5;
  padding: 12px;
  border-radius: 4px;
  overflow-x: auto;
  font-size: 12px;
  color: #333333;
}

.refund-query-form {
  background-color: #ffffff;
  padding: 24px;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  margin-bottom: 20px;
}
```

- [ ] **Step 6: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step5): 实现退款状态查询功能"
```

---

## Task 9: 实现 Step 6 - 订单详情

**Files:**
- Modify: `PayFrontend/src/views/StepView.vue`

- [ ] **Step 1: 添加 Step 6 模板结构**

在 Step 5 后添加：

```vue
<!-- Step 6: 订单详情 -->
<div v-else-if="stepNumber === '6'">
  <h2>Order Details</h2>

  <div v-if="orderStore.currentOrder" class="order-detail-card">
    <!-- 订单基本信息 -->
    <div class="detail-section">
      <h3>Order Information</h3>
      <div class="detail-grid">
        <div class="detail-item">
          <span class="label">Order No:</span>
          <span class="value">{{ orderStore.currentOrder.order_no }}</span>
        </div>
        <div class="detail-item">
          <span class="label">Status:</span>
          <span :class="['status-badge', getStatusClass(orderStore.currentOrder.status)]">
            {{ formatStatus(orderStore.currentOrder.status) }}
          </span>
        </div>
        <div class="detail-item">
          <span class="label">Amount:</span>
          <span class="value amount">¥{{ orderStore.currentOrder.amount }}</span>
        </div>
        <div class="detail-item">
          <span class="label">User ID:</span>
          <span class="value">{{ orderStore.currentOrder.user_id || 1 }}</span>
        </div>
        <div class="detail-item full-width">
          <span class="label">Product:</span>
          <span class="value">{{ orderStore.currentOrder.productName || orderStore.currentOrder.title }}</span>
        </div>
        <div class="detail-item full-width" v-if="orderStore.currentOrder.description">
          <span class="label">Description:</span>
          <span class="value">{{ orderStore.currentOrder.description }}</span>
        </div>
        <div class="detail-item" v-if="orderStore.currentOrder.createdAt">
          <span class="label">Created At:</span>
          <span class="value">{{ formatDate(orderStore.currentOrder.createdAt) }}</span>
        </div>
      </div>
    </div>

    <!-- 支付信息（如果已支付） -->
    <div v-if="orderStore.currentOrder.tradeNo" class="detail-section">
      <h3>Payment Information</h3>
      <div class="detail-grid">
        <div class="detail-item">
          <span class="label">Trade No:</span>
          <span class="value">{{ orderStore.currentOrder.tradeNo }}</span>
        </div>
        <div class="detail-item" v-if="orderStore.currentOrder.paymentNo">
          <span class="label">Payment No:</span>
          <span class="value">{{ orderStore.currentOrder.paymentNo }}</span>
        </div>
      </div>
    </div>

    <!-- 支付渠道响应 -->
    <div v-if="orderStore.currentOrder.alipay_response" class="detail-section">
      <h3>Payment Channel Response</h3>
      <button @click="toggleChannelResponse" class="text-btn">
        {{ showChannelResponse ? 'Hide' : 'Show' }} Details
      </button>
      <pre v-if="showChannelResponse" class="json-display">
{{ JSON.stringify(orderStore.currentOrder.alipay_response, null, 2) }}
      </pre>
    </div>
  </div>

  <!-- 操作按钮 -->
  <div class="button-group">
    <button @click="handleBackToOrders" class="secondary">
      Back to Orders
    </button>
    <button
      v-if="orderStore.currentOrder?.status === 'PAID'"
      @click="handleGoToRefundFromDetail"
      class="warning"
    >
      Apply for Refund
    </button>
  </div>
</div>
```

- [ ] **Step 2: 添加订单详情响应式数据**

```javascript
const showChannelResponse = ref(false)
```

- [ ] **Step 3: 添加订单详情处理函数**

```javascript
function toggleChannelResponse() {
  showChannelResponse.value = !showChannelResponse.value
}

function handleBackToOrders() {
  router.push('/step/3')
}

function handleGoToRefundFromDetail() {
  router.push('/step/4')
}
```

- [ ] **Step 4: 添加订单详情样式**

```css
.order-detail-card {
  background-color: #ffffff;
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 20px;
}

.detail-section {
  padding: 20px;
  border-bottom: 1px solid #f0f0f0;
}

.detail-section:last-child {
  border-bottom: none;
}

.detail-section h3 {
  margin-top: 0;
  margin-bottom: 16px;
  color: #000000;
}

.detail-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

@media (max-width: 600px) {
  .detail-grid {
    grid-template-columns: 1fr;
  }
}

.detail-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.detail-item.full-width {
  grid-column: 1 / -1;
}

.detail-item .label {
  font-weight: bold;
  color: #666666;
  font-size: 12px;
}

.detail-item .value {
  color: #000000;
  font-size: 14px;
}

.detail-item .value.amount {
  font-size: 18px;
  font-weight: bold;
  color: #0066cc;
}

.text-btn {
  background: none;
  border: none;
  color: #0066cc;
  cursor: pointer;
  padding: 0;
  font-size: 14px;
}

.text-btn:hover {
  text-decoration: underline;
}
```

- [ ] **Step 5: 提交变更**

```bash
git add src/views/StepView.vue
git commit -m "feat(step6): 实现订单详情展示功能"
```

---

## Task 10: 添加 StepIndicator 更新

**Files:**
- Modify: `PayFrontend/src/components/StepIndicator.vue`

- [ ] **Step 1: 更新步骤总数**

找到步骤总数定义，修改为 6：

```javascript
const totalSteps = 6
```

- [ ] **Step 2: 提交变更**

```bash
git add src/components/StepIndicator.vue
git commit -m "feat: 更新步骤指示器为6步"
```

---

## Task 11: 端到端测试

**Files:**
- No files modified (testing only)

- [ ] **Step 1: 启动前端开发服务器**

```bash
cd PayFrontend
npm run dev
```

- [ ] **Step 2: 测试创建订单流程**

1. 访问 `http://localhost:5173/step/1`
2. 填写表单：产品名称="Test Product", 金额="17.00", 用户ID="1"
3. 点击"Create Order & Generate QR Code"
4. 验证：自动跳转到 Step 2，二维码显示正确

- [ ] **Step 3: 测试支付流程**

1. 在 Step 2 等待支付成功或手动检查状态
2. 验证：轮询正常工作
3. 支付成功后验证：
   - 显示成功提示框
   - 显示订单号、交易号、金额、时间
   - 显示"View Order Details"和"Apply for Refund"按钮

- [ ] **Step 4: 测试订单列表**

1. 点击"Back to Orders"或直接访问 `/step/3`
2. 验证：订单列表显示正确
3. 测试状态筛选：选择"Paid"，验证筛选正常
4. 点击"View Details"，验证跳转到 Step 6

- [ ] **Step 5: 测试退款申请**

1. 在订单列表或订单详情页，点击"Apply for Refund"
2. 验证：跳转到 Step 4，订单信息预填充
3. 选择退款原因，点击"Submit Refund Request"
4. 验证：提交成功，自动跳转到 Step 5

- [ ] **Step 6: 测试退款查询**

1. 在 Step 5 验证退款信息显示正确
2. 点击"Refresh Status"，验证状态更新
3. 点击"Back to Orders"，验证返回 Step 3

- [ ] **Step 7: 测试订单详情**

1. 从订单列表点击"View Details"
2. 验证：Step 6 显示完整订单信息
3. 点击"Show Details"，验证渠道响应展开
4. 点击"Apply for Refund"，验证跳转到 Step 4

- [ ] **Step 8: 测试导航流程**

1. Step 6 → Step 3：点击"Back to Orders"
2. Step 3 → Step 4：选择已支付订单，点击"Refund"
3. Step 4 → Step 5：提交退款申请
4. Step 5 → Step 3：点击"Back to Orders"

- [ ] **Step 9: 测试错误处理**

1. 创建订单时断网，验证错误提示
2. 查询不存在的订单，验证错误提示
3. 重复申请退款，验证错误提示

---

## Task 12: 后端接口实现

**Files:**
- Create: `PayBackend/controllers/PayController.cc` (add new endpoint)
- Modify: `PayBackend/services/PaymentService.cc` (add listOrders method)

- [ ] **Step 1: 在 PayController.h 中添加订单列表接口声明**

```cpp
void orderList(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
```

- [ ] **Step 2: 在 PayController.cc 中实现订单列表接口**

```cpp
void PayController::orderList(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // 获取查询参数
    std::string userId = req->getParameter("user_id");
    std::string status = req->getParameter("status");

    if (userId.empty())
    {
        Json::Value error;
        error["code"] = 400;
        error["message"] = "Missing required parameter: user_id";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    LOG_DEBUG << "[PAY_CONTROLLER] orderList called with user_id=" << userId
              << " status=" << status;

    // 获取 service 并调用
    auto plugin = drogon::app().getPlugin<PayPlugin>();
    auto paymentService = plugin->paymentService();

    int userIdInt = std::stoi(userId);
    paymentService->listOrders(
        userIdInt,
        status,
        [callback](const Json::Value& result, const std::error_code& error) {
            auto resp = HttpResponse::newHttpJsonResponse(result);
            if (error)
            {
                resp->setStatusCode(k500InternalServerError);
            }
            callback(resp);
        }
    );
}
```

- [ ] **Step 3: 在路由中注册订单列表接口**

在 `METHOD_LIST_BEGIN` 中添加：

```cpp
ADD_METHOD_TO(PayController::orderList,
              "/api/pay/orders",
              Get,
              Options,
              "PayAuthFilter");
```

- [ ] **Step 4: 在 PaymentService.h 中添加 listOrders 方法声明**

```cpp
void listOrders(int userId,
                const std::string& status,
                std::function<void(const Json::Value&, const std::error_code&)>&& callback);
```

- [ ] **Step 5: 在 PaymentService.cc 中实现 listOrders 方法**

```cpp
void PaymentService::listOrders(
    int userId,
    const std::string& status,
    std::function<void(const Json::Value&, const std::error_code&)>&& callback)
{
    LOG_DEBUG << "[PAYMENT_SERVICE] listOrders: user_id=" << userId
              << " status=" << status;

    auto client = dbClient_;

    // 构建查询条件
    std::string whereClause = "WHERE user_id = $1";
    std::vector<std::string> params;
    params.push_back(std::to_string(userId));

    if (!status.empty() && status != "all") {
        whereClause += " AND status = $2";
        params.push_back(status);
    }

    std::string query = "SELECT * FROM pay_orders " + whereClause + " ORDER BY created_at DESC";

    client->execSqlAsync(
        query,
        [callback](const Result& result) {
            Json::Value response;
            response["code"] = 0;
            response["message"] = "Orders retrieved successfully";
            Json::Value orders(Json::arrayValue);

            for (size_t i = 0; i < result.size(); ++i) {
                Json::Value order;
                order["order_no"] = result[i]["order_no"].as<std::string>();
                order["title"] = result[i]["title"].as<std::string>();
                order["amount"] = result[i]["amount"].as<std::string>();
                order["currency"] = result[i]["currency"].as<std::string>();
                order["status"] = result[i]["status"].as<std::string>();
                order["user_id"] = result[i]["user_id"].as<int>();
                
                // 可选字段
                if (!result[i]["trade_no"].isNull()) {
                    order["trade_no"] = result[i]["trade_no"].as<std::string>();
                }
                if (!result[i]["payment_no"].isNull()) {
                    order["payment_no"] = result[i]["payment_no"].as<std::string>();
                }
                if (!result[i]["refund_no"].isNull()) {
                    order["refund_no"] = result[i]["refund_no"].as<std::string>();
                }

                order["created_at"] = result[i]["created_at"].as<std::string>();
                order["updated_at"] = result[i]["updated_at"].as<std::string>();

                orders.append(order);
            }

            response["data"]["orders"] = orders;
            callback(response, std::error_code());
        },
        [callback](const DrogonDbException& e) {
            Json::Value response;
            response["code"] = 1500;
            response["message"] = "Database error: " + std::string(e.base().what());
            callback(response, std::error_code());
        },
        params
    );
}
```

- [ ] **Step 6: 重新编译后端**

```bash
cd PayBackend
../scripts/build.bat
```

- [ ] **Step 7: 重启后端服务器**

```bash
taskkill /F /IM PayServer.exe
cd build/Release
./PayServer.exe
```

- [ ] **Step 8: 测试订单列表接口**

使用浏览器或 curl 测试：

```bash
curl "http://localhost:8848/api/pay/orders?user_id=1&status=all"
```

- [ ] **Step 9: 提交后端变更**

```bash
cd PayBackend
git add controllers/PayController.h controllers/PayController.cc services/PaymentService.h services/PaymentService.cc
git commit -m "feat: 实现订单列表查询接口"
```

---

## 验收标准

完成所有任务后，系统应该满足以下标准：

### 功能完整性
- [ ] Step 1-6 所有步骤功能正常
- [ ] 订单列表支持状态筛选
- [ ] 退款申请流程完整
- [ ] 退款状态查询正常
- [ ] 订单详情展示完整

### 导航流程
- [ ] Step 6 → Step 3 返回订单列表
- [ ] Step 3 → Step 4 跳转退款
- [ ] Step 4 → Step 5 自动查询退款
- [ ] 所有"返回"按钮正常工作

### 数据管理
- [ ] Store 正确管理订单列表
- [ ] Store 正确管理退款信息
- [ ] Store 正确管理当前查看订单
- [ ] API 层正确封装所有接口

### 用户体验
- [ ] 加载状态正确显示
- [ ] 错误提示友好清晰
- [ ] 成功提示明确
- [ ] 空状态有引导

### 后端集成
- [ ] 订单列表接口正常工作
- [ ] 所有接口响应格式统一
- [ ] 错误处理完善

---

## 总计

- **总任务数：** 12
- **预计时间：** 4-6 小时
- **优先级：** 高

**执行顺序：** 按任务编号顺序执行，确保每个任务完成后再开始下一个。
