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

#include "dht22.h"  // Lin: 引入DHT22单总线传感器驱动接口定义

/* DWT 计数器使能标志 */
static uint8_t dwtEnabled = 0;  // DWT初始化标志位 [2026-06-19 Lin: 防止DWT重复初始化]

/**
 * @brief 启用 DWT 周期计数器 (用于精确微秒延时)
 *        Cortex-M3 DWT_CYCCNT 挂在系统时钟上 (72MHz → 1cycle=13.9ns)
 */
static void DWT_Init(void)
{
    if (dwtEnabled) return;  // 已初始化则跳过,避免重复配置

    /* 解锁 DWT 寄存器 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // 使能DWT跟踪模块 [CoreDebug->DEMCR bit24=TRCENA]
    /* 清零计数器 */
    DWT->CYCCNT = 0;  // 复位周期计数器初值为0
    /* 使能计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;  // 启动CYCCNT递增 [72MHz时钟下每周期≈13.9ns]
    dwtEnabled = 1;  // 标记已初始化 [2026-06-19 Lin]
}

/**
 * @brief DWT 微秒延时 (阻塞)
 * @param us 微秒数
 */
void DelayUs(uint32_t us)
{
    uint32_t startTick = DWT->CYCCNT;  // 记录起始计数值
    uint32_t delayTicks = us * (SystemCoreClock / 1000000);  // 微秒→时钟周期换算 [72MHz时1us=72tick]
    while ((DWT->CYCCNT - startTick) < delayTicks);  // 阻塞等待差值达到延时目标 [Lin: 精确微秒级延时]
}

/**
 * @brief 初始化 DHT22 引脚为开漏输出模式
 */
void DHT22_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();  // 使能GPIOA时钟 [DHT22数据线接PA0]

    /* 初始化为推挽输出, 拉高总线 */
    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;        // PA0引脚
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   // 开漏输出(兼容单总线)
    GPIO_InitStruct.Pull  = GPIO_NOPULL;           // 不使用内部上下拉[外部需4.7K上拉电阻]
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速模式[50MHz]确保时序精度
    HAL_GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStruct);

    /* 释放总线 (拉高) */
    DHT22_DATA_HIGH();  // 拉高PA0,总线进入空闲状态 [2026-06-19 Lin]

    /* 初始化DWT */
    DWT_Init();  // 启动DWT周期计数器为后续微秒延时做准备
}

/**
 * @brief 设置 DHT22 引脚为输入模式
 */
static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;   // PA0引脚
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;   // 输入模式[读取DHT22数据]
    GPIO_InitStruct.Pull  = GPIO_PULLUP;       // 内部上拉[增强信号稳定性]
    HAL_GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief 设置 DHT22 引脚为输出模式
 */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = DHT22_GPIO_PIN;        // PA0引脚
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   // 开漏输出[主机驱动总线]
    GPIO_InitStruct.Pull  = GPIO_NOPULL;           // 外部上拉已提供高电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速模式确保us级时序 [2026-06-19 Lin]
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
    while (DHT22_DATA_READ() != level)  // 循环检测引脚电平是否达到期望值
    {
        if (count > timeoutUs)  // 超出预设超时时间
        {
            return 0;  // 超时返回0 [DHT22通信异常]
        }
        DelayUs(1);  // 每1us轮询一次电平状态 [精确计时]
        count++;
    }
    return 1;  // 成功检测到期望电平
}

/**
 * @brief 读取 DHT22 一字节 (8bit)
 */
static uint8_t DHT22_ReadByte(void)
{
    uint8_t byte = 0;  // 接收缓冲区[逐bit填充]

    for (uint8_t i = 0; i < 8; i++)  // 8bit串行读取
    {
        byte <<= 1;  // 左移腾出最低位空间 [高位先出]

        /* 等待50us低电平结束 */
        if (!DHT22_WaitLevel(GPIO_PIN_SET, DHT22_TIMEOUT)) return 0;  // 超时则通信失败

        /* 延时40us后采样 (判断高电平持续时间) */
        DelayUs(40);  // 40us是'0'和'1'的区分阈值 [Lin: 26~28us vs 70us的中间点]

        /* 如果40us后还是高电平 → bit='1' (高电平持续70us) */
        /* 如果40us后变低电平 → bit='0' (高电平持续26~28us) */
        if (DHT22_DATA_READ() == GPIO_PIN_SET)
        {
            byte |= 0x01;  // 最低位置1 [当前bit=1]

            /* 等待高电平结束 */
            if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT)) return 0;
        }
        // else: 当前bit=0, byte最低位保持0(已由左移清空),无需额外操作
    }
    return byte;  // 返回完整一字节
}

/**
 * @brief 读取 DHT22 温湿度数据
 * @param temperature 输出: 温度 °C
 * @param humidity    输出: 湿度 %RH
 * @return 1=成功, 0=失败
 */
uint8_t DHT22_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};      // 40bit缓冲区: [湿度高,湿度低,温度高,温度低,校验]
    uint16_t rawHumidity;       // 原始湿度值[分辨率0.1%RH]
    uint16_t rawTemperature;    // 原始温度值[分辨率0.1°C]

    /* ======== 步骤1: 主机发送起始信号 ======== */
    DHT22_SetOutput();           // 切换到输出模式[主机驱动总线]

    DHT22_DATA_LOW();            // 拉低总线 [准备发送起始信号]
    DelayUs(DHT22_START_LOW);   // 保持低电平>1ms [DHT22检测到>1ms低电平才会唤醒]
    DHT22_DATA_HIGH();          // 拉高释放总线 [起始信号结束标志]
    DelayUs(DHT22_START_HIGH);  // 等待30us后切换为输入 [Lin: DHT22会在此后拉低响应]

    /* ======== 步骤2: 切换为输入, 等待从机响应 ======== */
    DHT22_SetInput();  // 释放总线控制权,准备接收DHT22应答 [2026-06-19]

    /* 等待从机拉低 (响应信号 ~80us) */
    if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT))
    {
        return 0;  // 无响应 [检查接线和上拉电阻]
    }

    /* 等待从机拉高 (响应信号结束 ~80us) */
    if (!DHT22_WaitLevel(GPIO_PIN_SET, DHT22_TIMEOUT))
    {
        return 0;  // 响应异常 [传感器可能损坏]
    }

    /* 等待从机再次拉低 (数据开始标志) */
    if (!DHT22_WaitLevel(GPIO_PIN_RESET, DHT22_TIMEOUT))
    {
        return 0;  // 数据段启动失败
    }

    /* ======== 步骤3: 读取40bit数据 (5字节) ======== */
    for (uint8_t i = 0; i < 5; i++)
    {
        data[i] = DHT22_ReadByte();  // 逐字节读取 [每字节8bit,高位先出]
    }

    /* ======== 步骤4: 校验和验证 ========
     * 校验公式: 湿度高8 + 湿度低8 + 温度高8 + 温度低8 = 校验和 (低8位)
     */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];  // 计算前4字节之和
    if (checksum != data[4])  // 与第5字节校验值比对 [2026-06-19 Lin]
    {
        return 0;  // 校验失败 [传输干扰或传感器数据错误]
    }

    /* ======== 步骤5: 解析数据 ========
     * 湿度: data[0]<<8 | data[1] → 除以10得到实际 %RH
     * 温度: data[2]<<8 | data[3] → 除以10得到实际 °C
     *   - 最高位=1 表示负温度 (bit15为符号位)
     */
    rawHumidity    = (data[0] << 8) | data[1];  // 拼接湿度高/低字节 [16bit原始值]
    rawTemperature = (data[2] << 8) | data[3];  // 拼接温度高/低字节 [16bit原始值]

    *humidity    = rawHumidity / 10.0f;  // 转换为实际百分比 [数据手册规定]

    /* 处理负温度 (bit15 = 1 表示零下) */
    if (rawTemperature & 0x8000)  // 检测温度符号位 [bit15=1→负温]
    {
        rawTemperature &= 0x7FFF;  // 清除符号位取绝对值
        *temperature = -(rawTemperature / 10.0f);  // 添加负号 [Lin: -40°C~0°C范围]
    }
    else
    {
        *temperature = rawTemperature / 10.0f;  // 正温度直接转换 [0°C~80°C范围]
    }

    return 1;  // 读取成功 [数据有效]
}
