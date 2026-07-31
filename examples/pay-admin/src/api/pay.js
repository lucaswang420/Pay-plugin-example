import http from './http'

// 后端路由前缀：PayPlugin base_path 默认 /api/pay，经 vite 代理 baseURL=/api
const BASE = '/pay'

export const payApi = {
  // POST /api/pay/create - 创建支付（响应已含二维码字段，无需再调 /qrpay/create）
  create(data, { idempotencyKey, signal } = {}) {
    const headers = {}
    if (idempotencyKey) {
      headers['X-Idempotency-Key'] = idempotencyKey
    }
    return http.post(`${BASE}/create`, data, { headers, signal })
  },

  // GET /api/pay/query?order_no= - 查询支付状态（PAYING → PAID/FAILED）
  query(orderNo, { signal } = {}) {
    return http.get(`${BASE}/query`, { params: { order_no: orderNo }, signal })
  },

  // GET /api/pay/orders - 订单列表（limit ≤ 100，无 total 字段）
  orders({ status, userId, limit, offset } = {}, { signal } = {}) {
    const params = {}
    if (status) params.status = status
    if (userId) params.user_id = userId
    if (limit !== undefined) params.limit = limit
    if (offset !== undefined) params.offset = offset
    return http.get(`${BASE}/orders`, { params, signal })
  },

  // POST /api/pay/refund - 发起退款
  refund(data, { signal } = {}) {
    return http.post(`${BASE}/refund`, data, { signal })
  },

  // GET /api/pay/refund/query?refund_no= - 退款进度查询
  refundQuery(refundNo, { signal } = {}) {
    return http.get(`${BASE}/refund/query`, { params: { refund_no: refundNo }, signal })
  },

  // GET /api/pay/reconcile/summary?date=YYYY-MM-DD - 对账摘要
  reconcileSummary(date, { signal } = {}) {
    const params = {}
    if (date) params.date = date
    return http.get(`${BASE}/reconcile/summary`, { params, signal })
  },

  // GET /api/pay/metrics/auth - 认证指标（平铺 JSON）
  authMetrics({ signal } = {}) {
    return http.get(`${BASE}/metrics/auth`, { signal })
  }
}
