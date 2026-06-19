/**
 * @file    main.c
 * @brief   STM32智能温湿度监测系统 - 主程序
 * @author  Claude
 * @date    2026-06-18
 *
 * 功能整合:
 *   1. DHT22 每2秒采集一次温湿度
 *   2. SSD1306 OLED 实时刷新显示
 *   3. 3按键 阀值设定 (SET切换/UP加/DOWN减)
 *   4. 蜂鸣器 超限报警 (间歇鸣响)
 *   5. 串口协议 数据上传 (5秒周期 + 报警即时上报)
 *
 * 运行流程:
 *   main() → 系统初始化 → 显示Logo → 加载配置 → 主循环 {
 *       按键扫描(20ms) → 数据处理 → 界面刷新 → 报警检测 → 协议处理
 *   }
 */

#include "main.h"
#include "dht22.h"
#include "oled.h"
#include "oled_font.h"
#include "button.h"
#include "buzzer.h"
#include "protocol.h"
#include "config.h"

/* ======================== 全局变量 ======================== */
SensorData_t  g_sensorData;      // 当前传感器数据
Threshold_t   g_threshold;       // 阀值配置
SystemMode_t  g_sysMode   = MODE_NORMAL;  // 当前系统模式
uint8_t       g_alarmActive = 0;          // 当前是否处于报警状态
AlarmType_t   g_alarmType  = ALARM_NONE;  // 当前报警类型

/* ======================== 私有变量 ======================== */
static uint32_t g_lastSampleTick   = 0;    // 上次采集时刻
static uint32_t g_lastDisplayTick  = 0;    // 上次刷新显示时刻
static uint32_t g_lastUploadTick   = 0;    // 上次上传时刻
static uint32_t g_lastAlarmTick    = 0;    // 上次报警时刻
static uint32_t g_lastKeyScanTick  = 0;    // 上次按键扫描时刻
static uint32_t g_lastBuzzerTick   = 0;    // 上次蜂鸣器处理时刻
static uint8_t  g_modeBlink        = 0;    // 设定模式闪烁标志
static uint32_t g_lastBlinkTick    = 0;    // 上次闪烁切换时刻
static uint8_t  g_displayNeedsRefresh = 1; // 显示需要刷新标志

/* ======================== 函数原型 ======================== */
static void Alarm_Check(void);
static void KeyEvent_Process(KeyID_t key, KeyEvent_t event);
static void Mode_EnterSet(void);
static void Mode_ExitSet(void);
static void System_Init(void);

/* ======================== 主函数 ======================== */

/**
 * @brief 主函数入口
 */
int main(void)
{
    /* --- 系统初始化 --- */
    System_Init();

    /* --- 开机Logo (显示2秒) --- */
    OLED_ShowLogo();
    HAL_Delay(2000);

    /* --- 加载配置 --- */
    if (!Config_Load(&g_threshold))
    {
        /* Flash配置无效, 使用默认值并保存 */
        Config_ResetDefault(&g_threshold);
        Config_Save(&g_threshold);
    }

    /* 首次显示主界面 */
    g_displayNeedsRefresh = 1;

    /* --- 主循环 --- */
    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* ===== 1. 按键扫描 (每20ms) ===== */
        if ((now - g_lastKeyScanTick) >= KEY_SCAN_INTERVAL_MS)
        {
            g_lastKeyScanTick = now;
            Button_Scan(now);

            /* 处理按键事件 */
            KeyID_t    keyId;
            KeyEvent_t event = Button_GetEvent(&keyId);
            if (event != KEY_EVENT_NONE)
            {
                KeyEvent_Process(keyId, event);
            }
        }

        /* ===== 2. 蜂鸣器状态机 (每10ms) ===== */
        if ((now - g_lastBuzzerTick) >= 10)
        {
            g_lastBuzzerTick = now;
            Buzzer_Process(now);
        }

        /* ===== 3. 数据采集 (每2秒) ===== */
        if ((now - g_lastSampleTick) >= SAMPLE_INTERVAL_MS)
        {
            g_lastSampleTick = now;

            float temp, humi;
            uint8_t success = 0;

            /* 重试读取 (最多3次) */
            for (uint8_t retry = 0; retry < DHT22_MAX_RETRY; retry++)
            {
                if (DHT22_Read(&temp, &humi))
                {
                    success = 1;
                    break;
                }
                HAL_Delay(50);  // 重试间隔
            }

            if (success)
            {
                g_sensorData.temperature = temp;
                g_sensorData.humidity    = humi;
                g_sensorData.valid       = 1;
                g_sensorData.timestamp   = now;

                /* 正常模式下检查报警 */
                if (g_sysMode == MODE_NORMAL)
                {
                    Alarm_Check();
                }
            }
            else
            {
                g_sensorData.valid = 0;
                /* 连续读取失败不立即报警, 保持上次数据 */
            }

            g_displayNeedsRefresh = 1;
        }

        /* ===== 4. 显示刷新 (每500ms) ===== */
        if ((now - g_lastDisplayTick) >= DISPLAY_REFRESH_MS)
        {
            g_lastDisplayTick = now;

            if (g_sysMode == MODE_NORMAL)
            {
                /* 主界面 */
                OLED_ShowMainUI(g_sensorData.temperature,
                                g_sensorData.humidity,
                                g_alarmActive);
            }
            else
            {
                /* 阀值设定界面 (500ms闪烁提示当前编辑项) */
                g_displayNeedsRefresh = 1;
            }

            /* 设定模式闪烁逻辑 (每500ms翻转) */
            if (g_sysMode != MODE_NORMAL)
            {
                g_modeBlink = !g_modeBlink;
                g_displayNeedsRefresh = 1;
            }
        }

        /* ===== 5. 强制刷新显存到屏幕 ===== */
        if (g_displayNeedsRefresh)
        {
            g_displayNeedsRefresh = 0;

            if (g_sysMode == MODE_NORMAL)
            {
                OLED_ShowMainUI(g_sensorData.temperature,
                                g_sensorData.humidity,
                                g_alarmActive);
            }
            else
            {
                /* 设定模式下根据闪烁标志显示 (闪烁=隐藏当前编辑值) */
                if (g_modeBlink)
                {
                    OLED_ShowSetUI(g_sysMode, &g_threshold);
                }
                else
                {
                    /* 闪烁遮罩: 临时把当前编辑值改为0显示, 造成"消失"效果 */
                    Threshold_t tempThr = g_threshold;
                    switch (g_sysMode)
                    {
                        case MODE_SET_TEMP_HIGH: tempThr.temp_high = 0; break;
                        case MODE_SET_TEMP_LOW:  tempThr.temp_low  = 0; break;
                        case MODE_SET_HUMI_HIGH: tempThr.humi_high = 0; break;
                        case MODE_SET_HUMI_LOW:  tempThr.humi_low  = 0; break;
                        default: break;
                    }
                    OLED_ShowSetUI(g_sysMode, &tempThr);
                }
            }
        }

        /* ===== 6. 串口协议处理 ===== */
        Protocol_Process();

        /* ===== 7. 定时数据上传 (每5秒) ===== */
        if ((now - g_lastUploadTick) >= UPLOAD_INTERVAL_MS)
        {
            g_lastUploadTick = now;

            if (g_sensorData.valid)
            {
                Protocol_UploadData(&g_sensorData);
            }
        }

        /* ===== 8. 主循环延时 (释放CPU, 降低功耗) ===== */
        HAL_Delay(5);
    }
}

/* ======================== 系统初始化 ======================== */

/**
 * @brief 系统全面初始化
 */
static void System_Init(void)
{
    /* HAL库初始化 */
    HAL_Init();

    /* 系统时钟配置: 72MHz (HSE 8MHz → PLL x9) */
    SystemClock_Config();

    /* GPIO 初始化 */
    GPIO_Init();

    /* UART1 初始化 */
    USART1_Init();

    /* 模块初始化 */
    DHT22_Init();
    OLED_Init();
    Button_Init();
    Buzzer_Init();
    Protocol_Init();

    /* 初始化全局变量 */
    memset(&g_sensorData, 0, sizeof(g_sensorData));
    g_sysMode      = MODE_NORMAL;
    g_alarmActive  = 0;
    g_alarmType    = ALARM_NONE;

    /* 启动UART1接收中断 (单字节接收) */
    __HAL_UART_ENABLE_IT(&DEBUG_UART, UART_IT_RXNE);
}

/**
 * @brief 系统时钟配置
 *        HSE 8MHz → PLL x9 → SYSCLK 72MHz
 *        APB1 = 36MHz, APB2 = 72MHz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 使能 HSE 振荡器 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* 配置系统时钟、AHB、APB1、APB2 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    /* 使能 DWT 时钟 (用于微秒延时) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief GPIO 初始化 (除DHT22/OLED/按键/蜂鸣器外, 还包括UART1引脚)
 */
void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 默认把所有引脚设为模拟输入 (降低功耗) */
    HAL_GPIO_WritePin(GPIOA, 0xFFFF, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, 0xFFFF, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, 0xFFFF, GPIO_PIN_RESET);

    /* 各模块的 GPIO 初始化由各自的 Init() 函数完成 */
    /* UART1 的 GPIO 在 USART1_Init() 中初始化 */
}

/* ======================== 报警检测 ======================== */

/**
 * @brief 检查温湿度是否超出阀值
 */
static void Alarm_Check(void)
{
    if (!g_sensorData.valid) return;

    AlarmType_t newAlarm = ALARM_NONE;

    /* 温度检查 */
    if (g_sensorData.temperature > g_threshold.temp_high)
    {
        newAlarm = ALARM_TEMP_HIGH;
    }
    else if (g_sensorData.temperature < g_threshold.temp_low)
    {
        newAlarm = ALARM_TEMP_LOW;
    }
    /* 湿度检查 */
    else if (g_sensorData.humidity > g_threshold.humi_high)
    {
        newAlarm = ALARM_HUMI_HIGH;
    }
    else if (g_sensorData.humidity < g_threshold.humi_low)
    {
        newAlarm = ALARM_HUMI_LOW;
    }

    if (newAlarm != ALARM_NONE)
    {
        /* 确认报警 */
        if (!g_alarmActive || g_alarmType != newAlarm)
        {
            g_alarmActive = 1;
            g_alarmType   = newAlarm;

            /* 启动蜂鸣器间歇报警: 响200ms, 停500ms, 循环5次 */
            Buzzer_BeepPattern(200, 500, 5);

            /* 串口即时上报报警 */
            float alarmValue = 0;
            switch (newAlarm)
            {
                case ALARM_TEMP_HIGH:
                case ALARM_TEMP_LOW:
                    alarmValue = g_sensorData.temperature;
                    break;
                case ALARM_HUMI_HIGH:
                case ALARM_HUMI_LOW:
                    alarmValue = g_sensorData.humidity;
                    break;
                default: break;
            }
            Protocol_UploadAlarm(newAlarm, alarmValue);
        }
    }
    else
    {
        /* 数据恢复正常, 解除报警 */
        if (g_alarmActive)
        {
            g_alarmActive = 0;
            g_alarmType   = ALARM_NONE;
            Buzzer_Alarm(0);  // 关闭蜂鸣器
        }
    }
}

/* ======================== 按键事件处理 ======================== */

/**
 * @brief 处理按键事件
 * @param key   按键ID
 * @param event 事件类型
 */
static void KeyEvent_Process(KeyID_t key, KeyEvent_t event)
{
    switch (g_sysMode)
    {
        case MODE_NORMAL:
        {
            /* ===== 正常模式 ===== */
            if (event == KEY_EVENT_CLICK)
            {
                switch (key)
                {
                    case KEY_SET:
                        /* 进入设定模式 */
                        Mode_EnterSet();
                        break;

                    case KEY_UP:
                        /* 手动触发一次数据上传 */
                        if (g_sensorData.valid)
                        {
                            Protocol_UploadData(&g_sensorData);
                        }
                        Buzzer_KeyBeep();
                        break;

                    case KEY_DOWN:
                        /* 静音报警 (如果正在报警) */
                        if (g_alarmActive)
                        {
                            Buzzer_Alarm(0);
                            g_alarmActive = 0;
                        }
                        Buzzer_KeyBeep();
                        break;

                    default: break;
                }
            }
            else if (event == KEY_EVENT_LONG_PRESS)
            {
                if (key == KEY_SET)
                {
                    /* 长按SET → 恢复默认阀值 */
                    Config_ResetDefault(&g_threshold);
                    Config_Save(&g_threshold);
                    Buzzer_BeepPattern(100, 100, 3);  // 提示音
                }
            }
            break;
        }

        case MODE_SET_TEMP_HIGH:
        case MODE_SET_TEMP_LOW:
        case MODE_SET_HUMI_HIGH:
        case MODE_SET_HUMI_LOW:
        {
            /* ===== 阀值设定模式 ===== */
            if (event == KEY_EVENT_CLICK)
            {
                float *pValue = NULL;
                float step = THRESHOLD_STEP;

                /* 确定当前编辑的是哪个阀值 */
                switch (g_sysMode)
                {
                    case MODE_SET_TEMP_HIGH: pValue = &g_threshold.temp_high; break;
                    case MODE_SET_TEMP_LOW:  pValue = &g_threshold.temp_low;  break;
                    case MODE_SET_HUMI_HIGH: pValue = &g_threshold.humi_high; break;
                    case MODE_SET_HUMI_LOW:  pValue = &g_threshold.humi_low;  break;
                    default: break;
                }

                switch (key)
                {
                    case KEY_UP:
                        /* 增加 */
                        if (pValue)
                        {
                            *pValue += step;
                            /* 温度范围限制: -40 ~ 80 */
                            if (g_sysMode == MODE_SET_TEMP_HIGH ||
                                g_sysMode == MODE_SET_TEMP_LOW)
                            {
                                if (*pValue > 80.0f)  *pValue = 80.0f;
                                if (*pValue < -40.0f) *pValue = -40.0f;
                            }
                            /* 湿度范围限制: 0 ~ 100 */
                            else
                            {
                                if (*pValue > 100.0f) *pValue = 100.0f;
                                if (*pValue < 0.0f)   *pValue = 0.0f;
                            }
                        }
                        g_displayNeedsRefresh = 1;
                        break;

                    case KEY_DOWN:
                        /* 减少 */
                        if (pValue)
                        {
                            *pValue -= step;
                            if (g_sysMode == MODE_SET_TEMP_HIGH ||
                                g_sysMode == MODE_SET_TEMP_LOW)
                            {
                                if (*pValue < -40.0f) *pValue = -40.0f;
                                if (*pValue > 80.0f)  *pValue = 80.0f;
                            }
                            else
                            {
                                if (*pValue < 0.0f)   *pValue = 0.0f;
                                if (*pValue > 100.0f) *pValue = 100.0f;
                            }
                        }
                        g_displayNeedsRefresh = 1;
                        break;

                    case KEY_SET:
                        /* 切换到下一个设定项, 或退出设定模式 */
                        {
                            SystemMode_t nextMode = (SystemMode_t)(g_sysMode + 1);
                            if (nextMode >= MODE_MAX)
                            {
                                /* 所有项设定完毕 → 保存并退出 */
                                Config_Save(&g_threshold);
                                Mode_ExitSet();
                            }
                            else
                            {
                                g_sysMode = nextMode;
                                g_displayNeedsRefresh = 1;
                            }
                        }
                        Buzzer_KeyBeep();
                        break;

                    default: break;
                }
            }
            else if (event == KEY_EVENT_LONG_PRESS)
            {
                if (key == KEY_SET)
                {
                    /* 长按SET → 保存并退出设定模式 */
                    Config_Save(&g_threshold);
                    Mode_ExitSet();
                    Buzzer_KeyBeep();
                }
            }
            break;
        }

        default:
            g_sysMode = MODE_NORMAL;
            break;
    }
}

/**
 * @brief 进入阀值设定模式
 */
static void Mode_EnterSet(void)
{
    g_sysMode = MODE_SET_TEMP_HIGH;   // 从温度上限开始
    g_modeBlink = 1;
    g_displayNeedsRefresh = 1;
    Buzzer_KeyBeep();
}

/**
 * @brief 退出阀值设定模式, 返回正常监测
 */
static void Mode_ExitSet(void)
{
    g_sysMode = MODE_NORMAL;
    g_modeBlink = 0;
    g_displayNeedsRefresh = 1;
    Buzzer_KeyBeep();
}

/* ======================== UART 初始化 ======================== */

/**
 * @brief UART1 初始化 (PA9=TX, PA10=RX)
 *        波特率: 115200, 8数据位, 1停止位, 无校验
 */
void USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能时钟 */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX (复用推挽输出) */
    GPIO_InitStruct.Pin       = GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA10 = RX (浮空输入) */
    GPIO_InitStruct.Pin       = GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* UART1 参数配置 */
    DEBUG_UART.Instance          = USART1;
    DEBUG_UART.Init.BaudRate     = 115200;
    DEBUG_UART.Init.WordLength   = UART_WORDLENGTH_8B;
    DEBUG_UART.Init.StopBits     = UART_STOPBITS_1;
    DEBUG_UART.Init.Parity       = UART_PARITY_NONE;
    DEBUG_UART.Init.Mode         = UART_MODE_TX_RX;
    DEBUG_UART.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    DEBUG_UART.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&DEBUG_UART);
}

/* ======================== UART 接收中断回调 ======================== */

/**
 * @brief UART 接收完成回调 (HAL库中断中调用)
 *        单字节接收模式: 每收到1字节触发一次
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint8_t rxByte;
        HAL_UART_Receive(&DEBUG_UART, &rxByte, 1, 0);
        Protocol_RxHandler(rxByte);

        /* 重新使能接收中断 */
        __HAL_UART_ENABLE_IT(&DEBUG_UART, UART_IT_RXNE);
    }
}

/* ======================== SysTick 中断处理 ======================== */

/**
 * @brief SysTick 中断处理 (HAL库需要)
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ======================== HardFault 异常处理 ======================== */

/**
 * @brief HardFault 异常处理 (调试用)
 */
void HardFault_Handler(void)
{
    /* 死循环, 方便调试器定位问题 */
    while (1)
    {
        /* 可在此处设置断点 */
    }
}

/* ======================== 其他必要的中断处理 ======================== */
void NMI_Handler(void) {}
void MemManage_Handler(void) { while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void) { while(1); }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
