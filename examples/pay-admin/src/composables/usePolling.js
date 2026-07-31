import { ref, onScopeDispose } from 'vue'

/**
 * 统一轮询原语（全应用禁止裸 setInterval）：
 * - 初始间隔 3s，指数退避（×1.5）至上限 15s
 * - 页面隐藏时暂停，恢复可见时继续
 * - 每 tick 独立 AbortController，停止时 abort 在途请求
 * - 达到最大次数后停止并置 exhausted，由页面转手动刷新
 *
 * @param {Function} task - async (signal) => any，抛错不中断轮询（取消错误除外）
 * @param {Object} options
 * @param {Function} options.isDone - (result) => boolean 终态谓词，true 时停止
 * @param {Function} [options.onTick] - (result) => void 每次成功回调
 * @param {number} [options.initialInterval=3000]
 * @param {number} [options.maxInterval=15000]
 * @param {number} [options.maxAttempts=60]
 */
export function usePolling(task, options) {
  const {
    isDone,
    onTick,
    initialInterval = 3000,
    maxInterval = 15000,
    maxAttempts = 60
  } = options

  const polling = ref(false)
  const exhausted = ref(false)
  const attempts = ref(0)

  let timer = null
  let controller = null
  let currentInterval = initialInterval
  let hiddenPaused = false

  async function tick() {
    timer = null
    if (!polling.value) return
    if (attempts.value >= maxAttempts) {
      polling.value = false
      exhausted.value = true
      return
    }
    attempts.value += 1
    controller = new AbortController()
    try {
      const result = await task(controller.signal)
      if (!polling.value) return
      if (onTick) onTick(result)
      if (isDone(result)) {
        polling.value = false
        return
      }
    } catch (err) {
      if (err?.isCanceled || !polling.value) return
      // 非取消错误：保留轮询，退避后重试
    } finally {
      controller = null
    }
    currentInterval = Math.min(currentInterval * 1.5, maxInterval)
    schedule()
  }

  function schedule() {
    if (!polling.value) return
    // 隐藏时不调度但必须标记暂停：tick 在途期间页面隐藏（timer 为空）时，
    // onVisibilityChange 的 timer 分支不会命中，恢复可见全靠此标记重新调度
    if (document.hidden) {
      hiddenPaused = true
      return
    }
    timer = setTimeout(tick, currentInterval)
  }

  function onVisibilityChange() {
    if (document.hidden) {
      if (timer) {
        clearTimeout(timer)
        timer = null
        hiddenPaused = true
      }
    } else if (hiddenPaused && polling.value) {
      hiddenPaused = false
      schedule()
    }
  }

  document.addEventListener('visibilitychange', onVisibilityChange)

  function start() {
    stop()
    polling.value = true
    exhausted.value = false
    attempts.value = 0
    currentInterval = initialInterval
    hiddenPaused = false
    // 首次立即执行
    tick()
  }

  function stop() {
    polling.value = false
    if (timer) {
      clearTimeout(timer)
      timer = null
    }
    if (controller) {
      controller.abort()
      controller = null
    }
  }

  onScopeDispose(() => {
    stop()
    document.removeEventListener('visibilitychange', onVisibilityChange)
  })

  return { polling, exhausted, attempts, start, stop }
}
