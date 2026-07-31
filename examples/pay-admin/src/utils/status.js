// 状态单一映射表 —— 与后端词表严格一致，前端不得出现后端不存在的状态
// 订单：PAYING → PAID / FAILED
export const ORDER_STATUS = {
  PAYING: { label: '支付中', tagType: 'warning' },
  PAID: { label: '已支付', tagType: 'success' },
  FAILED: { label: '支付失败', tagType: 'danger' }
}

// 退款：REFUND_INIT → REFUNDING → REFUND_SUCCESS / REFUND_FAIL
export const REFUND_STATUS = {
  REFUND_INIT: { label: '退款受理', tagType: 'info' },
  REFUNDING: { label: '退款中', tagType: 'warning' },
  REFUND_SUCCESS: { label: '退款成功', tagType: 'success' },
  REFUND_FAIL: { label: '退款失败', tagType: 'danger' }
}

export function orderStatusInfo(status) {
  return ORDER_STATUS[status] || { label: status || '-', tagType: 'info' }
}

export function refundStatusInfo(status) {
  return REFUND_STATUS[status] || { label: status || '-', tagType: 'info' }
}

// 订单列表筛选选项（仅后端真实状态）
export const ORDER_STATUS_OPTIONS = Object.entries(ORDER_STATUS).map(([value, v]) => ({
  value,
  label: v.label
}))
