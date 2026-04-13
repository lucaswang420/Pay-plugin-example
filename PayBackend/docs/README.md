# Pay Plugin 文档中心

**版本：** v1.0.0 | **更新时间：** 2026-04-13

---

## 📚 文档导航

### 🚀 快速开始

新手必读文档，按顺序阅读：

1. [项目 README](../../README.md) - 项目概述和快速开始
2. [部署指南](deployment_guide.md) - 完整的部署步骤
3. [API配置指南](api_configuration_guide.md) - API Key 配置

---

## 📖 核心文档

### 部署和运维

- **[部署指南](deployment_guide.md)**
  - 系统要求和依赖
  - Windows/Linux 安装步骤
  - 配置文件说明
  - 部署验证
  - 故障排查

- **[运维手册](operations_manual.md)**
  - 日常操作（启动/停止/重启）
  - 配置更新流程
  - 日志查看和分析
  - 故障诊断和处理
  - 备份和恢复
  - 扩容和缩容

- **[监控配置](monitoring_setup.md)**
  - Prometheus 配置
  - Grafana 仪表板
  - 告警规则配置
  - 日志聚合
  - 监控最佳实践

### 安全和质量

- **[安全检查清单](security_checklist.md)**
  - API 密钥管理
  - 数据保护
  - 认证和授权
  - 输入验证
  - 安全头配置
  - 依赖安全
  - 日志和审计

- **[E2E 测试指南](e2e_testing_guide.md)**
  - 端到端测试设置
  - 测试场景
  - 执行和验证

---

## 🏗️ 架构和设计

- **[项目状态总结](current_status_2026_04_13.md)**
  - 项目进度
  - 已完成工作
  - 待完成任务
  - 关键指标

- **[生产就绪路线图](production_readiness_roadmap.md)**
  - 方案选择
  - 实施计划
  - 时间估算
  - 成功标准

- **[发布说明 v1.0.0](release_notes_v1.0.md)**
  - 新功能
  - 性能指标
  - 测试覆盖率
  - 迁移指南
  - 已知限制

---

## 🔧 技术文档

### 架构和配置

- **[架构概览](architecture_overview.md)** - 系统架构设计
- **[配置指南](configuration_guide.md)** - 应用配置说明
- **[迁移指南](migration_guide.md)** - API 迁移指南
- **[测试指南](testing_guide.md)** - 测试框架使用
- **[故障排查](troubleshooting.md)** - 常见问题解决

### 性能和优化

- **[性能测试报告](performance_test_final_report.md)**
  - 性能基准测试
  - 优化措施
  - 测试结果

- **[性能优化完成报告](performance_optimization_complete.md)**
  - 配置优化
  - 验证结果

### API 参考

- **[支付 API 示例](pay-api-examples.md)**
  - 创建支付
  - 查询订单
  - 创建退款
  - 查询退款

---

## 📊 项目报告

### 阶段性报告

- **[项目完成总结](project_completion_summary.md)**
  - 最终成就统计
  - 时间投入分析
  - 成功标准达成
  - 经验教训

### 阶段报告（归档）

以下报告已归档到 [archive/](archive/) 目录：

- Phase 1 完成报告
- Phase 2 性能测试报告
- TDD 执行报告
- 重构总结

---

## 🗂️ 归档文档

### TDD 报告 (archive/tdd_reports/)

历史 TDD 开发过程中的报告：

- tdd_cycle_execution_report.md
- tdd_execution_report.md
- tdd_integration_test_plan.md
- tdd_option_c_completion_report.md
- tdd_queryrefund_tests_complete.md
- tdd_red_state_verification.md
- tdd_refundquery_update_report.md
- tdd_tests_update_final_report.md
- tdd_validation_plan.md

### 遗留文档 (archive/legacy_docs/)

早期的项目文档和报告：

- REFACTORING_SUMMARY.md
- validation_report.md
- compilation_validation_report.md
- correct_build_method_verification.md
- api_config_complete.md
- optimization_verification_report.md
- phase1_completion_summary.md
- phase2_performance_test_report.md

### 备份文件 (archive/)

测试文件的备份：

- RefundQueryTest.cc.backup
- RefundQueryTest.cc.backup2
- WechatCallbackIntegrationTest.cc.before_migration
- test_404_fix.sh

---

## 🔗 快速链接

### 常用命令

```bash
# 构建项目
cd PayBackend
scripts\build.bat  # Windows

# 运行测试
./build/Release/test_payplugin.exe

# 健康检查
curl http://localhost:5566/health

# 查看日志
./deploy/ops/view_logs.sh -f
```

### 配置文件

- `PayBackend/config.json` - 应用配置
- `deploy/prometheus_config.yml` - Prometheus 配置
- `deploy/alerts.yml` - 告警规则
- `deploy/security_headers_config.json` - 安全头配置

### 脚本工具

- `deploy/ops/backup_db.sh` - 数据库备份
- `deploy/ops/restore_db.sh` - 数据库恢复
- `deploy/ops/restart_service.sh` - 服务重启
- `deploy/ops/view_logs.sh` - 日志查看

---

## 📝 文档维护

### 文档分类

- **生产文档** - 部署、运维、安全、监控
- **开发文档** - 架构、API、测试
- **报告文档** - 项目进度、性能报告
- **归档文档** - 历史记录、备份文件

### 更新频率

- **生产文档** - 每次发布时更新
- **开发文档** - 功能变更时更新
- **报告文档** - 阶段完成时归档

---

## 🎯 推荐阅读路径

### 新用户

1. [项目 README](../../README.md) → 了解项目
2. [部署指南](deployment_guide.md) → 部署系统
3. [API配置指南](api_configuration_guide.md) → 配置 API
4. [E2E 测试指南](e2e_testing_guide.md) → 验证功能

### 运维人员

1. [部署指南](deployment_guide.md) → 部署系统
2. [运维手册](operations_manual.md) → 日常运维
3. [监控配置](monitoring_setup.md) → 配置监控
4. [安全检查清单](security_checklist.md) → 安全加固

### 开发人员

1. [架构概览](architecture_overview.md) → 了解架构
2. [迁移指南](migration_guide.md) → API 迁移
3. [测试指南](testing_guide.md) → 编写测试
4. [支付 API 示例](pay-api-examples.md) → API 使用

---

**文档版本：** v1.0.0
**最后更新：** 2026-04-13
**维护者：** Pay Plugin Team
