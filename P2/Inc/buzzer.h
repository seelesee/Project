/**
 * @file    buzzer.h
 * @brief   蜂鸣器报警模块
 *
 * 支持:
 *   - 连续鸣响 (报警)
 *   - 间歇鸣响 (提示)
 *   - 短促提示音 (按键反馈)
 *
 * 接线: 有源蜂鸣器 → PA4 (通过NPN三极管驱动, 或直接IO驱动需加限流)
 */

#ifndef __BUZZER_H
#define __BUZZER_H

#include "main.h"

/* 蜂鸣器引脚操作 */
#define BUZZER_ON()   HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET)
#define BUZZER_OFF()  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET)
#define BUZZER_TOGGLE() HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN)

/**
 * @brief 初始化蜂鸣器引脚
 */
void Buzzer_Init(void);

/**
 * @brief 报警模式 - 连续响
 * @param enable 1=开启, 0=关闭
 */
void Buzzer_Alarm(uint8_t enable);

/**
 * @brief 间歇报警 (需在主循环/定时器中周期性调用)
 * @param onMs  鸣响时间 (ms)
 * @param offMs 静音时间 (ms)
 * @param cycles 循环次数
 */
void Buzzer_BeepPattern(uint32_t onMs, uint32_t offMs, uint8_t cycles);

/**
 * @brief 短促提示音 (按键反馈, 50ms)
 */
void Buzzer_KeyBeep(void);

/**
 * @brief 蜂鸣器状态机处理 (放在主循环中每10ms调用)
 */
void Buzzer_Process(uint32_t sysTick);

#endif /* __BUZZER_H */
