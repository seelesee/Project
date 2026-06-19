/**
 * @file    buzzer.c
 * @brief   蜂鸣器报警模块实现
 *
 * 模式:
 *   BUZZER_IDLE     — 静音
 *   BUZZER_CONT     — 连续鸣响 (报警状态)
 *   BUZZER_PATTERN  — 间歇鸣响 (可配置 on/off 周期和循环次数)
 *   BUZZER_ONESHOT  — 单次短促 (按键提示音)
 */

#include "buzzer.h"

/* 蜂鸣器工作模式 */
typedef enum {
    BUZZER_IDLE = 0,
    BUZZER_CONT,
    BUZZER_PATTERN,
    BUZZER_ONESHOT,
} BuzzerMode_t;

static BuzzerMode_t g_buzzerMode = BUZZER_IDLE;

/* 间歇模式参数 */
static uint32_t g_patternOnMs  = 0;
static uint32_t g_patternOffMs = 0;
static uint8_t  g_patternCycles = 0;
static uint8_t  g_patternCount  = 0;
static uint8_t  g_patternPhase  = 0;    // 0=ON阶段, 1=OFF阶段
static uint32_t g_phaseStartTick = 0;

/* 单次模式参数 */
static uint32_t g_oneshotStartTick = 0;
static uint32_t g_oneshotDuration  = 50;

/**
 * @brief 初始化蜂鸣器
 */
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin   = BUZZER_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);

    BUZZER_OFF();
    g_buzzerMode = BUZZER_IDLE;
}

/**
 * @brief 连续报警
 */
void Buzzer_Alarm(uint8_t enable)
{
    if (enable)
    {
        BUZZER_ON();
        g_buzzerMode = BUZZER_CONT;
    }
    else
    {
        BUZZER_OFF();
        g_buzzerMode = BUZZER_IDLE;
    }
}

/**
 * @brief 间歇报警模式
 * @param onMs   鸣响时长
 * @param offMs  静音时长
 * @param cycles 循环次数
 */
void Buzzer_BeepPattern(uint32_t onMs, uint32_t offMs, uint8_t cycles)
{
    g_patternOnMs   = onMs;
    g_patternOffMs  = offMs;
    g_patternCycles = cycles;
    g_patternCount  = 0;
    g_patternPhase  = 0;   // 从ON阶段开始
    g_phaseStartTick = HAL_GetTick();
    g_buzzerMode    = BUZZER_PATTERN;
    BUZZER_ON();
}

/**
 * @brief 短促提示音 (50ms)
 */
void Buzzer_KeyBeep(void)
{
    g_oneshotStartTick = HAL_GetTick();
    g_oneshotDuration  = 50;
    g_buzzerMode = BUZZER_ONESHOT;
    BUZZER_ON();
}

/**
 * @brief 蜂鸣器状态机 (每10ms调用一次)
 * @param sysTick 当前系统tick
 */
void Buzzer_Process(uint32_t sysTick)
{
    switch (g_buzzerMode)
    {
        case BUZZER_IDLE:
            /* 确保关闭 (可能有残留) */
            BUZZER_OFF();
            break;

        case BUZZER_CONT:
            /* 连续鸣响 — 不需要处理 */
            break;

        case BUZZER_PATTERN:
        {
            uint32_t elapsed = sysTick - g_phaseStartTick;

            if (g_patternPhase == 0)
            {
                /* ON 阶段 */
                if (elapsed >= g_patternOnMs)
                {
                    BUZZER_OFF();
                    g_patternPhase = 1;
                    g_phaseStartTick = sysTick;
                }
            }
            else
            {
                /* OFF 阶段 */
                if (elapsed >= g_patternOffMs)
                {
                    g_patternCount++;

                    if (g_patternCount >= g_patternCycles)
                    {
                        /* 循环完成, 停止 */
                        BUZZER_OFF();
                        g_buzzerMode = BUZZER_IDLE;
                    }
                    else
                    {
                        BUZZER_ON();
                        g_patternPhase = 0;
                        g_phaseStartTick = sysTick;
                    }
                }
            }
            break;
        }

        case BUZZER_ONESHOT:
        {
            if ((sysTick - g_oneshotStartTick) >= g_oneshotDuration)
            {
                BUZZER_OFF();
                g_buzzerMode = BUZZER_IDLE;
            }
            break;
        }

        default:
            BUZZER_OFF();
            g_buzzerMode = BUZZER_IDLE;
            break;
    }
}
