# 前端修改指南 - 支付宝E2E测试

## 📋 需要修改的文件清单

### 1. API密钥更新 ✅ (已完成)
**文件:** `src/api/index.js`

**修改内容:**
```javascript
// 第15行，将默认API密钥改为:
const apiKey = sessionStorage.getItem('api_key') || 'test_key_123456'
```

---

### 2. 响应数据结构适配 ⚠️ (需要手动修改)
**文件:** `src/views/StepView.vue`

**当前代码 (第100-106行):**
```javascript
orderStore.setOrder({
  orderId: response.out_trade_no,
  productName: form.value.productName,
  amount: form.value.amount,
  description: form.value.description,
  status: response.status || 'pending'
})
```

**修改为:**
```javascript
// Extract order info from nested response structure
const orderData = response.data || response
const alipayData = orderData.alipay_response || {}

orderStore.setOrder({
  orderId: orderData.order_no || alipayData.out_trade_no,
  productName: form.value.productName,
  amount: form.value.amount,
  description: form.value.description,
  status: orderData.status || 'pending',
  tradeNo: alipayData.trade_no || '',
  paymentNo: orderData.payment_no || ''
})
```

**原因:** 后端返回嵌套结构:
```json
{
  "code": 0,
  "data": {
    "alipay_response": { "code": "10000", "trade_no": "..." },
    "order_no": "...",
    "status": "PAYING"
  }
}
```

---

### 3. 添加订单状态轮询 ⚠️ (可选，建议添加)
**文件:** `src/views/StepView.vue`

**在 `router.push('/step/2')` 后添加轮询逻辑:**

```javascript
router.push('/step/2')

// Start polling order status every 3 seconds
const pollInterval = setInterval(async () => {
  try {
    const statusResponse = await paymentApi.queryOrder(orderId)

    // Check if order status changed from PAYING
    if (statusResponse.data.status !== 'PAYING') {
      clearInterval(pollInterval)

      // Update order store with final status
      orderStore.updateOrder({
        status: statusResponse.data.status
      })

      console.log('Order payment completed:', statusResponse.data)
    }
  } catch (err) {
    console.error('Polling error:', err)
    // Don't clear interval on error, keep trying
  }
}, 3000) // Poll every 3 seconds

// Clean up interval on component unmount
onUnmounted(() => {
  clearInterval(pollInterval)
})
```

**需要添加的import:**
```javascript
import { onUnmounted } from 'vue'
```

---

## 🧪 测试步骤

### 1. 启动前端
```bash
cd payFrontend
npm install
npm run dev
```

### 2. 访问前端
```
http://localhost:5173
```

### 3. 测试订单创建
- 产品名称: `测试产品`
- 金额: `100` (1元)
- 描述: `支付宝沙箱测试`

### 4. 检查控制台
- 打开浏览器开发者工具
- 查看Network标签，确认请求包含:
  - `channel: "alipay"` ✅
  - `X-Api-Key: test_key_123456` ✅
- 查看Console标签，确认响应数据正确解析

---

## 🔧 后端对应检查

确保后端正在运行:
```bash
cd PayBackend
./build/Release/PayServer.exe
```

后端应该监听: `http://localhost:5566`

---

## ✅ 验证清单

- [ ] API密钥已更新为 `test_key_123456`
- [ ] 响应数据结构已适配
- [ ] 前端能正确创建支付宝订单
- [ ] 控制台无CORS错误
- [ ] 控制台无401认证错误
- [ ] 订单状态正确存储到store
- [ ] 页面跳转到step/2成功

---

## 📝 注意事项

1. **CORS配置**: 后端的 `config.json` 已配置允许 `localhost:5173`
2. **API密钥**: 确保前后端使用相同的密钥
3. **数据类型**: 金额字段后端期望字符串，前端已正确转换
4. **轮询间隔**: 建议3-5秒，避免过于频繁

---

## 🐛 常见问题排查

### 问题1: 401 Unauthorized
**原因**: API密钥不匹配
**解决**: 确认前端使用 `test_key_123456`

### 问题2: CORS错误
**原因**: 前端URL未在后端允许列表中
**解决**: 检查 `config.json` 中的 `cors.allow_origins`

### 问题3: 订单创建成功但页面无响应
**原因**: 响应数据解析错误
**解决**: 检查浏览器控制台的Network标签，查看实际响应格式

### 问题4: 订单状态一直显示PAYING
**原因**: 未实现轮询或异步通知
**解决**: 添加轮询代码或等待对账服务(5分钟间隔)
