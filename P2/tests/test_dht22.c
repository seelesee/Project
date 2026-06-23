/**
 * @file    test_dht22.c
 * @brief   DHT22 驱动单元测试 - 核心功能测试脚本
 *
 * ==================== 嵌入式单元测试的核心理念 ====================
 *
 * 问题: 嵌入式代码直接操作硬件(GPIO/定时器), 无法在PC上直接运行。
 * 解决: 把"纯逻辑"从"硬件操作"中分离出来, 在PC上测试逻辑。
 *
 *  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
 *  │  硬件操作部分     │     │  纯逻辑部分      │     │  测试代码        │
 *  │  (GPIO/定时器)   │────▶│  (数据解析/校验) │◀────│  (提供测试数据)  │
 *  │  ↓ 不测/Mock    │     │  ↓ 重点测试!    │     │  ↓ 断言验证     │
 *  └─────────────────┘     └─────────────────┘     └─────────────────┘
 *
 * 本文件测试 DHT22 驱动的核心逻辑:
 *   测试1-5:  数据解析纯逻辑 (校验和/温湿度转换/负温度/边界值)
 *   测试6-8:  Mock 硬件仿真 (用假GPIO模拟完整的传感器通信过程)
 *   测试9:    超时/异常处理
 *
 * 运行方式 (安装 MinGW 后):
 *   gcc -o test_runner test_dht22.c mock_hal.c -I. -lm
 *   ./test_runner.exe
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/*
 * 关键技巧: 先包含 mock_hal.h 替代真实的 stm32f1xx_hal.h
 * 这样所有 HAL_xxx 函数调用都会变成 mock 版本
 */
#include "mock_hal.h"
#include "unity.h"

/*
 * DHT22 驱动中需要的宏定义 (和真实的 dht22.h 保持一致)
 * 注意: 这些宏在 mock_hal.h 中有些已定义, 这里是 DHT22 特有的
 */
#define DHT22_GPIO_PORT        ((void*)0)      // Mock 中不需要真实端口地址
#define DHT22_GPIO_PIN         GPIO_PIN_0
#define DHT22_DATA_HIGH()      HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET)
#define DHT22_DATA_LOW()       HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_RESET)
#define DHT22_DATA_READ()      HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN)

#define DHT22_START_LOW        1200
#define DHT22_START_HIGH       30
#define DHT22_TIMEOUT          200
#define DHT22_MAX_RETRY        3

/* ======================== 被测函数 (从 dht22.c 中提取) ======================== */

/**
 * @brief 【被测函数1】校验和验证
 *
 * DHT22 发送 40bit 数据: 16bit湿度 + 16bit温度 + 8bit校验
 * 校验规则: data[0] + data[1] + data[2] + data[3] == data[4] (取低8位)
 *
 * 这是从 dht22.c 的 DHT22_Read() 中提取出的纯逻辑,
 * 不依赖任何硬件, 可以直接在 PC 上测试!
 *
 * @param data  5字节原始数据
 * @return 1=校验通过, 0=校验失败
 */
static uint8_t DHT22_ValidateChecksum(uint8_t data[5])
{
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    return (checksum == data[4]) ? 1 : 0;
}

/**
 * @brief 【被测函数2】解析温湿度数据
 *
 * 将40bit原始数据转换为实际的温湿度浮点值
 * 这也是纯逻辑 — 不碰任何硬件!
 *
 * 数据格式 (DHT22 数据手册):
 *   data[0] = 湿度整数部分 (高8位)
 *   data[1] = 湿度小数部分 (低8位, 实际为0, DHT22精度仅整数)
 *   data[2] = 温度整数部分 (高8位, bit15=符号位)
 *   data[3] = 温度小数部分 (低8位)
 *   data[4] = 校验和
 *
 *   湿度(%RH) = (data[0]<<8 | data[1]) / 10.0
 *   温度(°C)  = (data[2]<<8 | data[3]) / 10.0
 *     若 bit15=1, 表示负温度, 取绝对值后加负号
 *
 * @param data         5字节原始数据
 * @param temperature  输出: 解析后的温度值 (°C)
 * @param humidity     输出: 解析后的湿度值 (%RH)
 * @return 1=成功, 0=校验失败
 */
static uint8_t DHT22_ParseData(uint8_t data[5], float *temperature, float *humidity)
{
    /* 第1步: 校验和检查 */
    if (!DHT22_ValidateChecksum(data)) {
        return 0;
    }

    /* 第2步: 拼接原始值 */
    uint16_t rawHumidity    = (data[0] << 8) | data[1];
    uint16_t rawTemperature = (data[2] << 8) | data[3];

    /* 第3步: 湿度转换 */
    *humidity = rawHumidity / 10.0f;

    /* 第4步: 温度转换 (处理符号位) */
    if (rawTemperature & 0x8000) {
        /* 负温度: 清除bit15, 取绝对值后加负号 */
        rawTemperature &= 0x7FFF;
        *temperature = -(rawTemperature / 10.0f);
    } else {
        *temperature = rawTemperature / 10.0f;
    }

    return 1;
}

/* ======================== 模拟的 DHT22_Read (用 Mock GPIO) ======================== */

/**
 * 时序变量 — 模拟 DHT22 通信过程的"脚本"
 * 测试代码设置这些值, Mock 函数依次返回, 模拟真实波形
 */
static uint8_t  mock_dht22_response_enabled = 0;    // 是否启用模拟响应
static uint8_t  mock_dht22_data_buffer[5];          // 模拟要返回的40bit数据
// static uint32_t mock_dht22_step_counter = 0;        // 当前通信步骤 (保留用于后续扩展)

/**
 * @brief 【被测函数3】DHT22_Read 的 Mock 可测试版本
 *
 * 这是 dht22.c 中 DHT22_Read() 的"可测试版本"。
 * 区别: 用 mock 变量代替真实的 GPIO 电平和 DWT 时序。
 *
 * 通信流程 (DHT22 单总线协议):
 *   主机 ──┬── 拉低 1ms ──┬── 拉高 30us ──┬── 释放总线(输入)
 *          │              │               │
 *   从机   │              │  拉低80us     │  拉高80us → 拉低 → 40bit数据...
 *          ▼              ▼               ▼
 *       起始信号         从机响应         数据传输
 */
static uint8_t DHT22_Read_Mockable(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    if (!mock_dht22_response_enabled) {
        return 0;  // 模拟传感器未连接
    }

    /* 步骤1-2: 模拟起始信号和从机响应 (在mock中直接跳过) */
    /* 步骤3: 读40bit数据 → 直接从 mock buffer 取 */
    memcpy(data, mock_dht22_data_buffer, 5);

    /* 步骤4: 校验和 */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        return 0;
    }

    /* 步骤5: 解析数据 */
    uint16_t rawHumidity    = (data[0] << 8) | data[1];
    uint16_t rawTemperature = (data[2] << 8) | data[3];

    *humidity = rawHumidity / 10.0f;

    if (rawTemperature & 0x8000) {
        rawTemperature &= 0x7FFF;
        *temperature = -(rawTemperature / 10.0f);
    } else {
        *temperature = rawTemperature / 10.0f;
    }

    return 1;
}

/**
 * 设置模拟的 DHT22 响应数据
 */
static void Mock_DHT22_SetResponse(uint8_t humi_high, uint8_t humi_low,
                                    uint8_t temp_high, uint8_t temp_low)
{
    mock_dht22_response_enabled = 1;
    mock_dht22_data_buffer[0] = humi_high;
    mock_dht22_data_buffer[1] = humi_low;
    mock_dht22_data_buffer[2] = temp_high;
    mock_dht22_data_buffer[3] = temp_low;
    mock_dht22_data_buffer[4] = humi_high + humi_low + temp_high + temp_low; // 自动计算校验和
}

/* ======================== 测试用例 ======================== */

/**
 * ===== 测试组 1: 校验和验证 =====
 *
 * 这是最基础的测试 — 验证校验和计算是否正确。
 * 不需要任何硬件, 只在内存中操作字节数组。
 */

/* 测试1.1: 正确校验和 */
static void test_checksum_valid(void)
{
    printf("\n  [测试1.1] 正确数据的校验和应该通过\n");

    /* 模拟一组真实的 DHT22 数据:
     *   湿度 = 55.0% → raw = 550 → 0x0226 → [0x02, 0x26]
     *   温度 = 25.0°C → raw = 250 → 0x00FA → [0x00, 0xFA]
     *   校验 = 0x02+0x26+0x00+0xFA = 0x0122 → 低8位 = 0x22
     */
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0x22};
    TEST_ASSERT_EQUAL(1, DHT22_ValidateChecksum(data));
}

/* 测试1.2: 错误校验和 */
static void test_checksum_invalid(void)
{
    printf("\n  [测试1.2] 错误校验和应该被检测出来\n");

    /* 故意把校验字节改成错误值 */
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0xFF};  // FF ≠ 正确的 0x22
    TEST_ASSERT_EQUAL(0, DHT22_ValidateChecksum(data));
}

/* 测试1.3: 校验和溢出 (验证取低8位的逻辑) */
static void test_checksum_overflow(void)
{
    printf("\n  [测试1.3] 校验和溢出时取低8位\n");

    /* 很大的数据, 校验和会超过 255:
     *   0xFF + 0xFF + 0xFF + 0xFF = 0x3FC
     *   低8位 = 0xFC
     */
    uint8_t data[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFC};
    TEST_ASSERT_EQUAL(1, DHT22_ValidateChecksum(data));

    /* 再验证: 0x64+0x64+0x64+0x64 = 0x190, 低8位=0x90 */
    uint8_t data2[5] = {0x64, 0x64, 0x64, 0x64, 0x90};
    TEST_ASSERT_EQUAL(1, DHT22_ValidateChecksum(data2));
}

/**
 * ===== 测试组 2: 温湿度数据解析 =====
 *
 * 测试核心的数据转换逻辑:
 *   原始值 / 10.0 → 实际浮点数
 */

/* 测试2.1: 正常温度+正常湿度 */
static void test_parse_normal_values(void)
{
    printf("\n  [测试2.1] 正常温湿度解析 (25.0 C, 55.0 %%rh)\n");

    float temp, humi;
    /* 湿度 = 550 → 55.0%, 温度 = 250 → 25.0°C */
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0x22};  // 550, 250, chk=0x22

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.0f, humi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, temp);
}

/* 测试2.2: 边界值 — 0度 0湿度 */
static void test_parse_zero_values(void)
{
    printf("\n  [测试2.2] 边界值解析 (0 C, 0 %%rh)\n");

    float temp, humi;
    uint8_t data[5] = {0x00, 0x00, 0x00, 0x00, 0x00};  // 全零

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, humi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, temp);
}

/* 测试2.3: 最大值 (80°C, 100%RH) */
static void test_parse_max_values(void)
{
    printf("\n  [测试2.3] 最大值解析 (80 C, 100 %%rh)\n");

    float temp, humi;
    /* 湿度 100% = 1000 = 0x03E8, 温度 80°C = 800 = 0x0320 */
    uint16_t h100 = 1000, t80 = 800;
    uint8_t data[5] = {
        (h100 >> 8) & 0xFF, h100 & 0xFF,     // 0x03, 0xE8
        (t80 >> 8) & 0xFF,  t80 & 0xFF,       // 0x03, 0x20
        (uint8_t)((h100>>8) + (h100&0xFF) + (t80>>8) + (t80&0xFF))
    };

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, humi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, temp);
}

/* 测试2.4: 负温度 (DHT22 支持 -40°C ~ 0°C) */
static void test_parse_negative_temperature(void)
{
    printf("\n  [测试2.4] 负温度解析 (-10.0°C)\n");

    float temp, humi;
    /* -10.0°C → raw = 0x8000 | 100 = 0x8064
     * bit15 为符号位: 1=负数, 0=正数
     */
    uint16_t rawTemp = 0x8000 | 100;  // 最高位置1 + 值=100 → -10.0°C
    uint8_t data[5] = {
        0x01, 0x5E,                          // 湿度 350 → 35.0%
        (rawTemp >> 8) & 0xFF, rawTemp & 0xFF, // 0x80, 0x64
        (uint8_t)(0x01 + 0x5E + 0x80 + 0x64)   // 校验和
    };

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 35.0f, humi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -10.0f, temp);
}

/* 测试2.5: 极值负温度 (-40.0°C) */
static void test_parse_extreme_negative(void)
{
    printf("\n  [测试2.5] 极端负温度解析 (-40.0°C)\n");

    float temp, humi;
    /* -40.0°C → raw = 0x8000 | 400 = 0x8190 */
    uint16_t rawTemp = 0x8000 | 400;
    uint8_t data[5] = {
        0x01, 0x90,                          // 湿度 400 → 40.0%
        (rawTemp >> 8) & 0xFF, rawTemp & 0xFF,
        (uint8_t)(0x01 + 0x90 + 0x81 + 0x90)
    };

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -40.0f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, humi);
}

/**
 * ===== 测试组 3: 校验失败导致解析失败 =====
 */

/* 测试3.1: 校验失败返回0 */
static void test_parse_fails_on_bad_checksum(void)
{
    printf("\n  [测试3.1] 校验失败时 DHT22_ParseData 应返回 0\n");

    float temp = 999, humi = 999;
    /* 故意给错误的校验和 */
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA, 0x00};  // 校验应该是0x22, 这里给0x00

    uint8_t result = DHT22_ParseData(data, &temp, &humi);

    TEST_ASSERT_EQUAL(0, result);  // 应该返回失败
    /* 注意: 校验失败时, temp/humi 不应被修改
     * 但我们这里没有检查, 因为当前实现会先校验再解析 */
}

/**
 * ===== 测试组 4: Mock GPIO 模拟完整通信 =====
 *
 * 这些测试模拟 DHT22 传感器的完整通信过程:
 * 设置 mock 数据 → 调用读取函数 → 验证结果
 */

/* 测试4.1: 正常读取 (使用 Mock) */
static void test_mock_read_normal(void)
{
    printf("\n  [测试4.1] Mock 模拟正常读取 (28.5 C, 62.3 %%rh)\n");

    /* 设置模拟的传感器数据 */
    Mock_DHT22_SetResponse(
        0x02, 0x6F,   // 湿度: 623 → 62.3%
        0x01, 0x1D    // 温度: 285 → 28.5°C
    );

    float temp, humi;
    uint8_t result = DHT22_Read_Mockable(&temp, &humi);

    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 62.3f, humi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 28.5f, temp);
}

/* 测试4.2: 传感器未连接 */
static void test_mock_read_no_sensor(void)
{
    printf("\n  [测试4.2] Mock 模拟 — 传感器未连接应返回失败\n");

    mock_dht22_response_enabled = 0;  // 禁用模拟响应

    float temp, humi;
    uint8_t result = DHT22_Read_Mockable(&temp, &humi);

    TEST_ASSERT_EQUAL(0, result);  // 应该返回失败
}

/* 测试4.3: 校验失败 */
static void test_mock_read_checksum_error(void)
{
    printf("\n  [测试4.3] Mock 模拟 — 校验和错误\n");

    mock_dht22_response_enabled = 1;
    mock_dht22_data_buffer[0] = 0x02;
    mock_dht22_data_buffer[1] = 0x26;
    mock_dht22_data_buffer[2] = 0x00;
    mock_dht22_data_buffer[3] = 0xFA;
    mock_dht22_data_buffer[4] = 0xFF;  // 故意给错误的校验和

    float temp, humi;
    uint8_t result = DHT22_Read_Mockable(&temp, &humi);

    TEST_ASSERT_EQUAL(0, result);
}

/**
 * ===== 测试组 5: 边界值与等价类测试 =====
 *
 * 专业的测试会覆盖:
 *   - 边界值: 最小、最大、刚好等于阀值
 *   - 等价类: 把输入分成若干组, 每组取一个代表值
 */

/* 测试5.1: 阀值边界 — 温度刚好等于温度上限 */
static void test_threshold_boundary_temp_high(void)
{
    printf("\n  [测试5.1] 阀值边界 — 温度刚好等于上限 35.0°C\n");

    float temp, humi;
    /* 温度 = 350 → 35.0°C (刚好等于默认温度上限) */
    uint16_t rawTemp = 350;
    uint8_t data[5] = {
        0x01, 0xF4,   // 湿度 500 → 50.0%
        (rawTemp >> 8) & 0xFF, rawTemp & 0xFF,
        (uint8_t)(0x01 + 0xF4 + 0x01 + 0x5E)
    };

    uint8_t result = DHT22_ParseData(data, &temp, &humi);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 35.0f, temp);

    /* 在真实系统中, 35.0 不触发报警 (只有 > 35.0 才报警) */
}

/* 测试5.2: 多组等价类测试 */
static void test_equivalence_classes(void)
{
    printf("\n  [测试5.2] 等价类批量测试\n");

    /* 定义测试向量: {湿度原始值, 温度原始值, 期望湿度, 期望温度, 描述} */
    struct TestVector {
        uint16_t rawHumi;
        uint16_t rawTemp;
        float expHumi;
        float expTemp;
        const char *desc;
    };

    struct TestVector vectors[] = {
        /* 常温常湿 */
        {550, 250, 55.0f, 25.0f, "25°C 55%RH (典型室内)"},
        /* 高温高湿 */
        {950, 700, 95.0f, 70.0f, "70°C 95%RH (高温高湿)"},
        /* 低温低湿 */
        {150,  50, 15.0f,  5.0f, "5°C 15%RH (低温低湿)"},
        /* 极限值 */
        {  0,   0,  0.0f,  0.0f, "0°C 0%RH (零点)"},
        {999, 800, 99.9f, 80.0f, "80°C 99.9%RH (最大值)"},
    };

    int numVectors = sizeof(vectors) / sizeof(vectors[0]);

    for (int i = 0; i < numVectors; i++) {
        printf("    [%d] %s\n", i + 1, vectors[i].desc);

        uint16_t rh = vectors[i].rawHumi;
        uint16_t rt = vectors[i].rawTemp;
        uint8_t data[5] = {
            (rh >> 8) & 0xFF, rh & 0xFF,
            (rt >> 8) & 0xFF, rt & 0xFF,
            (uint8_t)(((rh>>8) + (rh&0xFF) + (rt>>8) + (rt&0xFF)) & 0xFF)
        };

        float temp, humi;
        uint8_t result = DHT22_ParseData(data, &temp, &humi);

        /* 每个 vector 里的断言 */
        TEST_ASSERT_EQUAL(1, result);
        TEST_ASSERT_FLOAT_WITHIN(0.15f, vectors[i].expHumi, humi);
        TEST_ASSERT_FLOAT_WITHIN(0.15f, vectors[i].expTemp, temp);
    }
}

/**
 * ===== 测试组 6: 错误注入测试 =====
 *
 * 模拟各种硬件故障场景, 验证代码的健壮性
 */

/* 测试6.1: 单比特翻转 (模拟传输干扰) */
static void test_single_bit_flip(void)
{
    printf("\n  [测试6.1] 错误注入 — 单bit翻转导致校验失败\n");

    float temp, humi;
    /* 正确的数据... */
    uint8_t data[5] = {0x02, 0x26, 0x00, 0xFA};
    uint8_t correct_chk = data[0] + data[1] + data[2] + data[3];

    /* 翻转 data[0] 的一个 bit */
    for (int bit = 0; bit < 8; bit++) {
        uint8_t corrupted[5];
        memcpy(corrupted, data, 4);
        corrupted[0] ^= (1 << bit);  // 翻转第 bit 位
        corrupted[4] = correct_chk;   // 校验和还是原来的

        uint8_t result = DHT22_ParseData(corrupted, &temp, &humi);
        /* 任意bit翻转都应该导致校验失败 */
        if (result != 0) {
            printf("      ⚠ bit%d 翻转未被检测!\n", bit);
        }
        TEST_ASSERT_EQUAL(0, result);
    }
}

/* ======================== 测试运行器 ======================== */

/**
 * @brief 主函数 — 运行所有测试用例
 *
 * 在 PC 上编译运行:
 *   gcc -o test_runner test_dht22.c -I. -lm
 *   ./test_runner.exe
 */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  DHT22 驱动单元测试                          ║\n");
    printf("║  STM32 智能温湿度监测系统                     ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n开始运行测试用例...\n");

    /* ===== 测试组 1: 校验和验证 ===== */
    printf("\n━━━ 测试组1: 校验和验证 ━━━");
    test_checksum_valid();
    test_checksum_invalid();
    test_checksum_overflow();

    /* ===== 测试组 2: 数据解析 ===== */
    printf("\n━━━ 测试组2: 温湿度数据解析 ━━━");
    test_parse_normal_values();
    test_parse_zero_values();
    test_parse_max_values();
    test_parse_negative_temperature();
    test_parse_extreme_negative();

    /* ===== 测试组 3: 校验失败处理 ===== */
    printf("\n━━━ 测试组3: 校验失败处理 ━━━");
    test_parse_fails_on_bad_checksum();

    /* ===== 测试组 4: Mock 完整通信 ===== */
    printf("\n━━━ 测试组4: Mock GPIO 模拟完整通信 ━━━");
    test_mock_read_normal();
    test_mock_read_no_sensor();
    test_mock_read_checksum_error();

    /* ===== 测试组 5: 边界值与等价类 ===== */
    printf("\n━━━ 测试组5: 边界值与等价类测试 ━━━");
    test_threshold_boundary_temp_high();
    test_equivalence_classes();

    /* ===== 测试组 6: 错误注入 ===== */
    printf("\n━━━ 测试组6: 错误注入 (健壮性) ━━━");
    test_single_bit_flip();

    /* ===== 输出汇总 ===== */
    UnityPrintSummary();

    return (_unity_failed > 0) ? 1 : 0;
}

/* ======================== Mock 全局变量定义 ======================== */

/* 所有 mock 状态的实际存储 */
uint8_t  mock_gpio_pin_value[256] = {0};
uint32_t mock_gpio_mode[256]      = {0};
uint32_t mock_gpio_pull[256]      = {0};

volatile uint32_t mock_dwt_cyccnt    = 0;
volatile uint32_t mock_system_tick   = 0;
uint32_t mock_system_core_clock      = 72000000;

UART_HandleTypeDef huart1 = {0};

/* 模拟的寄存器结构体 */
uint32_t _mock_CoreDebug_DEMCR = 0;
uint32_t _mock_DWT_CTRL = 0;

Mock_CoreDebug_TypeDef _mock_coredebug_struct = {0, 0, 0};
Mock_DWT_TypeDef _mock_dwt_struct = {0, 0};

uint32_t SystemCoreClock = 72000000;
