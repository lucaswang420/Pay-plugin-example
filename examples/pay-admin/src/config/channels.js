import { markRaw } from 'vue'
import { Wallet, ChatDotRound } from '@element-plus/icons-vue'

// 支付渠道配置化：新渠道零代码接入（只需在此追加一项）
// qrField: /pay/create 响应 data 中承载二维码内容的字段名
export const CHANNELS = [
  {
    key: 'alipay',
    label: '支付宝',
    icon: markRaw(Wallet),
    qrField: 'qr_code'
  },
  {
    key: 'wechat',
    label: '微信支付',
    icon: markRaw(ChatDotRound),
    qrField: 'code_url'
  }
]

export function channelInfo(key) {
  return CHANNELS.find((c) => c.key === key)
}
