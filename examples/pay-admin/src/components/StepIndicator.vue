<template>
  <div class="step-indicator">
    <div
      v-for="(stepInfo, index) in steps"
      :key="index"
      :class="['step', { active: (index + 1) === currentStep, completed: (index + 1) < currentStep }]"
    >
      <div class="step-circle">
        <span v-if="(index + 1) < currentStep">✓</span>
        <span v-else>{{ index + 1 }}</span>
      </div>
      <div class="step-label">{{ stepInfo.label }}</div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'

const props = defineProps({
  currentStep: {
    type: Number,
    required: true
  }
})

const steps = ref([
  { label: 'Create Order' },
  { label: 'Scan & Pay' },
  { label: 'Order List' },
  { label: 'Refund' },
  { label: 'Refund Status' },
  { label: 'Order Details' }
])
</script>

<style scoped>
.step-indicator {
  display: flex;
  justify-content: space-between;
  margin-bottom: 40px;
  padding: 0 20px;
}

.step {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1;
  position: relative;
}

.step-circle {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  border: 2px solid #e0e0e0;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: bold;
  background-color: #ffffff;
  margin-bottom: 8px;
}

.step.active .step-circle {
  border-color: #0066cc;
  color: #0066cc;
}

.step.completed .step-circle {
  background-color: #28a745;
  border-color: #28a745;
  color: #ffffff;
}

.step-label {
  font-size: 12px;
  color: #666666;
}

.step.active .step-label {
  color: #0066cc;
  font-weight: bold;
}
</style>