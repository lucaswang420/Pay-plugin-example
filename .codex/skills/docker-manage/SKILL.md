---
name: docker-manage
description: Docker Compose 管理 — 通过 docker compose 管理 Pay Plugin 的开发/测试容器环境。
---

# Docker 容器管理

管理 Pay Plugin 的 Docker Compose 容器化开发环境，包括 PostgreSQL、Redis 和 PayServer 服务。

## 前置要求

- Docker Engine 已安装并运行
- `docker compose` CLI（Windows/macOS 内置，Linux 需单独安装插件）
- 可选配置文件：`examples/pay-server/.env`（若不提供，需设置环境变量 `PAY_DB_PASSWORD`、`PAY_API_KEY`）

## 当前栈（`examples/pay-server/docker-compose.yml`）

核心服务：

| 服务 | 镜像 | 容器名 | 端口 | 健康检查 |
|------|------|--------|------|----------|
| `postgres` | postgres:15-alpine | `pay_postgres` | 5432 | `pg_isready -U postgres` |
| `redis` | redis:7-alpine | `pay_redis` | 6379 | `redis-cli ping` |
| `payserver` | 本地构建（Dockerfile） | `pay_server` | 5566 | `curl /healthz` |

可选监控（profile: `monitoring`）：

| 服务 | 镜像 | 容器名 | 端口 |
|------|------|--------|------|
| `prometheus` | prom/prometheus:latest | `pay_prometheus` | 9090 |
| `grafana` | grafana/grafana:latest | `pay_grafana` | 3000 |

## 常用命令

### 启动/停止核心服务

```powershell
cd examples/pay-server

# 设置必需的环境变量（或写入 .env 文件）
$env:PAY_DB_PASSWORD = "postgres"
$env:PAY_API_KEY = "test-dev-key"

# 启动所有核心服务
docker-compose up -d

# 验证所有容器正常运行
docker-compose ps

# 停止所有核心服务
docker-compose down

# 停止并清理数据卷
docker-compose down -v
```

### 带监控启动

```powershell
# 启动核心 + 监控栈
docker-compose --profile monitoring up -d

# 仅启动监控
docker-compose --profile monitoring up -d prometheus grafana
```

### 查看日志

```powershell
# 查看所有服务日志（实时跟踪）
docker-compose logs -f

# 仅查看 payserver
docker-compose logs -f payserver

# 仅查看 postgres
docker-compose logs -f postgres
```

### 进入容器

```powershell
# 进入 payserver 容器
docker-compose exec payserver bash

# PostgreSQL CLI
docker-compose exec postgres psql -U postgres -d pay_test

# Redis CLI
docker-compose exec redis redis-cli
```

### 诊断命令

```powershell
# 检查端口占用
netstat -ano | findstr "5566 5432 6379"

# 检查 payserver 健康状态
curl -f http://localhost:5566/healthz

# 检查 PostgreSQL 连接
docker-compose exec postgres psql -U postgres -d pay_test -c "SELECT 1;"

# 检查 Redis 连接
docker-compose exec redis redis-cli ping

# 查看容器资源使用
docker stats --filter "name=pay_"
```

### 重建服务

```powershell
# 仅重建 payserver（代码变更后）
docker-compose up -d --build payserver

# 完全重建所有服务
docker-compose up -d --build --force-recreate
```

## 环境变量

docker-compose.yml 需要以下环境变量（`:?` 语法表示必须提供）：

| 变量 | 必填 | 默认值 | 说明 |
|------|------|--------|------|
| `PAY_DB_PASSWORD` | ✅ | — | PostgreSQL 密码 |
| `PAY_REDIS_PASSWORD` | ❌ | 空 | Redis 密码（默认无密码） |
| `PAY_API_KEY` | ✅ | — | API 认证密钥 |

设置方式：
```powershell
# 方式 1：环境变量（当前会话）
$env:PAY_DB_PASSWORD = "postgres"
$env:PAY_API_KEY = "your-strong-key-here"

# 方式 2：.env 文件（推荐用于本地开发）
# 在 examples/pay-server/ 下创建 .env 文件（已在 .gitignore 中）
```

## 常见问题

| 症状 | 诊断 | 解决方案 |
|------|------|----------|
| `PAY_DB_PASSWORD was not set` | 缺少环境变量 | 设置 `$env:PAY_DB_PASSWORD = "postgres"` |
| payserver 反复重启 | 查看日志 | `docker-compose logs payserver \| Select-Object -Last 50` |
| 端口 5566 被占用 | 已有进程 | `netstat -ano \| findstr 5566` → `taskkill /F /PID <pid>` |
| PostgreSQL 连接被拒绝 | DB 未就绪 | 等待 `pay_postgres` 状态变为 healthy |
| healthcheck 失败 | 服务未完全启动 | 检查 `start_period`（10s）已过 |

## 与 docker-integration-test 对比

| | docker-manage | docker-integration-test |
|--|---------------|------------------------|
| 目的 | 容器生命周期管理 | E2E 自动化测试 |
| 何时用 | 开发调试 | 提交前验证 |
| 交互方式 | 手动命令 | 自动脚本 |
| 输出 | CLI 输出 | HTML 报告 |
