<template>
  <el-row :gutter="16">
    <el-col :span="payment ? 12 : 24" :lg="payment ? 12 : 14">
      <el-card shadow="never">
        <template #header>
          <span>创建支付订单</span>
        </template>
        <el-form
          ref="formRef"
          :model="form"
          :rules="rules"
          label-width="90px"
          :disabled="submitting"
        >
          <el-form-item label="支付渠道" prop="channel">
            <el-radio-group v-model="form.channel">
              <el-radio-button v-for="c in CHANNELS" :key="c.key" :value="c.key">
                <el-icon style="vertical-align: -2px; margin-right: 4px">
                  <component :is="c.icon" />
                </el-icon>
                {{ c.label }}
              </el-radio-button>
            </el-radio-group>
          </el-form-item>
          <el-form-item label="订单号" prop="order_no">
            <el-input v-model="form.order_no" clearable>
              <template #append>
                <el-button :icon="Refresh" @click="regenerateOrderNo">重新生成</el-button>
              </template>
            </el-input>
          </el-form-item>
          <el-form-item label="金额" prop="amount">
            <el-input v-model="form.amount" placeholder="如 9.99（最多两位小数）">
              <template #prefix>¥</template>
            </el-input>
          </el-form-item>
          <el-form-item label="用户 ID" prop="user_id">
            <el-input v-model="form.user_id" placeholder="user_id" clearable />
          </el-form-item>
          <el-form-item label="商品描述" prop="description">
            <el-input v-model="form.description" type="textarea" :rows="2" maxlength="128" show-word-limit />
          </el-form-item>
          <el-form-item>
            <el-button type="primary" :loading="submitting" @click="submit">创建支付</el-button>
            <el-button v-if="payment" @click="resetAll">再创建一笔</el-button>
          </el-form-item>
        </el-form>
      </el-card>
    </el-col>

    <el-col v-if="payment" :span="12" :lg="12">
      <el-card shadow="never">
        <template #header>
          <span>支付进度</span>
        </template>

        <el-steps :active="stepActive" align-center finish-status="success" :process-status="stepProcessStatus">
          <el-step title="创建订单" />
          <el-step title="待支付" />
          <el-step title="完成" />
        </el-steps>

        <el-alert
          v-if="degraded"
          type="warning"
          :closable="false"
          show-icon
          title="渠道查询降级，以本地状态为准"
          style="margin-top: 16px"
        />

        <div v-if="!isFinal" class="qr-area">
          <img v-if="qrDataUrl" :src="qrDataUrl" alt="支付二维码" class="qr-img" />
          <el-skeleton v-else animated style="width: 220px">
            <template #template>
              <el-skeleton-item variant="image" style="width: 220px; height: 220px" />
            </template>
          </el-skeleton>
          <p class="qr-tip">
            请使用{{ channelLabel }}扫码支付
            <el-tag :type="statusInfo.tagType" size="small" style="margin-left: 8px">
              {{ statusInfo.label }}
            </el-tag>
          </p>
          <p v-if="polling" class="poll-tip">
            <el-icon class="is-loading"><Loading /></el-icon>
            正在轮询支付状态（第 {{ attempts }} 次）…
          </p>
          <el-button v-if="exhausted" :icon="Refresh" @click="refreshOnce">手动刷新状态</el-button>
        </div>

        <el-result
          v-else-if="status === 'PAID'"
          icon="success"
          title="支付成功"
          :sub-title="`订单号 ${payment.order_no}`"
        >
          <template #extra>
            <el-button type="primary" @click="goDetail">查看订单详情</el-button>
          </template>
        </el-result>

        <el-result
          v-else
          icon="error"
          title="支付失败"
          :sub-title="`订单号 ${payment.order_no}`"
        >
          <template #extra>
            <el-button @click="resetAll">重新创建</el-button>
          </template>
        </el-result>

        <el-descriptions v-if="payment" :column="1" size="small" border style="margin-top: 16px">
          <el-descriptions-item label="订单号">{{ payment.order_no }}</el-descriptions-item>
          <el-descriptions-item label="支付流水号">{{ payment.payment_no || '-' }}</el-descriptions-item>
          <el-descriptions-item label="金额">{{ formatAmount(form.amount) }}</el-descriptions-item>
        </el-descriptions>
      </el-card>
    </el-col>
  </el-row>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { Refresh, Loading } from '@element-plus/icons-vue'
import QRCode from 'qrcode'
import { payApi } from '../../api/pay'
import { usePolling } from '../../composables/usePolling'
import { CHANNELS, channelInfo } from '../../config/channels'
import { formatAmount } from '../../utils/format'
import { orderStatusInfo } from '../../utils/status'
import { useUserStore } from '../../stores/user'

const router = useRouter()
const userStore = useUserStore()

function generateOrderNo() {
  const ts = Date.now()
  const rand = Math.floor(Math.random() * 9000) + 1000
  return `ORDER_${ts}_${rand}`
}

const formRef = ref(null)
const form = reactive({
  channel: CHANNELS[0].key,
  order_no: generateOrderNo(),
  amount: '',
  user_id: userStore.userId || '',
  description: '测试商品'
})

// 金额校验对齐后端正则
const AMOUNT_RE = /^\d+(\.\d{1,2})?$/
const rules = {
  channel: [{ required: true, message: '请选择支付渠道' }],
  order_no: [{ required: true, message: '请输入订单号', trigger: 'blur' }],
  amount: [
    { required: true, message: '请输入金额', trigger: 'blur' },
    {
      validator: (_, value, cb) => {
        if (!AMOUNT_RE.test(value)) {
          cb(new Error('金额格式：正数，最多两位小数'))
        } else if (Number(value) <= 0) {
          cb(new Error('金额必须大于 0'))
        } else {
          cb()
        }
      },
      trigger: 'blur'
    }
  ],
  user_id: [
    { required: true, message: '请输入用户 ID', trigger: 'blur' },
    { pattern: /^\d+$/, message: '用户 ID 必须为纯数字', trigger: 'blur' }
  ]
}

const submitting = ref(false)
const payment = ref(null)
const qrDataUrl = ref('')
const status = ref('PAYING')
const degraded = ref(false)

const channelLabel = computed(() => channelInfo(form.channel)?.label || form.channel)
const statusInfo = computed(() => orderStatusInfo(status.value))
const isFinal = computed(() => status.value === 'PAID' || status.value === 'FAILED')
const stepActive = computed(() => {
  if (!payment.value) return 0
  if (status.value === 'PAID') return 3
  if (status.value === 'FAILED') return 2
  return 1
})
const stepProcessStatus = computed(() => (status.value === 'FAILED' ? 'error' : 'process'))

const { polling, exhausted, attempts, start, stop } = usePolling(
  (signal) => payApi.query(form.order_no, { signal }),
  {
    isDone: (data) => data?.status === 'PAID' || data?.status === 'FAILED',
    onTick: (data) => {
      if (data?.status) status.value = data.status
      degraded.value = Boolean(data?.degraded)
    }
  }
)

function regenerateOrderNo() {
  form.order_no = generateOrderNo()
}

async function submit() {
  const valid = await formRef.value.validate().catch(() => false)
  if (!valid) return
  submitting.value = true
  try {
    const data = await payApi.create(
      {
        order_no: form.order_no,
        amount: form.amount,
        channel: form.channel,
        // 后端 asInt64() 要求数字类型，字符串会触发框架 500
        user_id: Number(form.user_id),
        description: form.description
      },
      { idempotencyKey: form.order_no }
    )
    payment.value = data
    status.value = data?.status || 'PAYING'
    degraded.value = false
    await renderQrCode(data)
    start()
  } catch (err) {
    if (!err?.isCanceled && !err?.friendlyMessage && err?.message) {
      // 拦截器已提示的错误不重复弹
    }
  } finally {
    submitting.value = false
  }
}

async function renderQrCode(data) {
  const qrField = channelInfo(form.channel)?.qrField
  const content = qrField ? data?.[qrField] : null
  if (!content) {
    qrDataUrl.value = ''
    ElMessage.warning('响应中未包含二维码内容')
    return
  }
  // toDataURL 生成 data URL 灌 img，避免 canvas ref 竞态
  qrDataUrl.value = await QRCode.toDataURL(content, { width: 220, margin: 1 })
}

async function refreshOnce() {
  try {
    const data = await payApi.query(form.order_no)
    if (data?.status) status.value = data.status
    degraded.value = Boolean(data?.degraded)
    if (!isFinal.value) {
      ElMessage.info(`当前状态：${statusInfo.value.label}`)
    }
  } catch {
    // 拦截器已统一提示
  }
}

function resetAll() {
  stop()
  payment.value = null
  qrDataUrl.value = ''
  status.value = 'PAYING'
  degraded.value = false
  form.order_no = generateOrderNo()
  form.amount = ''
}

function goDetail() {
  router.push(`/orders/${encodeURIComponent(payment.value.order_no)}`)
}
</script>

<style scoped>
.qr-area {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 20px 0 8px;
}

.qr-img {
  width: 220px;
  height: 220px;
  border: 1px solid var(--el-border-color-light);
  border-radius: 6px;
}

.qr-tip {
  margin-top: 12px;
  color: var(--el-text-color-regular);
}

.poll-tip {
  margin-top: 8px;
  display: flex;
  align-items: center;
  gap: 6px;
  color: var(--el-text-color-secondary);
  font-size: 13px;
}
</style>
