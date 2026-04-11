# TDD 集成测试实施计划

**日期：** 2026-04-11
**状态：** 选项C执行中 - 折中方案
**阶段：** 步骤3 - 用TDD更新集成测试

---

## 已完成的工作

### ✅ 步骤1：删除无价值的单元测试
- 删除 `test/service/PaymentServiceTest.cc`
- 删除 `test/service/RefundServiceTest.cc`
- 删除 `test/service/CallbackServiceTest.cc`
- 删除 `test/service/MockHelpers.h`

### ✅ 步骤2：添加技术债务标记
在所有服务头文件中添加了TDD违规通知：
- `services/PaymentService.h`
- `services/RefundService.h`
- `services/CallbackService.h`
- `services/IdempotencyService.h`
- `services/ReconciliationService.h`

### 🔄 步骤3：用TDD更新集成测试（进行中）

---

## TDD测试文件创建

### 新创建的TDD测试文件

**文件：** `test/TDD_QueryOrderTest.cc`

**包含3个测试用例：**
1. `QueryExistingOrder_ReturnsOrderDetails` - 查询存在的订单
2. `QueryNonExistentOrder_ReturnsNotFoundError` - 查询不存在的订单
3. `QueryWithEmptyOrderNo_ReturnsValidationError` - 空订单号验证

**TDD原则遵循：**
- ✅ RED - 测试已写，预期会失败（actual["code"] = -1）
- ⏳ Verify RED - 需要运行测试确认失败
- ⏳ GREEN - 需要实现代码让测试通过
- ⏳ Verify GREEN - 需要验证测试通过
- ⏳ REFACTOR - 需要清理代码

---

## 当前TDD状态

### RED阶段 - ✅ 完成

**测试已编写：**
```cpp
TEST(TDD_QueryOrder, QueryExistingOrder_ReturnsOrderDetails) {
    // Arrange, Act, Assert
    // Test will FAIL because actual["code"] = -1
}
```

**预期失败原因：**
- 功能尚未实现（actual["code"] = -1是占位符）
- 这是正确的TDD失败：功能缺失，而非拼写错误

### Verify RED阶段 - ⏳ 待执行

**需要的行动：**
1. 设置测试环境
2. 编译测试文件
3. 运行测试
4. 确认测试因功能缺失而失败

**预期结果：**
```
[ RUN      ] TDD_QueryOrder.QueryExistingOrder_ReturnsOrderDetails
test/TDD_QueryOrderTest.cc:52: Failure
Expected: 0 (expected["code"])
Actual: -1 (actual["code"])
Error code should be 0 for success
[  FAILED  ] TDD_QueryOrder.QueryExistingOrder_ReturnsOrderDetails
```

### GREEN阶段 - ⏳ 待执行

**需要的行动：**
1. 实现查询订单功能
2. 编写最少代码让测试通过
3. 不要添加额外功能

**示例实现：**
```cpp
// 在Controller或Service中实现
Json::Value queryOrder(const std::string& orderNo) {
    // 查询数据库
    // 返回订单信息
}
```

### Verify GREEN阶段 - ⏳ 待执行

**验证清单：**
- [ ] 该测试通过
- [ ] 其他测试仍然通过
- [ ] 输出干净（无错误、警告）

### REFACTOR阶段 - ⏳ 待执行

**可能的清理：**
- 提取重复代码
- 改进命名
- 简化逻辑

---

## 下一步行动

### 立即需要

1. **设置测试环境**
   - 检查CMakeLists.txt是否包含新测试文件
   - 确保Google Test框架可用
   - 准备测试数据库

2. **编译并运行测试**
   ```bash
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   make
   ./PayBackendTests --gtest_filter=TDD_QueryOrder*
   ```

3. **确认RED状态**
   - 查看测试失败输出
   - 确认失败原因正确
   - 记录失败信息

### 短期内（今天）

4. **实现GREEN**
   - 修改Controller或Service代码
   - 实现查询订单功能
   - 让测试通过

5. **完成一个完整的TDD循环**
   - RED → Verify RED → GREEN → Verify GREEN → REFACTOR
   - 验证TDD流程的价值

### 长期内（本周）

6. **添加更多TDD测试**
   - 创建订单测试
   - 支付回调测试
   - 退款测试

7. **建立TDD工作流**
   - 文档化TDD流程
   - 培训团队成员
   - 建立CI集成

---

## TDD价值验证

这个新测试文件的价值：

✅ **测试先行**
- 测试在实现前编写
- 定义了期望的API和结果
- 测试会失败（功能缺失）

✅ **明确的行为规范**
- 测试名称描述了行为
- 断言清晰明确
- 包含正常和异常情况

✅ **可维护性**
- 独立的测试文件
- 不依赖旧代码结构
- 可以逐步扩展

---

## 技术债务记录

### 当前债务状态

| 服务 | 单元测试 | 集成测试 | 技术债务级别 |
|------|---------|---------|-------------|
| PaymentService | ❌ 无 | 🔄 TDD进行中 | 中 |
| RefundService | ❌ 无 | ⏳ 待开始 | 中 |
| CallbackService | ❌ 无 | ⏳ 待开始 | 中 |
| IdempotencyService | ❌ 无 | ⏳ 待开始 | 中 |
| ReconciliationService | ❌ 无 | ⏳ 待开始 | 中 |

### 债务减少计划

**本周目标：**
- 完成1个TDD集成测试（查询订单）
- 验证TDD流程价值
- 建立TDD模板

**下周目标：**
- 添加2-3个TDD集成测试
- 覆盖关键业务流程
- 建立CI自动化

**长期目标：**
- 所有集成测试用TDD重写
- 添加单元测试（TDD）
- 100%测试覆盖率

---

## 成功标准

### 最低标准（可接受）

- [ ] 新TDD测试能编译
- [ ] 新TDD测试失败（RED）
- [ ] 实现代码让测试通过（GREEN）
- [ ] 测试保持通过（REFACTOR）
- [ ] 完成一个完整TDD循环

### 理想标准（TDD合规）

- [ ] 所有关键流程有TDD测试
- [ ] 每个测试都遵循RED-GREEN-REFACTOR
- [ ] 测试文档化TDD过程
- [ ] 团队成员理解并使用TDD

---

**文档创建时间：** 2026-04-11
**下次更新：** 完成第一个TDD循环后
