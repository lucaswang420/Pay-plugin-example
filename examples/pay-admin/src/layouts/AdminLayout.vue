<template>
  <el-container class="admin-layout">
    <el-aside :width="asideWidth" class="admin-aside">
      <div class="brand">
        <el-icon :size="22"><CreditCard /></el-icon>
        <span>Pay Admin</span>
      </div>
      <el-menu
        router
        :default-active="activeMenu"
        background-color="#001529"
        text-color="rgba(255, 255, 255, 0.68)"
        active-text-color="#ffffff"
      >
        <el-menu-item index="/payments/create">
          <el-icon><Plus /></el-icon>
          <span>创建支付</span>
        </el-menu-item>
        <el-menu-item index="/orders">
          <el-icon><List /></el-icon>
          <span>订单管理</span>
        </el-menu-item>
        <el-menu-item index="/refunds">
          <el-icon><RefreshLeft /></el-icon>
          <span>退款管理</span>
        </el-menu-item>
        <el-menu-item index="/dashboard">
          <el-icon><DataAnalysis /></el-icon>
          <span>对账与指标</span>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <el-container>
      <el-header class="admin-header">
        <div class="header-title">{{ pageTitle }}</div>
        <div class="header-actions">
          <el-tag v-if="userStore.hasApiKey" type="success" effect="plain" size="small">
            API Key 已配置
          </el-tag>
          <el-tag v-else type="danger" effect="plain" size="small">API Key 未配置</el-tag>
          <el-button text :icon="Setting" @click="openKeyDialog">设置</el-button>
        </div>
      </el-header>

      <el-main class="admin-main">
        <router-view />
      </el-main>
    </el-container>
  </el-container>

  <el-dialog v-model="keyDialogVisible" title="API 访问设置" width="460px">
    <el-form label-width="90px" @submit.prevent>
      <el-form-item label="API Key">
        <el-input
          v-model="keyForm.apiKey"
          type="password"
          show-password
          placeholder="请输入 X-Api-Key"
          clearable
        />
      </el-form-item>
      <el-form-item label="用户 ID">
        <el-input v-model="keyForm.userId" placeholder="创建支付时的 user_id" clearable />
      </el-form-item>
    </el-form>
    <template #footer>
      <el-button @click="keyDialogVisible = false">取消</el-button>
      <el-button type="primary" @click="saveKey">保存</el-button>
    </template>
  </el-dialog>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { useRoute } from 'vue-router'
import { Setting, CreditCard, Plus, List, RefreshLeft, DataAnalysis } from '@element-plus/icons-vue'
import { useUserStore } from '../stores/user'

const asideWidth = 'var(--app-aside-width)'
const route = useRoute()
const userStore = useUserStore()

const pageTitle = computed(() => route.meta.title || '')

const activeMenu = computed(() => {
  // 订单详情页归属订单菜单
  if (route.path.startsWith('/orders')) return '/orders'
  return route.path
})

const keyDialogVisible = ref(false)
const keyForm = reactive({ apiKey: '', userId: '' })

function openKeyDialog() {
  keyForm.apiKey = userStore.apiKey
  keyForm.userId = userStore.userId
  keyDialogVisible.value = true
}

function saveKey() {
  userStore.setCredentials({ apiKey: keyForm.apiKey.trim(), userId: keyForm.userId.trim() })
  keyDialogVisible.value = false
  ElMessage.success('已保存')
}

defineExpose({ openKeyDialog })
</script>

<style scoped>
.admin-layout {
  height: 100%;
}

.admin-aside {
  background-color: #001529;
  display: flex;
  flex-direction: column;
}

.brand {
  height: var(--app-header-height);
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: #ffffff;
  font-size: 17px;
  font-weight: 600;
  letter-spacing: 0.5px;
}

.admin-aside :deep(.el-menu) {
  border-right: none;
}

.admin-aside :deep(.el-menu-item.is-active) {
  background-color: var(--el-color-primary);
}

.admin-header {
  height: var(--app-header-height);
  background-color: #ffffff;
  border-bottom: 1px solid var(--el-border-color-light);
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.header-title {
  font-size: 16px;
  font-weight: 600;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.admin-main {
  background-color: var(--app-bg);
  overflow-y: auto;
}
</style>
