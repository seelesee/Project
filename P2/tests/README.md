# 单元测试教程 — DHT22 驱动

> 🎯 **目标**: 学会为嵌入式 C 代码编写可在 PC 上运行的单元测试

---

## 为什么要写测试？

```
┌─────────────────────────────────────────────────────────┐
│  没有测试:                                               │
│    改一行代码 → 烧录到STM32 → 接硬件 → 观察 → 可能烧坏   │
│    ⏱️ 每次验证 5~10 分钟                                 │
│                                                         │
│  有测试:                                                 │
│    改一行代码 → 敲 make → 0.1 秒出结果                    │
│    ⏱️ 每次验证 < 1 秒                                    │
└─────────────────────────────────────────────────────────┘
```

**测试保护你**:
- ✅ 改了 A 模块，不会不小心搞坏 B 模块（回归测试）
- ✅ 新人接手代码，跑测试就知道哪里会坏
- ✅ 重构代码时，测试告诉你"有没有改变行为"

---

## 核心概念: 嵌入式代码如何在 PC 上测试？

嵌入式代码最大的问题是: 它依赖硬件 (GPIO、定时器、中断)。

**解法: Mock (模拟)**

```
真实运行:                    测试运行:
┌──────────┐                ┌──────────┐
│  dht22.c │                │  dht22.c │         (被测代码, 不变!)
├──────────┤                ├──────────┤
│ HAL库    │  ← 替换为 →   │ mock_hal.h│        (假的硬件层)
├──────────┤                ├──────────┤
│ STM32芯片 │               │  PC/GCC  │         (运行平台)
└──────────┘                └──────────┘
```

Mock 的核心思想很简单:
> 把 GPIO 电平存在一个全局变量里，测试想让它返回 0 就设 0，想返回 1 就设 1。

```c
// 真实 HAL (操作硬件寄存器)
uint8_t HAL_GPIO_ReadPin(...) {
    return (GPIOA->IDR & GPIO_PIN_0) ? 1 : 0;  // 读真实的STM32寄存器
}

// Mock HAL (读全局变量)
uint8_t HAL_GPIO_ReadPin(...) {
    return mock_gpio_pin_value[0];  // 读测试预设的值
}
```

---

## 文件结构

```
tests/
├── unity.h              ← 测试框架 (断言宏 + 结果统计)
├── mock_hal.h           ← Mock HAL 层 (替代 stm32f1xx_hal.h)
├── test_dht22.c         ← DHT22 测试用例 (⭐核心文件)
├── Makefile             ← Linux/MinGW 编译
└── build.bat            ← Windows 编译脚本
```

---

## 快速开始

### 1. 安装编译器 (选一种)

**方案A: MinGW-w64 (推荐)**
1. 下载 https://github.com/brechtsanders/winlibs_mingw/releases
2. 解压到 `C:\mingw64`
3. 把 `C:\mingw64\bin` 加入系统环境变量 PATH

**方案B: MSYS2**
```bash
pacman -S mingw-w64-x86_64-gcc
```

### 2. 编译运行

```bash
# 方法1: 双击运行
双击 build.bat

# 方法2: Make
make

# 方法3: 手动编译
gcc -o test_runner.exe test_dht22.c -I. -lm
./test_runner.exe
```

### 3. 预期输出

```
╔══════════════════════════════════════════════╗
║  DHT22 驱动单元测试                          ║
║  STM32 智能温湿度监测系统                     ║
╚══════════════════════════════════════════════╝

━━━ 测试组1: 校验和验证 ━━━
  [测试1.1] 正确数据的校验和应该通过
.  [TEST] 期望=1, 实际=1 ... PASSED
  [测试1.2] 错误校验和应该被检测出来
.  [TEST] 期望=0, 实际=0 ... PASSED

...（更多测试）...

============================================
  测试结果汇总
============================================
  总计: 18
  通过: 18  ✅
  失败: 0
============================================
  🎉 全部测试通过!
```

---

## 测试用例详解

### 测试组1: 校验和验证 (纯逻辑，无需硬件)

```c
// 被测代码:
static uint8_t DHT22_ValidateChecksum(uint8_t data[5]) {
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    return (checksum == data[4]) ? 1 : 0;
}

// 测试1.1: 正确的校验和
void test_checksum_valid(void) {
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0x22};
    // 0x02+0x26+0x00+0xFA = 0x0122, 低8位=0x22 ✓
    TEST_ASSERT_EQUAL(1, DHT22_ValidateChecksum(data));
}

// 测试1.2: 错误的校验和
void test_checksum_invalid(void) {
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0xFF};  // FF≠0x22
    TEST_ASSERT_EQUAL(0, DHT22_ValidateChecksum(data));
}
```

**知识点**: 把"纯逻辑"从硬件代码中提取出来，单独测试。这不需要任何硬件！

---

### 测试组2: 数据解析 (边界值 + 等价类)

```c
// 测试: 正常值、零值、最大值、负温度
test_parse_normal_values();      // 25.0°C, 55.0%RH
test_parse_zero_values();        // 0°C, 0%RH
test_parse_max_values();         // 80°C, 100%RH
test_parse_negative_temperature(); // -10.0°C (符号位!)
test_parse_extreme_negative();   // -40.0°C (极值)
```

**知识点**: 专业的测试要覆盖:
- **边界值**: 0, 最大值, 最小值
- **等价类**: 正常范围 / 负温度范围 / 超范围
- **特殊情况**: 符号位、溢出

---

### 测试组4: Mock 完整通信

```c
// 模拟传感器返回 28.5°C / 62.3%RH
Mock_DHT22_SetResponse(0x02, 0x6F,   // 湿度 623 → 62.3%
                       0x01, 0x1D);  // 温度 285 → 28.5°C

float temp, humi;
uint8_t result = DHT22_Read_Mockable(&temp, &humi);

TEST_ASSERT_EQUAL(1, result);
TEST_ASSERT_FLOAT_WITHIN(0.1f, 62.3f, humi);
TEST_ASSERT_FLOAT_WITHIN(0.1f, 28.5f, temp);
```

**知识点**: Mock 让被测代码以为在和真实传感器通信，但其实所有数据都是测试预设的。

---

### 测试组6: 错误注入 (健壮性测试)

```c
// 翻转数据中的每一个 bit，验证校验和能检测到
for (int bit = 0; bit < 8; bit++) {
    corrupted[0] ^= (1 << bit);  // 故意破坏1个bit
    TEST_ASSERT_EQUAL(0, DHT22_ParseData(corrupted, &temp, &humi));
}
```

**知识点**: 模拟"传输干扰"——数据在传输过程中某一位翻转了，校验和必须能发现。

---

## 如何为你的下一个模块写测试？

```
第1步: 找到"纯逻辑"
  ├─ 不依赖 GPIO/定时器/中断 的计算代码
  ├─ 数据校验 (校验和/CRC/魔数)
  ├─ 数据格式转换 (raw → 实际值)
  └─ 状态机逻辑 (如果能把状态抽象出来)

第2步: 提取成独立函数
  ├─ 把逻辑从 HAL_xxx 调用中分离
  └─ 输入输出都通过参数传递 (不依赖全局硬件状态)

第3步: 设计测试数据
  ├─ 正常情况: 1组
  ├─ 边界值: 上下限各1组
  ├─ 异常情况: 错误输入/超时/溢出
  └─ 等价类: 每种类型取1个代表

第4步: 写断言
  ├─ TEST_ASSERT_EQUAL     → 整数比较
  ├─ TEST_ASSERT_FLOAT_WITHIN → 浮点数 (允许误差)
  └─ TEST_ASSERT_TRUE      → 布尔条件

第5步: 编译运行 → 看绿灯 ✅
```

---

## 下一步可以测什么？

| 模块 | 可测试的纯逻辑 | 测试难度 |
|------|---------------|---------|
| **Protocol** | 帧解析状态机、校验和计算、命令分发 | ⭐⭐ |
| **Config** | CRC16 校验、魔数验证、默认值加载 | ⭐⭐ |
| **Buzzer** | 状态机转换 (CONT→PATTERN→ONESHOT)、定时计算 | ⭐⭐ |
| **Button** | 消抖状态机、长按计时、事件队列 | ⭐⭐⭐ |
| **OLED** | 字体取模偏移、显存坐标计算 | ⭐ |

---

## 常见问题

**Q: 为什么要用 Unity 而不是 Google Test / CppUTest?**
A: Unity 只有1个头文件，零依赖，特别适合嵌入式项目。Google Test 需要 C++，CppUTest 更适合大型项目。

**Q: Mock 和真实硬件行为不一样怎么办?**
A: Mock 测试验证的是**逻辑正确性**。硬件时序还需要在 Proteus/真机上验证。两者互补，不是替代关系。

**Q: 测试覆盖率多少才算够?**
A: 对于嵌入式项目，核心模块的纯逻辑部分 ≥ 80% 就很好了。硬件相关代码不需要 100% 覆盖。
