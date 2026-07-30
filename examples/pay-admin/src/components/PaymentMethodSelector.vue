<template>
  <div class="payment-method-selector">
    <h3>Select Payment Method</h3>
    <div class="options">
      <label
        v-for="method in methods"
        :key="method.value"
        :class="['option', { selected: modelValue === method.value }]"
      >
        <input
          type="radio"
          :value="method.value"
          :checked="modelValue === method.value"
          @change="$emit('update:modelValue', method.value)"
        />
        <span class="label">{{ method.label }}</span>
        <span class="description">{{ method.description }}</span>
      </label>
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  modelValue: String
})

defineEmits(['update:modelValue'])

const methods = [
  {
    value: 'qrcode',
    label: 'QR Code Payment',
    description: 'Scan with Alipay sandbox app'
  },
  {
    value: 'pc',
    label: 'PC Web Payment',
    description: 'Pay in browser on desktop'
  },
  {
    value: 'mobile',
    label: 'Mobile Web Payment',
    description: 'Mobile-optimized payment page'
  }
]
</script>

<style scoped>
.payment-method-selector {
  margin-bottom: 30px;
}

.payment-method-selector h3 {
  margin-bottom: 16px;
  font-size: 18px;
}

.options {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.option {
  display: flex;
  align-items: center;
  padding: 16px;
  border: 2px solid #e0e0e0;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s;
}

.option:hover {
  border-color: #0066cc;
  background-color: #f5f5f5;
}

.option.selected {
  border-color: #0066cc;
  background-color: #e6f2ff;
}

.option input[type="radio"] {
  margin-right: 12px;
  width: 18px;
  height: 18px;
}

.option .label {
  font-weight: bold;
  margin-right: 12px;
  flex: 0 0 auto;
}

.option .description {
  color: #666666;
  font-size: 14px;
}
</style>