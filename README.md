# Pay Plugin Example

基于 [Drogon](https://github.com/drogonframework/drogon) 框架开发的支付插件后端参考实现（PayBackend），主要用于演示在 C++ Drogon 环境下如何整合常用的第三方支付平台（如微信支付等）。

## 项目结构

- **PayBackend/**：核心的后端应用目录。
  - `controllers/`：HTTP 请求/响应路由层。
  - `plugins/`：支付核心业务逻辑层的独立插件。
  - `models/`：数据库 ORM 映射模型，基于 Drogon ORM 自动生成。
  - `docs/`：项目相关文档及 API 说明。
  - `test/`：包括单元测试与集成测试代码。
- **.agent/workflows/** 或 **.codex/workflows/**：项目中定义好的自动化工作流（如 `build`, `start`, `test` 等）。

## 构建与运行

系统内置了一系列自动化工作流以简化日常开发流程。

### 基本指令

- **构建项目**：可以通过预设的构建流程来编译 CMake 项目。
- **启动服务**：通过启动脚本，一键拉起后端与相关服务。
- **停止服务**：停止相关的后端应用进程。
- **数据库及ORM**：使用附带的 `init_pay.sql` 初始化数据库，使用相关工作流重新生成模型。
- **运行测试**：通过内置的测试工作流运行全局或模块单元测试（例如：`WechatPayClientTest`）。

> **注意：** 该项目严格遵循后端分层架构规范，并以异步和基于回调（与 `shared_ptr` 管理声明周期）的方式处理网络与业务协同请求。

## 开源协议

本项目采用 [MIT License](LICENSE) 协议开源，请自由使用和修改，详情请参阅 `LICENSE` 文件。
