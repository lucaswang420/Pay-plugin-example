<template>
  <el-card shadow="never">
    <template #header>
      <div class="list-header">
        <span>订单列表</span>
        <div class="filters">
          <el-select
            v-model="filters.status"
            placeholder="全部状态"
            clearable
            style="width: 140px"
            @change="reload"
          >
            <el-option
              v-for="opt in ORDER_STATUS_OPTIONS"
              :key="opt.value"
              :value="opt.value"
              :label="opt.label"
            />
          </el-select>
          <el-input
            v-model="filters.userId"
            placeholder="按用户 ID 搜索"
            clearable
            style="width: 200px"
            @keyup.enter="reload"
            @clear="reload"
          >
            <template #append>
              <el-button :icon="Search" @click="reload" />
            </template>
          </el-input>
          <el-button :icon="Refresh" @click="fetchPage()">刷新</el-button>
        </div>
      </div>
    </template>

    <el-table v-loading="loading" :data="orders" stripe @row-click="goDetail">
      <el-table-column prop="order_no" label="订单号" min-width="220" show-overflow-tooltip />
      <el-table-column prop="user_id" label="用户 ID" min-width="140" show-overflow-tooltip />
      <el-table-column label="金额" width="120">
        <template #default="{ row }">{{ formatAmount(row.amount) }}</template>
      </el-table-column>
      <el-table-column label="渠道" width="100">
        <template #default="{ row }">{{ channelLabel(row.channel) }}</template>
      </el-table-column>
      <el-table-column label="状态" width="110">
        <template #default="{ row }">
          <el-tag :type="orderStatusInfo(row.status).tagType" size="small">
            {{ orderStatusInfo(row.status).label }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column label="创建时间" width="170">
        <template #default="{ row }">{{ formatTime(row.created_at) }}</template>
      </el-table-column>
      <el-table-column label="操作" width="90" fixed="right">
        <template #default="{ row }">
          <el-button link type="primary" @click.stop="goDetail(row)">详情</el-button>
        </template>
      </el-table-column>
      <template #empty>
        <el-empty description="暂无订单" :image-size="80" />
      </template>
    </el-table>

    <!-- 后端无 total 字段，采用 limit+1 探测法，仅提供上一页/下一页 -->
    <div class="pager">
      <el-button :icon="ArrowLeft" :disabled="page === 0 || loading" @click="prevPage">
        上一页
      </el-button>
      <span class="page-indicator">第 {{ page + 1 }} 页</span>
      <el-button :disabled="!hasNext || loading" @click="nextPage">
        下一页<el-icon class="el-icon--right"><ArrowRight /></el-icon>
      </el-button>
    </div>
  </el-card>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { Search, Refresh, ArrowLeft, ArrowRight } from '@element-plus/icons-vue'
import { payApi } from '../../api/pay'
import { formatAmount, formatTime } from '../../utils/format'
import { orderStatusInfo, ORDER_STATUS_OPTIONS } from '../../utils/status'
import { channelInfo } from '../../config/channels'

const PAGE_SIZE = 20

const router = useRouter()
const loading = ref(false)
const orders = ref([])
const page = ref(0)
const hasNext = ref(false)
const filters = reactive({ status: '', userId: '' })

function channelLabel(key) {
  return channelInfo(key)?.label || key || '-'
}

async function fetchPage() {
  loading.value = true
  try {
    // limit+1 探测：多取 1 条判断是否有下一页
    const data = await payApi.orders({
      status: filters.status || undefined,
      userId: filters.userId || undefined,
      limit: PAGE_SIZE + 1,
      offset: page.value * PAGE_SIZE
    })
    const list = Array.isArray(data) ? data : []
    hasNext.value = list.length > PAGE_SIZE
    orders.value = list.slice(0, PAGE_SIZE)
  } catch {
    // 拦截器已统一提示
  } finally {
    loading.value = false
  }
}

function reload() {
  page.value = 0
  fetchPage()
}

function prevPage() {
  if (page.value === 0) return
  page.value -= 1
  fetchPage()
}

function nextPage() {
  if (!hasNext.value) return
  page.value += 1
  fetchPage()
}

function goDetail(row) {
  router.push(`/orders/${encodeURIComponent(row.order_no)}`)
}

onMounted(fetchPage)
</script>

<style scoped>
.list-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 10px;
}

.filters {
  display: flex;
  align-items: center;
  gap: 10px;
}

.el-table :deep(.el-table__row) {
  cursor: pointer;
}

.pager {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 16px;
  margin-top: 16px;
}

.page-indicator {
  color: var(--el-text-color-secondary);
  font-size: 14px;
}
</style>
