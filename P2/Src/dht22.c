/**
 * @file    dht22.c
 * @brief   DHT22 温湿度传感器驱动实现
 *
 * 工作原理:
 *   1. 主机发送起始信号: 拉低 >1ms, 拉高 30us 释放总线
 *   2. DHT22响应: 拉低 80us, 拉高 80us
 *   3. DHT22发送40bit数据: 16bit湿度 + 16bit温度 + 8bit校验
 *   4. 每bit: 50us低电平 + 26~28us高电平='0', 70us高电平='1'
 *
 * 注意: DHT22采样周期至少2秒, 两次读取之间需要间隔
 */

#include "dht22.h"

/* DWT 计数器使能标志 */
static uint8_t dwtEnabled = 0;

/**
 * @brief 启用 DWT 周期计数器 (用于精确微秒延时)
 *        Cortex-M3 DWT_CYCCNT 挂在系统时钟上 (72MHz → 1cycle=13.9ns)
 */
static void DWT_Init(void)
{
    if (dwtEnabled) return;

    /* 解锁 DWT 寄存器 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* 清零计数器 */
    DWT->CYCCNT = 0;
    /* 使能计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwtEnabled = 1;
}

/**
 * @brief DWT 微秒延时 (阻塞)
 * @param us 微秒数
 */
void DelayUs(uint32_t us)
{
    uint32_t startTick = DWT->CYCCNT;
    uint32_t delayTicks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - startTick) < delayTicks);
}

/**
 * @brief 初始化 DHT22 引脚为开漏输出模式
 */
void DHT22_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 初始化为推挽输出, 拉高总线 */
    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   // 开漏输出(兼容单总线)
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStruct);

    /* 释放总线 (拉高) */
    DHT22_DATA_HIGH();

    /* 初始化DWT */
    DWT_Init();
}

/**
 * @brief 设置 DHT22 引脚为输入模式
 */
static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief 设置 DHT22 引脚为输出模式
 */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief 等待引脚变为指定电平 (带超时)
 * @param level 期望电平
 * @param timeoutUs 超时时间 (us)
 * @return 1=成功, 0=超时
 */
static uint8_t DHT22_WaitLevel(uint8_t level, uint32_t timeoutUs)
{
    uint32_t count = 0;
    while (DHT22_DATA_READ() != level)
    {
        if (count > timeoutUs)
        {
            return 0;  // 超时
        }
        DelayUs(1);
        count++;
    }
    return 1;
}

/**
 * @brief 读取 DHT22 一字节 (8bit)
 */
static uint8_t DHT22_ReadByte(void)
{
    uint8_t byte = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;

        /* 等待50us低电平结束 */
        if (!DHT22_WaitLevel(GPIO_PIN_SET, DHT22_TIMEOUT)) return 0;

        /* 延时40us后采样 (判断高电平持续时间) */
        DelayUs(40);

        /* 如果40us后还是高电平 → bit='1' (高电平持续70us) */
        /* 如果40us后变低电平 → bit='0' (高电平持续26~28us) */
        if (DHT22_DATA_READ() == GPIO_PIN_SET)
        {
            byte |= 0x01;

            /* 等待高电平结束 */
            if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT)) return 0;
        }
    }
    return byte;
}

/**
 * @brief 读取 DHT22 温湿度数据
 * @param temperature 输出: 温度 °C
 * @param humidity    输出: 湿度 %RH
 * @return 1=成功, 0=失败
 */
uint8_t DHT22_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};      // [湿度高,湿度低,温度高,温度低,校验]
    uint16_t rawHumidity;
    uint16_t rawTemperature;

    /* 步骤1: 主机发送起始信号 */
    DHT22_SetOutput();

    DHT22_DATA_LOW();           // 拉低总线
    DelayUs(DHT22_START_LOW);   // 保持 >1ms
    DHT22_DATA_HIGH();          // 拉高释放
    DelayUs(DHT22_START_HIGH);  // 等待 30us

    /* 步骤2: 切换为输入, 等待从机响应 */
    DHT22_SetInput();

    /* 等待从机拉低 (响应信号) */
    if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT))
    {
        return 0;  // 无响应
    }

    /* 等待从机拉高 (响应信号结束) */
    if (!DHT22_WaitLevel(GPIO_PIN_SET, DHT22_TIMEOUT))
    {
        return 0;  // 无响应
    }

    /* 等待从机再次拉低 (数据开始) */
    if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT))
    {
        return 0;
    }

    /* 步骤3: 读取40bit数据 (5字节) */
    for (uint8_t i = 0; i < 5; i++)
    {
        data[i] = DHT22_ReadByte();
    }

    /* 步骤4: 校验和验证
     * 校验和 = 湿度高8 + 湿度低8 + 温度高8 + 温度低8 (取低8位)
     */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4])
    {
        return 0;  // 校验失败
    }

    /* 步骤5: 解析数据
     * 湿度: data[0]<<8 | data[1] → 除以10得到 %RH
     * 温度: data[2]<<8 | data[3] → 除以10得到 °C
     *   - 如果最高位=1, 表示负温度
     */
    rawHumidity    = (data[0] << 8) | data[1];
    rawTemperature = (data[2] << 8) | data[3];

    *humidity    = rawHumidity / 10.0f;

    /* 处理负温度 (bit15 = 1) */
    if (rawTemperature & 0x8000)
    {
        rawTemperature &= 0x7FFF;
        *temperature = -(rawTemperature / 10.0f);
    }
    else
    {
        *temperature = rawTemperature / 10.0f;
    }

    return 1;  // 成功
}
