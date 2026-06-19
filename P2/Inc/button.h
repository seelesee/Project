/**
 * @file    button.h
 * @brief   按键驱动模块
 *
 * 功能:
 *   - 3个独立按键: SET(设置/确认), UP(加), DOWN(减)
 *   - 支持: 单击、长按(1s)、释放检测
 *   - 软件消抖 (50ms)
 *   - 非阻塞扫描, 放在定时器中断或主循环中调用
 *
 * 按键接线:
 *   SET  → PA3  (外部下拉, 按下接高电平 / 或使用内部下拉)
 *   UP   → PA1
 *   DOWN → PA2
 *
 * 按键电平逻辑 (可根据实际电路修改 KEY_PRESSED_LEVEL):
 *   下拉电路: 按下=高电平 → KEY_PRESSED_LEVEL = GPIO_PIN_SET
 *   上拉电路: 按下=低电平 → KEY_PRESSED_LEVEL = GPIO_PIN_RESET
 */

#ifndef __BUTTON_H
#define __BUTTON_H

#include "main.h"

/* 按键按下的电平状态 (根据实际电路修改!) */
#define KEY_PRESSED_LEVEL   GPIO_PIN_SET   // 下拉+按下高电平

/* 按键ID */
typedef enum {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_SET
} KeyID_t;

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE = 0,       // 无事件
    KEY_EVENT_CLICK,          // 单击 (按下后释放)
    KEY_EVENT_LONG_PRESS,     // 长按 (持续1秒)
    KEY_EVENT_RELEASE,        // 释放
} KeyEvent_t;

/* 按键状态结构体 */
typedef struct {
    uint8_t  pressed;         // 当前是否按下
    uint8_t  lastState;       // 上次扫描状态
    uint32_t pressTime;       // 按下时刻 (ms)
    uint32_t holdTime;        // 按住持续时间 (ms)
    uint8_t  eventPending;    // 有待处理事件
    KeyEvent_t event;         // 当前事件类型
    uint8_t  longPressSent;   // 长按事件已发送
} KeyState_t;

/**
 * @brief 初始化按键引脚
 */
void Button_Init(void);

/**
 * @brief 按键扫描 (每20ms调用一次)
 * @param sysTick 系统运行毫秒数 (HAL_GetTick())
 */
void Button_Scan(uint32_t sysTick);

/**
 * @brief 获取按键事件
 * @return KeyEvent_t 当前按键事件
 */
KeyEvent_t Button_GetEvent(KeyID_t *keyId);

/**
 * @brief 检测是否有按键按下 (不消费事件)
 * @return 1=有按键按下, 0=无
 */
uint8_t Button_AnyPressed(void);

#endif /* __BUTTON_H */
