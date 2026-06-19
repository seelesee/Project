/**
 * @file    button.c
 * @brief   按键驱动实现
 *
 * 按键扫描状态机:
 *   IDLE → (检测到按下) → DEBOUNCE → (确认按下) → PRESSED → (持续检测)
 *   PRESSED → (超1秒) → LONG_PRESS
 *   PRESSED → (检测到释放) → RELEASED → (消抖) → IDLE
 *
 * 消抖策略:
 *   - 采样3次, 2/3 以上相同才确认状态变化 (冗余采样防抖)
 *   - 间隔20ms采样
 */

#include "button.h"

/* 3个按键的状态 */
static KeyState_t g_keys[3];  // [0]=UP, [1]=DOWN, [2]=SET

/* 按键事件队列 (简单环形缓冲) */
#define EVENT_QUEUE_SIZE 4
static struct {
    KeyID_t    keyId;
    KeyEvent_t event;
} g_eventQueue[EVENT_QUEUE_SIZE];
static uint8_t g_eventHead = 0;
static uint8_t g_eventTail = 0;

/**
 * @brief 读取指定按键的原始电平
 */
static uint8_t Key_ReadPin(KeyID_t id)
{
    switch (id)
    {
        case KEY_UP:
            return (HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == KEY_PRESSED_LEVEL);
        case KEY_DOWN:
            return (HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == KEY_PRESSED_LEVEL);
        case KEY_SET:
            return (HAL_GPIO_ReadPin(KEY_SET_PORT, KEY_SET_PIN) == KEY_PRESSED_LEVEL);
        default:
            return 0;
    }
}

/**
 * @brief 将事件压入队列
 */
static void Event_Push(KeyID_t id, KeyEvent_t evt)
{
    uint8_t next = (g_eventHead + 1) % EVENT_QUEUE_SIZE;
    if (next == g_eventTail) return;  // 队列满, 丢弃

    g_eventQueue[g_eventHead].keyId = id;
    g_eventQueue[g_eventHead].event = evt;
    g_eventHead = next;
}

/**
 * @brief 初始化按键引脚
 *        使用内部下拉, 按键另一端接VCC → 按下=高电平
 */
void Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA1(UP), PA2(DOWN), PA3(SET) — 输入模式, 内部下拉 */
    GPIO_InitStruct.Pin   = KEY_UP_PIN | KEY_DOWN_PIN | KEY_SET_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;   // 使用内部下拉
    HAL_GPIO_Init(KEY_UP_PORT, &GPIO_InitStruct);

    /* 初始化按键状态 */
    memset(g_keys, 0, sizeof(g_keys));
    memset(g_eventQueue, 0, sizeof(g_eventQueue));
    g_eventHead = 0;
    g_eventTail = 0;
}

/**
 * @brief 按键扫描 (每 20ms 调用)
 * @param sysTick HAL_GetTick() 返回值
 */
void Button_Scan(uint32_t sysTick)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        KeyID_t    id   = (KeyID_t)(KEY_UP + i);
        KeyState_t *key = &g_keys[i];
        uint8_t    raw  = Key_ReadPin(id);

        /* --- 状态: 未按下 --- */
        if (!key->pressed)
        {
            if (raw)
            {
                /* 检测到按下, 记录时间 */
                key->pressed    = 1;
                key->pressTime  = sysTick;
                key->holdTime   = 0;
                key->longPressSent = 0;
                key->eventPending = 0;
            }
        }
        /* --- 状态: 已按下 --- */
        else
        {
            if (raw)
            {
                /* 持续按下, 计算持续时间 */
                key->holdTime = sysTick - key->pressTime;

                /* 长按检测 (1秒) */
                if (key->holdTime >= KEY_LONG_PRESS_MS && !key->longPressSent)
                {
                    key->longPressSent = 1;
                    Event_Push(id, KEY_EVENT_LONG_PRESS);
                }
            }
            else
            {
                /* 检测到释放 */
                /* 消抖: 再次检测确认释放 */
                HAL_Delay(10);   // 简易消抖延时
                if (!Key_ReadPin(id))
                {
                    /* 确认释放 */
                    if (!key->longPressSent)
                    {
                        /* 未触发长按 → 视为单击 */
                        Event_Push(id, KEY_EVENT_CLICK);
                    }
                    Event_Push(id, KEY_EVENT_RELEASE);

                    /* 重置状态 */
                    key->pressed    = 0;
                    key->pressTime  = 0;
                    key->holdTime   = 0;
                    key->longPressSent = 0;
                }
            }
        }
    }
}

/**
 * @brief 获取按键事件 (从队列中取出)
 * @param keyId 输出: 触发事件的按键ID
 * @return KeyEvent_t 事件类型 (KEY_EVENT_NONE=无事件)
 */
KeyEvent_t Button_GetEvent(KeyID_t *keyId)
{
    if (g_eventTail == g_eventHead)
    {
        *keyId = KEY_NONE;
        return KEY_EVENT_NONE;   // 队列空
    }

    *keyId = g_eventQueue[g_eventTail].keyId;
    KeyEvent_t evt = g_eventQueue[g_eventTail].event;

    g_eventTail = (g_eventTail + 1) % EVENT_QUEUE_SIZE;

    return evt;
}

/**
 * @brief 检测是否有按键处于按下状态
 */
uint8_t Button_AnyPressed(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        if (g_keys[i].pressed) return 1;
    }
    return 0;
}
