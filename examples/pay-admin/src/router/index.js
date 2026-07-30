import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    redirect: '/step/1'
  },
  {
    path: '/step/:stepNumber',
    name: 'step',
    component: () => import('../views/StepView.vue'),
    props: true
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router