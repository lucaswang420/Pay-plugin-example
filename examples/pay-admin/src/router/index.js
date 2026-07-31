import { createRouter, createWebHistory } from 'vue-router'
import { useUserStore } from '../stores/user'

const routes = [
  {
    path: '/',
    component: () => import('../layouts/AdminLayout.vue'),
    children: [
      {
        path: '',
        redirect: '/payments/create'
      },
      {
        path: 'payments/create',
        name: 'payment-create',
        component: () => import('../views/payments/CreatePaymentView.vue'),
        meta: { title: '创建支付' }
      },
      {
        path: 'orders',
        name: 'order-list',
        component: () => import('../views/orders/OrderListView.vue'),
        meta: { title: '订单管理' }
      },
      {
        path: 'orders/:orderNo',
        name: 'order-detail',
        component: () => import('../views/orders/OrderDetailView.vue'),
        props: true,
        meta: { title: '订单详情' }
      },
      {
        path: 'refunds',
        name: 'refunds',
        component: () => import('../views/refunds/RefundView.vue'),
        meta: { title: '退款管理' }
      },
      {
        path: 'dashboard',
        name: 'dashboard',
        component: () => import('../views/dashboard/DashboardView.vue'),
        meta: { title: '对账与指标' }
      }
    ]
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

let warnedMissingKey = false

// 无 key 仅提示不阻塞路由；请求层 401 有统一兜底
router.beforeEach(() => {
  const userStore = useUserStore()
  if (!userStore.hasApiKey && !warnedMissingKey) {
    warnedMissingKey = true
    ElMessage.warning('尚未配置 API Key，请通过顶栏"设置"填写后再操作')
  }
  return true
})

export default router
