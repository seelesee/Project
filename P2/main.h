/**
 * @file    main.h
 * @brief   STM32智能温湿度监测系统 - 主头文件
 * @author  Claude
 * @date    2026-06-18
 *
 * 功能说明:
 *   - DHT22 温湿度传感器采集
 *   - SSD1306 OLED 实时显示
 *   - 3按键 阀值设定 (UP/DOWN/SET)
 *   - 蜂鸣器 超限报警
 *   - 串口协议 数据上传
 *
 * 硬件平台: STM32F103C8T6 (Blue Pill)
 * 开发环境: Keil MDK / STM32CubeIDE
 * 库类型:   STM32 HAL Library
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ======================== 硬件引脚定义 ======================== */

/* --- DHT22 温湿度传感器 --- */
#define DHT22_GPIO_PORT        GPIOA
#define DHT22_GPIO_PIN         GPIO_PIN_0

/* --- SSD1306 OLED (软件I2C) --- */
#define OLED_SCL_PORT          GPIOB
#define OLED_SCL_PIN           GPIO_PIN_6
#define OLED_SDA_PORT          GPIOB
#define OLED_SDA_PIN           GPIO_PIN_7
#define OLED_I2C_ADDR          0x78    // SSD1306 7位地址左移1位

/* --- 按键 --- */
#define KEY_UP_PORT            GPIOA
#define KEY_UP_PIN             GPIO_PIN_1
#define KEY_DOWN_PORT          GPIOA
#define KEY_DOWN_PIN           GPIO_PIN_2
#define KEY_SET_PORT           GPIOA
#define KEY_SET_PIN            GPIO_PIN_3

/* --- 蜂鸣器 (有源) --- */
#define BUZZER_PORT            GPIOA
#define BUZZER_PIN             GPIO_PIN_4

/* --- 串口调试/上传 --- */
#define DEBUG_UART             huart1
#define PROTOCOL_SYNC1         0xAA    // 帧同步头1
#define PROTOCOL_SYNC2         0x55    // 帧同步头2

/* ======================== 系统参数宏 ======================== */

/* 数据采集周期 (ms) */
#define SAMPLE_INTERVAL_MS     2000

/* 串口上报周期 (ms) — 通常为采集周期的整数倍 */
#define UPLOAD_INTERVAL_MS     5000

/* 按键扫描周期 (ms) */
#define KEY_SCAN_INTERVAL_MS   20

/* 按键消抖时间 (ms) */
#define KEY_DEBOUNCE_MS        50

/* 长按时间 (ms) */
#define KEY_LONG_PRESS_MS      1000

/* 报警最小间隔 (ms) — 防止连续蜂鸣 */
#define ALARM_COOLDOWN_MS      3000

/* 温湿度默认阀值 */
#define TEMP_HIGH_DEFAULT      35.0f   // 温度上限 °C
#define TEMP_LOW_DEFAULT       5.0f    // 温度下限 °C
#define HUMI_HIGH_DEFAULT      80.0f   // 湿度上限 %RH
#define HUMI_LOW_DEFAULT       20.0f   // 湿度下限 %RH

/* 阀值调整步进 */
#define THRESHOLD_STEP         0.5f

/* OLED 显示刷新率 (ms) */
#define DISPLAY_REFRESH_MS     500

/* DHT22 最大重试次数 */
#define DHT22_MAX_RETRY        3

/* ======================== 数据结构 ======================== */

/**
 * @brief 温湿度数据结构体
 */
typedef struct {
    float temperature;      // 温度值 (°C)
    float humidity;         // 湿度值 (%RH)
    uint8_t valid;          // 数据有效标志: 1=有效, 0=无效
    uint32_t timestamp;     // 采集时间戳 (ms)
} SensorData_t;

/**
 * @brief 阀值配置结构体
 */
typedef struct {
    float temp_high;        // 温度上限
    float temp_low;         // 温度下限
    float humi_high;        // 湿度上限
    float humi_low;         // 湿度下限
} Threshold_t;

/**
 * @brief 系统运行模式
 */
typedef enum {
    MODE_NORMAL = 0,        // 正常监测模式
    MODE_SET_TEMP_HIGH,     // 设定温度上限
    MODE_SET_TEMP_LOW,      // 设定温度下限
    MODE_SET_HUMI_HIGH,     // 设定湿度上限
    MODE_SET_HUMI_LOW,      // 设定湿度下限
    MODE_MAX
} SystemMode_t;

/**
 * @brief 报警状态
 */
typedef enum {
    ALARM_NONE = 0,
    ALARM_TEMP_HIGH,        // 温度过高
    ALARM_TEMP_LOW,         // 温度过低
    ALARM_HUMI_HIGH,        // 湿度过高
    ALARM_HUMI_LOW,         // 湿度过低
} AlarmType_t;

/**
 * @brief 串口协议命令字
 */
typedef enum {
    CMD_READ_DATA       = 0x01,   // 读取当前温湿度
    CMD_READ_THRESHOLD  = 0x02,   // 读取阀值配置
    CMD_SET_THRESHOLD   = 0x03,   // 设置阀值
    CMD_ALARM_NOTIFY    = 0x04,   // 报警通知(主动上报)
    CMD_ACK             = 0x80,   // 应答
    CMD_NACK            = 0xFF    // 错误应答
} ProtocolCmd_t;

/**
 * @brief 串口协议帧结构
 */
typedef struct {
    uint8_t sync1;          // 同步头1: 0xAA
    uint8_t sync2;          // 同步头2: 0x55
    uint8_t cmd;            // 命令字
    uint8_t length;         // 数据长度 (不含帧头/长度/校验)
    uint8_t data[32];       // 数据区
    uint8_t checksum;       // 校验和 (sync1~data末尾 XOR)
} ProtocolFrame_t;

/* ======================== 全局变量声明 ======================== */
extern SensorData_t  g_sensorData;
extern Threshold_t   g_threshold;
extern SystemMode_t  g_sysMode;
extern uint8_t       g_alarmActive;
extern AlarmType_t   g_alarmType;

/* ======================== 函数声明 ======================== */

/* 系统初始化 */
void SystemClock_Config(void);
void GPIO_Init(void);
void USART1_Init(void);

/* 延时 (微秒级, 使用DWT) */
void DelayUs(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
