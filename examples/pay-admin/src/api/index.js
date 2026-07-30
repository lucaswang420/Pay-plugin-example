import axios from 'axios'

const apiClient = axios.create({
  baseURL: '/api',
  timeout: 30000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// Request interceptor
apiClient.interceptors.request.use(
  (config) => {
    // Add X-Api-Key header for backend authentication
    const apiKey = sessionStorage.getItem('api_key')
    if (!apiKey) {
      console.error('[API] No API key found in sessionStorage')
      return Promise.reject(new Error('请先登录或提供API密钥'))
    }
    config.headers['X-Api-Key'] = apiKey
    return config
  },
  (error) => {
    return Promise.reject(error)
  }
)

// Response interceptor
apiClient.interceptors.response.use(
  (response) => {
    console.log('[API] Response received:', response.status, response.data)
    return response.data
  },
  (error) => {
    console.error('[API] Request failed:', error)
    console.error('[API] Error config:', error.config)
    console.error('[API] Error response:', error.response)
    console.error('[API] Error response data:', error.response?.data)
    const message = error.response?.data?.message || error.message || 'Request failed'
    return Promise.reject(new Error(message))
  }
)

export default apiClient