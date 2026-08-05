# Drogon Payment Plugin - Logging Standards

> **权威来源 (Source of truth):** [`TECH_SPECS.md`](../../TECH_SPECS.md) →
> 「[日志分级规范](../../TECH_SPECS.md#must-日志分级规范)」。
> 本文件仅作导航与速查；六等级定义、典型场景与硬性约定以 TECH_SPECS.md 为准。
> 出现冲突时以 TECH_SPECS.md 为准。

## 六等级速查

| 等级 | 用途 | 典型场景 |
|------|------|----------|
| `LOG_TRACE` | 最细粒度追踪 | 函数入参/出参、循环迭代、逐行执行轨迹；深度调试时按需开启 |
| `LOG_DEBUG` | 调试信息 | 变量值、分支走向、内部状态变化、**per-request 流程步骤** |
| `LOG_INFO` | 常规信息 | **生命周期/里程碑事件**（服务启动/停止、通道注册、任务完成） |
| `LOG_WARN` | 警告 | 可恢复降级：fire-and-forget 辅助写入（账本/幂等快照）失败、配置用默认值、资源接近阈值 |
| `LOG_ERROR` | 错误 | 影响单次操作但服务整体可用的失败（请求异常、DB 连接失败） |
| `LOG_FATAL` | 致命错误 | 进程无法继续（启动期配置/环境变量校验失败退出）；应极少出现 |

## 关键硬性约定

- **`LOG_INFO` 仅用于生命周期/里程碑事件**；per-request 流程步骤、分支决定、变量值一律用 `LOG_DEBUG`。
- **fire-and-forget 辅助写入**（返回 void、不阻断主流程，如账本、幂等快照）失败用 `LOG_WARN`，而非 `LOG_ERROR`。
  - 例外：幂等快照写入失败触发 `clearReservation` 兜底（影响重试幂等正确性）的路径仍用 `LOG_ERROR`
    （见 `PaymentService.cc` / `RefundService.cc` 的 `Failed to save idempotency snapshot; clearing reservation`）。
- 生产环境默认 `LOG_INFO` 及以上；`trace`/`debug` 按需动态开启；`fatal` 应极少出现。
- 禁止日志输出敏感信息（密钥、token、PII）。

## 监控告警注意事项

> ⚠️ 本项目曾将部分 fire-and-forget 辅助写入的失败日志由 `LOG_ERROR` 降级为 `LOG_WARN`
> （账本/幂等快照 helper）。若运维基于日志聚合（ELK/Loki 等）配置了按 `LOG_ERROR` 计数的告警，
> 这些失败将不再触发原告警。请同步检查告警规则：
> - 项目内置的 Prometheus 指标告警（`HighErrorRate` 等，见
>   [`docs/deployment/monitoring_setup.md`](../deployment/monitoring_setup.md)）基于 HTTP 指标，
>   不受日志级别影响。
> - 若有基于日志级别的自定义告警，需将「账本/幂等快照写失败」的告警条件从 ERROR 改为 WARN，
>   或为幂等快照失败单独建立指标告警（推荐后者，因其影响重试幂等正确性）。

详见 [`TECH_SPECS.md`](../../TECH_SPECS.md) 与 [`AGENTS.md`](../../AGENTS.md) Critical Constraints。
