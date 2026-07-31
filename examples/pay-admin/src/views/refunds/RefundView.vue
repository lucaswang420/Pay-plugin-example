<template>
  <el-row :gutter="16">
    <el-col :span="12">
      <el-card shadow="never">
        <template #header>
          <span>发起退款</span>
        </template>
        <el-form
          ref="formRef"
          :model="form"
          :rules="rules"
          label-width="90px"
          :disabled="submitting"
        >
          <el-form-item label="订单号" prop="order_no">
            <el-input v-model="form.order_no" placeholder="待退款的订单号" clearable />
          </el-form-item>
          <el-form-item label="退款方式">
            <el-radio-group v-model="form.mode">
              <el-radio value="full" :disabled="!orderAmount">全额退款</el-radio>
              <el-radio value="partial">部分退款</el-radio>
            </el-radio-group>
          </el-form-item>
          <el-form-item label="退款金额" prop="amount">
            <el-input-number
              v-model="form.amount"
              :min="0.01"
              :max="orderAmount || undefined"
              :precision="2"
              :step="1"
              :disabled="form.mode === 'full' && Boolean(orderAmount)"
              style="width: 200px"
            />
            <span v-if="orderAmount" class="amount-hint">订单金额 {{ formatAmount(orderAmount) }}</span>
          </el-form-item>
          <el-form-item label="退款原因" prop="reason">
            <el-input v-model="form.reason" type="textarea" :rows="2" maxlength="128" show-word-limit />
          </el-form-item>
          <el-form-item>
            <el-button type="warning" :loading="submitting" @click="submit">提交退款</el-button>
          </el-form-item>
        </el-form>

        <el-alert
          type="info"
          :closable="false"
          show-icon
          title="支付宝退款状态以数据库快照为准（后端仅微信渠道实时刷新退款状态）"
        />
      </el-card>
    </el-col>

    <el-col :span="12">
      <el-card shadow="never">
        <template #header>
          <div class="progress-header">
            <span>退款进度</span>
            <el-input
              v-model="queryRefundNo"
              placeholder="输入 refund_no 查询历史退款"
              clearable
              style="width: 280px"
              @keyup.enter="queryManual"
            >
              <template #append>
                <el-button :icon="Search" @click="queryManual" />
              </template>
            </el-input>
          </div>
        </template>

        <template v-if="refund">
          <el-steps :active="stepActive" align-center finish-status="success" :process-status="stepProcessStatus">
            <el-step title="退款受理" />
            <el-step title="退款中" />
            <el-step title="完成" />
          </el-steps>

          <div class="status-line">
            <el-tag :type="statusInfo.tagType">{{ statusInfo.label }}</el-tag>
            <span v-if="polling" class="poll-tip">
              <el-icon class="is-loading"><Loading /></el-icon>
              正在轮询退款状态（第 {{ attempts }} 次）…
            </span>
            <el-button v-if="exhausted" size="small" :icon="Refresh" @click="refreshOnce">
              手动刷新
            </el-button>
          </div>

          <el-descriptions :column="1" size="small" border style="margin-top: 16px">
            <el-descriptions-item label="退款单号">{{ refund.refund_no }}</el-descriptions-item>
            <el-descriptions-item label="订单号">{{ refund.order_no || form.order_no || '-' }}</el-descriptions-item>
            <el-descriptions-item label="退款金额">{{ formatAmount(refund.refund_amount) }}</el-descriptions-item>
            <el-descriptions-item label="渠道退款号">{{ refund.channel_refund_no || '-' }}</el-descriptions-item>
          </el-descriptions>
        </template>

        <el-empty v-else description="提交退款或输入退款单号后展示进度" :image-size="80" />
      </el-card>
    </el-col>
  </el-row>
</template>

<script setup>
import { computed, onMounted, reactive, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { Search, Refresh, Loading } from '@element-plus/icons-vue'
import { payApi } from '../../api/pay'
import { usePolling } from '../../composables/usePolling'
import { formatAmount } from '../../utils/format'
import { refundStatusInfo } from '../../utils/status'

const route = useRoute()

const formRef = ref(null)
const form = reactive({
  order_no: '',
  mode: 'partial',
  amount: null,
  reason: '用户申请退款'
})
// 从订单详情跳转时预填的订单金额（作为退款上限）
const orderAmount = ref(0)

// 金额统一收敛到分精度，避免 el-input-number / Number(query) 带入浮点尾差（如 0.30000001）
const toCents = (value) => Math.round(Number(value) * 100) / 100

const rules = {
  order_no: [{ required: true, message: '请输入订单号', trigger: 'blur' }],
  amount: [{ required: true, message: '请输入退款金额' }]
}

const submitting = ref(false)
const refund = ref(null)
const status = ref('')
const queryRefundNo = ref('')

const statusInfo = computed(() => refundStatusInfo(status.value))
const isFinal = computed(
  () => status.value === 'REFUND_SUCCESS' || status.value === 'REFUND_FAIL'
)
const stepActive = computed(() => {
  if (status.value === 'REFUND_SUCCESS') return 3
  if (status.value === 'REFUND_FAIL') return 2
  if (status.value === 'REFUNDING') return 1
  return 0
})
const stepProcessStatus = computed(() =>
  status.value === 'REFUND_FAIL' ? 'error' : 'process'
)

const { polling, exhausted, attempts, start, stop } = usePolling(
  (signal) => payApi.refundQuery(refund.value.refund_no, { signal }),
  {
    isDone: (data) =>
      data?.status === 'REFUND_SUCCESS' || data?.status === 'REFUND_FAIL',
    onTick: (data) => {
      if (data?.status) status.value = data.status
      if (data) refund.value = { ...refund.value, ...data }
    }
  }
)

// 全额退款模式下金额锁定为订单金额
watch(
  () => form.mode,
  (mode) => {
    if (mode === 'full' && orderAmount.value) {
      form.amount = toCents(orderAmount.value)
    }
  }
)

async function submit() {
  const valid = await formRef.value.validate().catch(() => false)
  if (!valid) return
  submitting.value = true
  try {
    const data = await payApi.refund({
      order_no: form.order_no,
      amount: toCents(form.amount).toFixed(2),
      reason: form.reason
    })
    refund.value = { order_no: form.order_no, ...data }
    status.value = data?.status || 'REFUND_INIT'
    ElMessage.success(`退款已受理：${data.refund_no}`)
    start()
  } catch (e) {
    // 业务失败（如渠道退款失败）时退款单已落库，展示 refund_no 与终态进度
    if (e?.bizData?.refund_no) {
      refund.value = { order_no: form.order_no, ...e.bizData }
      status.value = e.bizData.status || 'REFUND_FAIL'
    }
    // 错误提示由拦截器统一弹出
  } finally {
    submitting.value = false
  }
}

async function queryManual() {
  const refundNo = queryRefundNo.value.trim()
  if (!refundNo) {
    ElMessage.warning('请输入退款单号')
    return
  }
  stop()
  try {
    const data = await payApi.refundQuery(refundNo)
    refund.value = { refund_no: refundNo, ...data }
    status.value = data?.status || ''
    if (!isFinal.value && status.value) {
      start()
    }
  } catch {
    // 拦截器已统一提示
  }
}

async function refreshOnce() {
  try {
    const data = await payApi.refundQuery(refund.value.refund_no)
    if (data?.status) status.value = data.status
    if (data) refund.value = { ...refund.value, ...data }
  } catch {
    // 拦截器已统一提示
  }
}

onMounted(() => {
  // 支持 /refunds?order_no=xxx&amount=xxx 预填（订单详情跳转）
  const { order_no: orderNo, amount } = route.query
  if (orderNo) form.order_no = String(orderNo)
  if (amount && !Number.isNaN(Number(amount))) {
    orderAmount.value = toCents(amount)
    form.mode = 'full'
    form.amount = toCents(amount)
  }
})
</script>

<style scoped>
.amount-hint {
  margin-left: 10px;
  color: var(--el-text-color-secondary);
  font-size: 13px;
}

.progress-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}

.status-line {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-top: 20px;
  justify-content: center;
}

.poll-tip {
  display: flex;
  align-items: center;
  gap: 6px;
  color: var(--el-text-color-secondary);
  font-size: 13px;
}

.el-alert {
  margin-top: 8px;
}
</style>
