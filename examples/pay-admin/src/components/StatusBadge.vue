<template>
  <span :class="['status-badge', statusClass]">
    {{ displayText }}
  </span>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  status: {
    type: String,
    required: true
  }
})

const displayText = computed(() => {
  return props.status.charAt(0).toUpperCase() + props.status.slice(1)
})

const statusClass = computed(() => {
  const status = props.status.toLowerCase()
  if (status === 'success' || status === 'paid') {
    return 'success'
  } else if (status === 'pending' || status === 'processing') {
    return 'pending'
  } else if (status === 'failed' || status === 'error') {
    return 'failed'
  }
  return 'info'
})
</script>

<style scoped>
.status-badge {
  padding: 4px 12px;
  border-radius: 4px;
  font-size: 14px;
  font-weight: bold;
}

.status-badge.success {
  background-color: #d4edda;
  color: #155724;
}

.status-badge.pending {
  background-color: #fff3cd;
  color: #856404;
}

.status-badge.failed {
  background-color: #f8d7da;
  color: #721c24;
}

.status-badge.info {
  background-color: #d1ecf1;
  color: #0c5460;
}
</style>