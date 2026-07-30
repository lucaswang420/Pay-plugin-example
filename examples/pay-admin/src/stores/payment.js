import { defineStore } from 'pinia'
import { ref } from 'vue'

export const usePaymentStore = defineStore('payment', () => {
  const paymentMethod = ref('')
  const paymentStatus = ref('')
  const qrCodeUrl = ref('')
  const paymentUrl = ref('')

  const refundAmount = ref(0)
  const refundReason = ref('')
  const refundStatus = ref('')

  function setPaymentMethod(method) {
    paymentMethod.value = method
  }

  function setPaymentInfo(info) {
    paymentStatus.value = info.status || ''
    qrCodeUrl.value = info.qrCodeUrl || ''
    paymentUrl.value = info.paymentUrl || ''
  }

  function setRefundInfo(info) {
    refundAmount.value = info.amount || 0
    refundReason.value = info.reason || ''
    refundStatus.value = info.status || ''
  }

  function clear() {
    paymentMethod.value = ''
    paymentStatus.value = ''
    qrCodeUrl.value = ''
    paymentUrl.value = ''
    refundAmount.value = 0
    refundReason.value = ''
    refundStatus.value = ''
  }

  return {
    paymentMethod,
    paymentStatus,
    qrCodeUrl,
    paymentUrl,
    refundAmount,
    refundReason,
    refundStatus,
    setPaymentMethod,
    setPaymentInfo,
    setRefundInfo,
    clear
  }
})