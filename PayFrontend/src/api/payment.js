import apiClient from './index'

export const paymentApi = {
  // Create payment order
  createOrder(data) {
    console.log('[DEBUG] Sending createOrder request:', data)
    return apiClient.post('/pay/create', data)
  },

  // Create QR payment (returns qr_code)
  createQRPayment(data) {
    return apiClient.post('/qrpay/create', data)
  },

  // Query order status
  queryOrder(orderNo) {
    return apiClient.get('/pay/query', {
      params: { order_no: orderNo }
    })
  },

  // Request refund
  refund(data) {
    return apiClient.post('/pay/refund', data)
  },

  // Query refund status
  queryRefund(outTradeNo) {
    return apiClient.get('/pay/refund/query', {
      params: { out_trade_no: outTradeNo }
    })
  },

  // Query order list
  queryOrderList(params) {
    console.log('[API] Querying order list:', params)
    return apiClient.get('/pay/orders', { params })
  }
}