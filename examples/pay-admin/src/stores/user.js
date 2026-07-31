import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

const STORAGE_KEYS = { userId: 'user_id', apiKey: 'api_key', envOptOut: 'api_key_env_opt_out' }

export const useUserStore = defineStore('user', () => {
  const userId = ref('')
  const apiKey = ref('')

  const hasApiKey = computed(() => Boolean(apiKey.value))

  // 持久化收敛为单函数
  function persist() {
    if (userId.value) {
      sessionStorage.setItem(STORAGE_KEYS.userId, String(userId.value))
    } else {
      sessionStorage.removeItem(STORAGE_KEYS.userId)
    }
    if (apiKey.value) {
      sessionStorage.setItem(STORAGE_KEYS.apiKey, apiKey.value)
    } else {
      sessionStorage.removeItem(STORAGE_KEYS.apiKey)
    }
  }

  function setCredentials({ userId: uid, apiKey: key }) {
    if (uid !== undefined) userId.value = uid ? String(uid) : ''
    if (key !== undefined) {
      apiKey.value = key || ''
      // 用户显式保存空 Key 时记录退出标记，阻止 init() 再从 .env 回填；
      // 保存非空 Key 则清除标记，恢复默认行为
      if (key) {
        sessionStorage.removeItem(STORAGE_KEYS.envOptOut)
      } else {
        sessionStorage.setItem(STORAGE_KEYS.envOptOut, '1')
      }
    }
    persist()
  }

  function clearCredentials() {
    userId.value = ''
    apiKey.value = ''
    persist()
  }

  // 初始化：session 优先，其次 .env 默认值（开发/测试环境）；
  // 用户显式清空过 Key（envOptOut 标记）时不回填，便于测试 401 场景
  function init() {
    const savedUserId = sessionStorage.getItem(STORAGE_KEYS.userId)
    const savedApiKey = sessionStorage.getItem(STORAGE_KEYS.apiKey)
    if (savedApiKey) {
      userId.value = savedUserId || ''
      apiKey.value = savedApiKey
      return
    }
    if (sessionStorage.getItem(STORAGE_KEYS.envOptOut)) {
      userId.value = savedUserId || ''
      return
    }
    loadFromEnv()
  }

  function loadFromEnv() {
    const defaultUserId = import.meta.env.VITE_DEFAULT_USER_ID
    const defaultApiKey = import.meta.env.VITE_DEFAULT_API_KEY
    if (defaultApiKey) {
      userId.value = defaultUserId ? String(defaultUserId) : ''
      apiKey.value = defaultApiKey
      sessionStorage.removeItem(STORAGE_KEYS.envOptOut)
      persist()
    }
  }

  return {
    userId,
    apiKey,
    hasApiKey,
    setCredentials,
    clearCredentials,
    init,
    loadFromEnv
  }
})
