import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

const STORAGE_KEYS = { userId: 'user_id', apiKey: 'api_key' }

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
    if (key !== undefined) apiKey.value = key || ''
    persist()
  }

  function clearCredentials() {
    userId.value = ''
    apiKey.value = ''
    persist()
  }

  // 初始化：session 优先，其次 .env 默认值（开发/测试环境）
  function init() {
    const savedUserId = sessionStorage.getItem(STORAGE_KEYS.userId)
    const savedApiKey = sessionStorage.getItem(STORAGE_KEYS.apiKey)
    if (savedApiKey) {
      userId.value = savedUserId || ''
      apiKey.value = savedApiKey
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
