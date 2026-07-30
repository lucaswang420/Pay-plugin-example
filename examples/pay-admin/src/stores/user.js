import { defineStore } from 'pinia'
import { ref } from 'vue'

export const useUserStore = defineStore('user', () => {
  const userId = ref(null)
  const apiKey = ref(null)
  const isLoggedIn = ref(false)

  function login(userIdValue, apiKeyValue) {
    userId.value = userIdValue
    apiKey.value = apiKeyValue
    isLoggedIn.value = true
    sessionStorage.setItem('user_id', userIdValue)
    sessionStorage.setItem('api_key', apiKeyValue)
  }

  function logout() {
    userId.value = null
    apiKey.value = null
    isLoggedIn.value = false
    sessionStorage.removeItem('user_id')
    sessionStorage.removeItem('api_key')
  }

  function loadFromSession() {
    const savedUserId = sessionStorage.getItem('user_id')
    const savedApiKey = sessionStorage.getItem('api_key')
    if (savedUserId && savedApiKey) {
      userId.value = parseInt(savedUserId)
      apiKey.value = savedApiKey
      isLoggedIn.value = true
    }
  }

  function loadFromEnv() {
    // 从环境变量加载默认值（开发/测试环境）
    const defaultUserId = import.meta.env.VITE_DEFAULT_USER_ID
    const defaultApiKey = import.meta.env.VITE_DEFAULT_API_KEY

    if (defaultUserId && defaultApiKey) {
      userId.value = parseInt(defaultUserId)
      apiKey.value = defaultApiKey
      isLoggedIn.value = true
      // 也保存到sessionStorage，确保一致性
      sessionStorage.setItem('user_id', defaultUserId)
      sessionStorage.setItem('api_key', defaultApiKey)
    }
  }

  return {
    userId,
    apiKey,
    isLoggedIn,
    login,
    logout,
    loadFromSession,
    loadFromEnv
  }
})
