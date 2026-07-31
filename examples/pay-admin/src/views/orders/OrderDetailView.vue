<template>
  <div>
    <el-page-header content="订单详情" style="margin-bottom: 16px" @back="router.back()" />

    <el-card v-loading="loading" shadow="never">
      <el-alert
        v-if="degraded"
        type="warning"
        :closable="false"
        show-icon
        title="渠道查询降级，以下为本地数据库状态"
        style="margin-bottom: 16px"
      />

      <template v-if="order">
        <el-descriptions :column="2" border>
          <el-descriptions-item label="订单号">{{ order.order_no }}</el-descriptions-item>
          <el-descriptions-item label="支付流水号">{{ order.payment_no || '-' }}</el-descriptions-item>
          <el-descriptions-item label="状态">
            <el-tag :type="statusInfo.tagType" size="small">{{ statusInfo.label }}</el-tag>
          </el-descriptions-item>
          <el-descriptions-item label="金额">{{ formatAmount(order.amount) }}</el-descriptions-item>
          <el-descriptions-item label="渠道">{{ channelLabel }}</el-descriptions-item>
          <el-descriptions-item label="用户 ID">{{ order.user_id || '-' }}</el-descriptions-item>
          <!-- /pay/query 用 title 字段承载商品描述 -->
          <el-descriptions-item label="商品描述">{{ order.description || order.title || '-' }}</el-descriptions-item>
          <el-descriptions-item label="创建时间">{{ formatTime(order.created_at) }}</el-descriptions-item>
          <el-descriptions-item label="更新时间">{{ formatTime(order.updated_at) }}</el-descriptions-item>
          <el-descriptions-item label="支付时间">{{ formatTime(order.paid_at) }}</el-descriptions-item>
        </el-descriptions>

        <el-collapse v-if="channelResponses.length" style="margin-top: 16px">
          <el-collapse-item
            v-for="item in channelResponses"
            :key="item.key"
            :title="item.title"
            :name="item.key"
          >
            <pre class="json-block">{{ item.pretty }}</pre>
          </el-collapse-item>
        </el-collapse>

        <div class="actions">
          <el-button :icon="Refresh" :loading="loading" @click="load">刷新状态</el-button>
          <el-button
            v-if="order.status === 'PAID'"
            type="warning"
            :icon="RefreshLeft"
            @click="goRefund"
          >
            发起退款
          </el-button>
        </div>
      </template>

      <el-empty v-else-if="!loading" description="未查询到该订单" />
    </el-card>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { Refresh, RefreshLeft } from '@element-plus/icons-vue'
import { payApi } from '../../api/pay'
import { formatAmount, formatTime } from '../../utils/format'
import { orderStatusInfo } from '../../utils/status'
import { channelInfo } from '../../config/channels'

const props = defineProps({
  orderNo: { type: String, required: true }
})

const router = useRouter()
const loading = ref(false)
const order = ref(null)
const degraded = ref(false)

const statusInfo = computed(() => orderStatusInfo(order.value?.status))
const channelLabel = computed(
  () => channelInfo(order.value?.channel)?.label || order.value?.channel || '-'
)

// 渠道原始响应字段折叠展示（格式化 JSON）
const channelResponses = computed(() => {
  if (!order.value) return []
  const fields = [
    { key: 'alipay_response', title: '支付宝渠道响应' },
    { key: 'wechat_response', title: '微信渠道响应' },
    { key: 'channel_response', title: '渠道响应' }
  ]
  return fields
    .filter((f) => order.value[f.key])
    .map((f) => {
      let pretty = order.value[f.key]
      try {
        const parsed = typeof pretty === 'string' ? JSON.parse(pretty) : pretty
        pretty = JSON.stringify(parsed, null, 2)
      } catch {
        pretty = String(pretty)
      }
      return { ...f, pretty }
    })
})

// 进入时调 /query 实时同步渠道状态
async function load() {
  loading.value = true
  try {
    const data = await payApi.query(props.orderNo)
    order.value = data
    degraded.value = Boolean(data?.degraded)
  } catch {
    // 拦截器已统一提示
  } finally {
    loading.value = false
  }
}

function goRefund() {
  router.push({
    path: '/refunds',
    query: { order_no: order.value.order_no, amount: order.value.amount }
  })
}

onMounted(load)
</script>

<style scoped>
.json-block {
  margin: 0;
  padding: 12px;
  background-color: var(--el-fill-color-light);
  border-radius: 4px;
  font-size: 12px;
  line-height: 1.6;
  overflow-x: auto;
  white-space: pre-wrap;
  word-break: break-all;
}

.actions {
  margin-top: 16px;
  display: flex;
  gap: 10px;
}
</style>
