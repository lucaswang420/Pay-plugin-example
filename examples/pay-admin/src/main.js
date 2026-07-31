import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import router from './router'
import './styles/index.css'

// 图标按需在各组件内显式引入（全量注册会使 icons 整包不可 tree-shake）
const app = createApp(App)

app.use(createPinia())
app.use(router)

app.mount('#app')
