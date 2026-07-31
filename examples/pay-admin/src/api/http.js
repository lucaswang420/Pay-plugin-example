import axios from 'axios'
import { useUserStore } from '../stores/user'

// ElMessage 由 unplugin-auto-import 按需注入（手动 barrel import 会打入全量 element-plus）

// 鉴权失败时后端返回纯文本 body，必须按 HTTP status 归一化提示文案
const AUTH_ERROR_TEXT = {
  401: 'API Key 缺失或无效，请在右上角设置有效的 Key',
  403: '当前 API Key 权限不足（scope 不允许该操作）',
  503: '服务端未配置 API Key，请检查 pay-server 配置'
}

const http = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || '/api',
  timeout: 15000
})

// 有 key 则注入 X-Api-Key；无 key 不拦截，交给响应侧 401 统一处理
http.interceptors.request.use((config) => {
  const userStore = useUserStore()
  if (userStore.apiKey) {
    config.headers['X-Api-Key'] = userStore.apiKey
  }
  return config
})

http.interceptors.response.use(
  (response) => {
    const payload = response.data
    // 非 {code,...} 包裹（如 /metrics/auth 平铺 JSON）直接透传
    if (payload === null || typeof payload !== 'object' || payload.code === undefined) {
      return payload
    }
    // code=1：渠道查询降级，透传 data 并附 degraded 标记
    if (payload.code === 1) {
      return { ...(payload.data || {}), degraded: true, degradedMessage: payload.message }
    }
    // code=0 或 200 均为成功（后端两种成功码并存）
    if (payload.code === 0 || payload.code === 200) {
      return payload.data
    }
    const error = new Error(payload.message || '请求失败')
    error.bizCode = payload.code
    // 业务失败时后端 data 可能携带已落库的单据（如退款 REFUND_FAIL 含 refund_no），透传给调用方
    error.bizData = payload.data
    ElMessage.error(error.message)
    return Promise.reject(error)
  },
  (error) => {
    // 主动取消的请求（轮询 abort / 路由切换）静默处理
    if (axios.isCancel(error) || error.code === 'ERR_CANCELED') {
      error.isCanceled = true
      return Promise.reject(error)
    }
    const status = error.response?.status
    if (status && AUTH_ERROR_TEXT[status]) {
      error.friendlyMessage = AUTH_ERROR_TEXT[status]
    } else if (status) {
      error.friendlyMessage = `请求失败（HTTP ${status}）`
    } else {
      error.friendlyMessage = '网络异常，请检查 pay-server 是否已启动'
    }
    ElMessage.error(error.friendlyMessage)
    return Promise.reject(error)
  }
)

export default http
