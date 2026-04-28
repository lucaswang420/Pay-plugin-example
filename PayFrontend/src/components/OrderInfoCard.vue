<template>
  <div class="order-info-card">
    <h3>Order Information</h3>
    <div class="info-row">
      <span class="label">Order ID:</span>
      <span class="value">{{ orderId }}</span>
    </div>
    <div class="info-row">
      <span class="label">Product:</span>
      <span class="value">{{ productName }}</span>
    </div>
    <div class="info-row">
      <span class="label">Amount:</span>
      <span class="value">¥{{ amount.toFixed(2) }}</span>
    </div>
    <div v-if="description" class="info-row">
      <span class="label">Description:</span>
      <span class="value">{{ description }}</span>
    </div>
    <div v-if="status" class="info-row">
      <span class="label">Status:</span>
      <StatusBadge :status="status" />
    </div>
  </div>
</template>

<script setup>
import StatusBadge from './StatusBadge.vue'

const props = defineProps({
  orderId: String,
  productName: String,
  amount: Number,
  description: String,
  status: String
})
</script>

<style scoped>
.order-info-card {
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  padding: 20px;
  margin-bottom: 20px;
  background-color: #f9f9f9;
}

.order-info-card h3 {
  margin-bottom: 16px;
  font-size: 18px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid #e0e0e0;
}

.info-row:last-child {
  border-bottom: none;
}

.label {
  font-weight: bold;
  color: #666666;
}

.value {
  color: #000000;
}
</style>