/**
 * @file    mock_hal.h
 * @brief   Mock HAL 层 - 让 STM32 代码能在 PC 上编译和测试
 *
 * ==================== 核心概念: 什么是 Mock? ====================
 *
 * 嵌入式代码依赖硬件 (GPIO, 定时器, 中断...), 无法直接在 PC 上运行。
 * Mock (模拟) 就是创建"假的"硬件函数, 让代码以为在操作真实硬件,
 * 实际上是在 PC 内存中运行。
 *
 * 层次关系:
 *   ┌──────────────────────────────────┐
 *   │         test_dht22.c             │  ← 测试用例 (在 PC 上运行)
 *   ├──────────────────────────────────┤
 *   │         dht22.c                  │  ← 被测代码 (原封不动)
 *   ├──────────────────────────────────┤
 *   │   mock_hal.h  (本文件)           │  ← Mock 层 (替代真实 HAL)
 *   ├──────────────────────────────────┤
 *   │   PC (GCC/MinGW)                 │  ← 运行平台
 *   └──────────────────────────────────┘
 *
 * 测试时我们设置 mock 值, 被测代码读取这些值, 就像在操作真实硬件:
 *   测试代码:  mock_gpio_input_value = GPIO_PIN_SET;  // "假装PA0是高电平"
 *   被测代码:  HAL_GPIO_ReadPin(...) → 返回 GPIO_PIN_SET  // 读到了!
 */

#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== STM32 类型模拟 ======================== */

typedef uint32_t (*Mock_GPIOCallback)(uint16_t);  // 模拟GPIO回调类型

/* GPIO 模拟 */
typedef struct {
    uint16_t Pin;        // 引脚号
    uint32_t Mode;       // 模式
    uint32_t Pull;       // 上下拉
    uint32_t Speed;      // 速度
} GPIO_InitTypeDef;

/* UART 模拟 */
typedef struct {
    void *Instance;
    struct {
        uint32_t BaudRate;
        uint32_t WordLength;
        uint32_t StopBits;
        uint32_t Parity;
        uint32_t Mode;
        uint32_t HwFlowCtl;
        uint32_t OverSampling;
    } Init;
} UART_HandleTypeDef;

/* RCC 模拟 */
typedef struct {
    uint32_t OscillatorType;
    uint32_t HSEState;
    uint32_t HSEPredivValue;
    struct {
        uint32_t PLLState;
        uint32_t PLLSource;
        uint32_t PLLMUL;
    } PLL;
} RCC_OscInitTypeDef;

typedef struct {
    uint32_t ClockType;
    uint32_t SYSCLKSource;
    uint32_t AHBCLKDivider;
    uint32_t APB1CLKDivider;
    uint32_t APB2CLKDivider;
} RCC_ClkInitTypeDef;

/* Cortex-M 内核寄存器模拟 */
typedef struct {
    volatile uint32_t CTRL;       // DWT控制寄存器
    volatile uint32_t CYCCNT;     // DWT周期计数器
    volatile uint32_t DEMCR;     // CoreDebug DEMCR
} Mock_CoreDebug_TypeDef;

/* ======================== 引脚/端口宏 ======================== */

#define GPIO_PIN_0       ((uint16_t)0x0001)
#define GPIO_PIN_1       ((uint16_t)0x0002)
#define GPIO_PIN_2       ((uint16_t)0x0004)
#define GPIO_PIN_3       ((uint16_t)0x0008)
#define GPIO_PIN_4       ((uint16_t)0x0010)
#define GPIO_PIN_5       ((uint16_t)0x0020)
#define GPIO_PIN_6       ((uint16_t)0x0040)
#define GPIO_PIN_7       ((uint16_t)0x0080)
#define GPIO_PIN_8       ((uint16_t)0x0100)
#define GPIO_PIN_9       ((uint16_t)0x0200)
#define GPIO_PIN_10      ((uint16_t)0x0400)
#define GPIO_PIN_13      ((uint16_t)0x2000)
#define GPIO_PIN_14      ((uint16_t)0x4000)
#define GPIO_PIN_15      ((uint16_t)0x8000)
#define GPIO_PIN_ALL     ((uint16_t)0xFFFF)

#define GPIO_PIN_SET     ((uint8_t)1)
#define GPIO_PIN_RESET   ((uint8_t)0)

/* GPIO 模式 (仅保留代码中用到的) */
#define GPIO_MODE_OUTPUT_OD      ((uint32_t)0x00000005)  // 开漏输出
#define GPIO_MODE_OUTPUT_PP      ((uint32_t)0x00000001)  // 推挽输出
#define GPIO_MODE_AF_PP          ((uint32_t)0x00000002)  // 复用推挽
#define GPIO_MODE_INPUT          ((uint32_t)0x00000000)  // 输入
#define GPIO_MODE_ANALOG         ((uint32_t)0x00000003)  // 模拟

#define GPIO_NOPULL              ((uint32_t)0x00000000)
#define GPIO_PULLUP              ((uint32_t)0x00000001)
#define GPIO_PULLDOWN            ((uint32_t)0x00000002)

#define GPIO_SPEED_FREQ_LOW      ((uint32_t)0x00000002)
#define GPIO_SPEED_FREQ_MEDIUM   ((uint32_t)0x00000001)
#define GPIO_SPEED_FREQ_HIGH     ((uint32_t)0x00000003)

/* ======================== 全局 Mock 状态 ======================== */

/**
 * Mock 的核心: 所有 GPIO 状态存在这些变量里
 * 测试代码可以自由读写, 模拟真实硬件的行为
 */
extern uint8_t  mock_gpio_pin_value[256];   // 每个引脚的当前电平 (0/1)
extern uint32_t mock_gpio_mode[256];        // 每个引脚的模式
extern uint32_t mock_gpio_pull[256];        // 每个引脚的上下拉

extern volatile uint32_t mock_dwt_cyccnt;    // DWT计数器值 (模拟时间流逝)
extern volatile uint32_t mock_system_tick;   // HAL_GetTick() 返回值
extern uint32_t mock_system_core_clock;      // 系统时钟频率 (默认72MHz)

extern UART_HandleTypeDef huart1;            // 模拟的 UART1 句柄

/**
 * 重置所有 Mock 状态 (每个测试用例开始前调用)
 */
static inline void mock_reset_all(void)
{
    memset(mock_gpio_pin_value, 0, sizeof(mock_gpio_pin_value));
    memset(mock_gpio_mode, 0, sizeof(mock_gpio_mode));
    memset(mock_gpio_pull, 0, sizeof(mock_gpio_pull));
    mock_dwt_cyccnt = 0;
    mock_system_tick = 0;
    mock_system_core_clock = 72000000;  // 默认72MHz
}

/* ======================== HAL 函数 Mock ======================== */

/* 写 GPIO 引脚 (设置 mock 状态) */
static inline void HAL_GPIO_WritePin(void *GPIOx, uint16_t GPIO_Pin, uint8_t PinState)
{
    (void)GPIOx;  // Mock 层不需要端口地址
    /* 简单的映射: 根据Pin值找到对应mock索引 */
    int idx = 0;
    uint16_t p = GPIO_Pin;
    while (p) { p >>= 1; idx++; }
    idx--;  // Pin_0 → idx=0, Pin_1 → idx=1, ...
    if (idx >= 0 && idx < 256) {
        mock_gpio_pin_value[idx] = PinState;
    }
}

/* 读 GPIO 引脚 (返回 mock 中设置的值) */
static inline uint8_t HAL_GPIO_ReadPin(void *GPIOx, uint16_t GPIO_Pin)
{
    (void)GPIOx;
    int idx = 0;
    uint16_t p = GPIO_Pin;
    while (p) { p >>= 1; idx++; }
    idx--;
    if (idx >= 0 && idx < 256) {
        return mock_gpio_pin_value[idx];
    }
    return 0;
}

/* GPIO 初始化 (存到 mock 状态) */
static inline void HAL_GPIO_Init(void *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    (void)GPIOx;
    int idx = 0;
    uint16_t p = GPIO_Init->Pin;
    while (p) { p >>= 1; idx++; }
    idx--;
    if (idx >= 0 && idx < 256) {
        mock_gpio_mode[idx] = GPIO_Init->Mode;
        mock_gpio_pull[idx] = GPIO_Init->Pull;
    }
}

/* HAL 初始化 */
static inline uint32_t HAL_Init(void) { return 0; }

/* HAL 延时 (测试中可以不做任何事) */
static inline void HAL_Delay(uint32_t ms)
{
    mock_system_tick += ms;  // 推进模拟时间
}

/* HAL 获取 Tick */
static inline uint32_t HAL_GetTick(void)
{
    return mock_system_tick;
}

/* RCC 时钟配置 */
static inline uint32_t HAL_RCC_OscConfig(void *cfg)      { (void)cfg; return 0; }
static inline uint32_t HAL_RCC_ClockConfig(void *cfg, uint32_t latency) { (void)cfg; (void)latency; return 0; }

/* UART 初始化 */
static inline uint32_t HAL_UART_Init(void *huart) { (void)huart; return 0; }
static inline uint32_t HAL_UART_Receive(void *huart, uint8_t *data, uint16_t size, uint32_t timeout) { (void)huart; (void)data; (void)size; (void)timeout; return 0; }

/* ======================== HAL 宏定义 ======================== */

#define __HAL_RCC_GPIOA_CLK_ENABLE()   do {} while(0)
#define __HAL_RCC_GPIOB_CLK_ENABLE()   do {} while(0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()   do {} while(0)
#define __HAL_RCC_USART1_CLK_ENABLE()  do {} while(0)
#define __HAL_UART_ENABLE_IT(huart, it) do {} while(0)

#define __HAL_RCC_HSE_CONFIG(state)    (state)

/* ======================== 寄存器模拟 ======================== */

/* CoreDebug->DEMCR 模拟 */
#define CoreDebug_DEMCR_TRCENA_Msk   (1UL << 24)

/* DWT 寄存器模拟 */
#define DWT_CTRL_CYCCNTENA_Msk       (1UL << 0)

/* 结构体指针模拟 (指向我们的全局变量) */
extern uint32_t _mock_CoreDebug_DEMCR;
extern uint32_t _mock_DWT_CTRL;

#define CoreDebug  ((Mock_CoreDebug_TypeDef*)&_mock_coredebug_struct)
extern Mock_CoreDebug_TypeDef _mock_coredebug_struct;

/* DWT 模拟 */
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} Mock_DWT_TypeDef;

extern Mock_DWT_TypeDef _mock_dwt_struct;
#define DWT  ((Mock_DWT_TypeDef*)&_mock_dwt_struct)

/* SystemCoreClock (全局变量, 在 main.h 或 system 中声明) */
extern uint32_t SystemCoreClock;

/* Flash 延时 */
#define FLASH_LATENCY_2  ((uint32_t)0x00000002)

/* RCC 配置常量 */
#define RCC_OSCILLATORTYPE_HSE    ((uint32_t)0x00000001)
#define RCC_HSE_ON                ((uint32_t)0x00000001)
#define RCC_HSE_PREDIV_DIV1       ((uint32_t)0x00000000)
#define RCC_PLL_ON                ((uint32_t)0x00000001)
#define RCC_PLLSOURCE_HSE         ((uint32_t)0x00000001)
#define RCC_PLL_MUL9              ((uint32_t)0x00000007)

#define RCC_CLOCKTYPE_HCLK        ((uint32_t)0x00000001)
#define RCC_CLOCKTYPE_SYSCLK      ((uint32_t)0x00000002)
#define RCC_CLOCKTYPE_PCLK1       ((uint32_t)0x00000004)
#define RCC_CLOCKTYPE_PCLK2       ((uint32_t)0x00000008)
#define RCC_SYSCLKSOURCE_PLLCLK   ((uint32_t)0x00000001)
#define RCC_SYSCLK_DIV1           ((uint32_t)0x00000000)
#define RCC_HCLK_DIV2             ((uint32_t)0x00000004)

/* UART 相关常量 */
#define UART_WORDLENGTH_8B        ((uint32_t)0x00000000)
#define UART_STOPBITS_1           ((uint32_t)0x00000000)
#define UART_PARITY_NONE          ((uint32_t)0x00000000)
#define UART_MODE_TX_RX           ((uint32_t)0x00000003)
#define UART_HWCONTROL_NONE       ((uint32_t)0x00000000)
#define UART_OVERSAMPLING_16      ((uint32_t)0x00000000)
#define UART_IT_RXNE              ((uint32_t)0x00000001)

/* HAL 状态 */
#define HAL_OK                    ((uint32_t)0x00000000)
#define HAL_ERROR                 ((uint32_t)0x00000001)

#ifdef __cplusplus
}
#endif

#endif /* MOCK_HAL_H */
