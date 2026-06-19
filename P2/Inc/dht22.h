/**
 * @file    dht22.h
 * @brief   DHT22 数字温湿度传感器驱动 (单总线)
 *
 * 特性:
 *   - 温度范围: -40 ~ 80°C, 精度 ±0.5°C
 *   - 湿度范围: 0 ~ 100%RH, 精度 ±2%RH
 *   - 采样周期: ≥2秒
 *   - 使用 DWT 计数器实现微秒级精确延时
 *
 * 接线: DATA → PA0 (需外接 4.7K~10K 上拉电阻至 VCC)
 */

#ifndef __DHT22_H
#define __DHT22_H

#include "main.h"

/* DHT22 引脚操作宏 */
#define DHT22_DATA_HIGH()   HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET)
#define DHT22_DATA_LOW()    HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_RESET)
#define DHT22_DATA_READ()   HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN)

/* DHT22 时序参数 (us) */
#define DHT22_START_LOW     1200    // 主机拉低起始信号
#define DHT22_START_HIGH    30      // 主机拉高释放
#define DHT22_RESP_LOW      80      // 从机响应低电平 (典型80us)
#define DHT22_RESP_HIGH     80      // 从机响应高电平 (典型80us)
#define DHT22_BIT0_HIGH     28      // '0' 高电平 (26-28us)
#define DHT22_BIT1_HIGH     70      // '1' 高电平 (70us)
#define DHT22_TIMEOUT       200     // 超时判断 (us)

/**
 * @brief 初始化 DHT22 引脚
 */
void DHT22_Init(void);

/**
 * @brief 读取 DHT22 温湿度数据
 * @param temperature 输出: 温度值 (°C)
 * @param humidity    输出: 湿度值 (%RH)
 * @return uint8_t: 1=成功, 0=失败(校验错误/超时)
 */
uint8_t DHT22_Read(float *temperature, float *humidity);

#endif /* __DHT22_H */
