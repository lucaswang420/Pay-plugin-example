<template>
  <div>
    <el-card shadow="never" class="section-card">
      <template #header>
        <div class="section-header">
          <span>对账摘要</span>
          <div class="section-actions">
            <el-date-picker
              v-model="date"
              type="date"
              value-format="YYYY-MM-DD"
              :clearable="false"
              style="width: 160px"
              @change="loadAll"
            />
            <el-button :icon="Refresh" :loading="loading" @click="loadAll">刷新</el-button>
          </div>
        </div>
      </template>

      <el-row :gutter="16">
        <el-col :span="6">
          <el-card shadow="hover" class="stat-card">
            <el-statistic title="支付中订单数" :value="summary.paying_orders ?? 0" />
          </el-card>
        </el-col>
        <el-col :span="6">
          <el-card shadow="hover" class="stat-card">
            <el-statistic title="退款中笔数" :value="summary.refunding_refunds ?? 0" />
          </el-card>
        </el-col>
        <el-col :span="6">
          <el-card shadow="hover" class="stat-card">
            <div class="time-stat">
              <div class="time-stat-title">最早滞留支付单更新于</div>
              <div class="time-stat-value">{{ formatTime(summary.oldest_paying_updated) }}</div>
            </div>
          </el-card>
        </el-col>
        <el-col :span="6">
          <el-card shadow="hover" class="stat-card">
            <div class="time-stat">
              <div class="time-stat-title">最早滞留退款单更新于</div>
              <div class="time-stat-value">{{ formatTime(summary.oldest_refund_updated) }}</div>
            </div>
          </el-card>
        </el-col>
      </el-row>
    </el-card>

    <el-card shadow="never" class="section-card">
      <template #header>
        <span>认证指标</span>
      </template>
      <el-row :gutter="16">
        <el-col v-for="item in metricCards" :key="item.key" :span="6">
          <el-card shadow="hover" class="stat-card">
            <el-statistic :title="item.label" :value="metrics[item.key] ?? 0" />
          </el-card>
        </el-col>
      </el-row>
    </el-card>
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { Refresh } from '@element-plus/icons-vue'
import { payApi } from '../../api/pay'
import { usePolling } from '../../composables/usePolling'
import { formatTime, todayString } from '../../utils/format'

const metricCards = [
  { key: 'missing_key', label: '缺失 Key 次数' },
  { key: 'invalid_key', label: '无效 Key 次数' },
  { key: 'scope_denied', label: '权限拒绝次数' },
  { key: 'not_configured', label: '未配置拒绝次数' }
]

const date = ref(todayString())
const loading = ref(false)
const summary = ref({})
const metrics = ref({})

async function loadAll() {
  loading.value = true
  try {
    const [summaryData, metricsData] = await Promise.all([
      payApi.reconcileSummary(date.value),
      payApi.authMetrics()
    ])
    if (summaryData) summary.value = summaryData
    if (metricsData) metrics.value = metricsData
  } catch {
    // 拦截器已统一提示
  } finally {
    loading.value = false
  }
}

// 长间隔自动刷新（30s 固定，不退避、不设终态）
const { start } = usePolling(
  async (signal) => {
    const [summaryData, metricsData] = await Promise.all([
      payApi.reconcileSummary(date.value, { signal }),
      payApi.authMetrics({ signal })
    ])
    if (summaryData) summary.value = summaryData
    if (metricsData) metrics.value = metricsData
    return null
  },
  {
    isDone: () => false,
    initialInterval: 30000,
    maxInterval: 30000,
    maxAttempts: Number.MAX_SAFE_INTEGER
  }
)

onMounted(() => {
  loadAll()
  start()
})
</script>

<style scoped>
.section-card + .section-card {
  margin-top: 16px;
}

.section-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.section-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.stat-card :deep(.el-card__body) {
  text-align: center;
}

.time-stat-title {
  font-size: 13px;
  color: var(--el-text-color-secondary);
  margin-bottom: 8px;
}

.time-stat-value {
  font-size: 16px;
  font-weight: 600;
  color: var(--el-text-color-primary);
}
</style>
