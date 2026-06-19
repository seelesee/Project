/**
 * @file    oled.c
 * @brief   SSD1306 OLED 0.96寸 128x64 驱动实现 (软件I2C)
 *
 * 软件I2C时序:
 *   - 起始: SCL高 → SDA下降沿
 *   - 停止: SCL高 → SDA上升沿
 *   - 写字节: SCL低→放SDA→SCL高(锁存)→SCL低, 高位先发
 *   - 等待ACK: 第9个SCL脉冲后检测SDA低电平
 *
 * SSD1306 显示模式: 页寻址模式
 *   128列 × 8页 × 8行/页 = 128×64
 */

#include "oled.h"
#include "oled_font.h"
#include <stdarg.h>

/* ==================== 软件I2C 宏定义 ==================== */
#define SCL_H()  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)
#define SDA_IN() HAL_GPIO_ReadPin(OLED_SDA_PORT, OLED_SDA_PIN)

/* 全屏显存: 128列 × 8页 = 1024 字节 */
static uint8_t OLED_Buffer[OLED_WIDTH * OLED_PAGES];

/* ==================== 软件I2C 底层 ==================== */

/**
 * @brief 软件I2C 延时 (约4us @72MHz)
 */
static void I2C_Delay(void)
{
    /* 约 4us @72MHz, 根据实际晶振微调 */
    for (volatile uint8_t i = 0; i < 20; i++);
}

/**
 * @brief I2C 起始信号
 */
static void I2C_Start(void)
{
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

/**
 * @brief I2C 停止信号
 */
static void I2C_Stop(void)
{
    SCL_L();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
}

/**
 * @brief I2C 发送一字节
 */
static void I2C_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        SCL_L();
        I2C_Delay();

        if (byte & 0x80)
            SDA_H();
        else
            SDA_L();

        I2C_Delay();
        SCL_H();
        I2C_Delay();
        byte <<= 1;
    }
    SCL_L();
    I2C_Delay();
}

/**
 * @brief I2C 等待ACK (可选, 不等待也不影响SSD1306)
 */
static void I2C_WaitAck(void)
{
    SDA_H();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

/* ==================== SSD1306 命令/数据发送 ==================== */

/**
 * @brief 发送命令字节
 *       I2C地址 + 控制字节(0x00=命令) + 命令字节
 */
static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_I2C_ADDR);   // I2C地址 (写模式, bit0=0)
    I2C_WaitAck();
    I2C_WriteByte(0x00);            // 控制字节: 0x00=后续是命令
    I2C_WaitAck();
    I2C_WriteByte(cmd);             // 命令字节
    I2C_WaitAck();
    I2C_Stop();
}

/**
 * @brief 发送数据字节
 *       I2C地址 + 控制字节(0x40=数据) + 数据字节
 */
static void OLED_WriteData(uint8_t dat)
{
    I2C_Start();
    I2C_WriteByte(OLED_I2C_ADDR);
    I2C_WaitAck();
    I2C_WriteByte(0x40);            // 控制字节: 0x40=后续是数据
    I2C_WaitAck();
    I2C_WriteByte(dat);
    I2C_WaitAck();
    I2C_Stop();
}

/**
 * @brief 批量发送数据 (提高刷新效率)
 */
static void OLED_WriteDataBurst(uint8_t *data, uint16_t len)
{
    I2C_Start();
    I2C_WriteByte(OLED_I2C_ADDR);
    I2C_WaitAck();
    I2C_WriteByte(0x40);
    I2C_WaitAck();

    for (uint16_t i = 0; i < len; i++)
    {
        I2C_WriteByte(data[i]);
        I2C_WaitAck();
    }
    I2C_Stop();
}

/* ==================== OLED 初始化 ==================== */

/**
 * @brief 初始化 OLED 引脚和 SSD1306 控制器
 */
void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能GPIO时钟 */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* SCL: PB6, SDA: PB7 — 推挽输出 */
    GPIO_InitStruct.Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(OLED_SCL_PORT, &GPIO_InitStruct);

    /* 初始电平 */
    SCL_H();
    SDA_H();

    /* 延时等待OLED上电稳定 */
    HAL_Delay(100);

    /* ===== SSD1306 初始化序列 ===== */

    OLED_WriteCmd(0xAE);  // 关闭显示

    OLED_WriteCmd(0x00);  // 列低地址
    OLED_WriteCmd(0x10);  // 列高地址
    OLED_WriteCmd(0x40);  // 显示起始行

    OLED_WriteCmd(0x81);  // 对比度
    OLED_WriteCmd(0xCF);  // 对比度值 (0~255, 默认0x7F)

    OLED_WriteCmd(0xA1);  // 列重映射 (左右翻转: A0/A1)
    OLED_WriteCmd(0xC8);  // 行重映射 (上下翻转: C0/C8)

    OLED_WriteCmd(0xA6);  // 正常显示 (A7=反白)

    OLED_WriteCmd(0xA8);  // 多路复用比
    OLED_WriteCmd(0x3F);  // 1/64 duty

    OLED_WriteCmd(0xD3);  // 显示偏移
    OLED_WriteCmd(0x00);

    OLED_WriteCmd(0xD5);  // 显示时钟分频/振荡器频率
    OLED_WriteCmd(0x80);

    OLED_WriteCmd(0xD9);  // 预充电周期
    OLED_WriteCmd(0xF1);

    OLED_WriteCmd(0xDA);  // COM引脚硬件配置
    OLED_WriteCmd(0x12);

    OLED_WriteCmd(0xDB);  // VCOMH 电压
    OLED_WriteCmd(0x40);

    OLED_WriteCmd(0x8D);  // 电荷泵使能
    OLED_WriteCmd(0x14);

    OLED_WriteCmd(0xAF);  // 开启显示

    /* 清空显存和屏幕 */
    OLED_Clear();
    OLED_Refresh();
}

/* ==================== 基础绘图 ==================== */

/**
 * @brief 清空显存缓冲区
 */
void OLED_Clear(void)
{
    memset(OLED_Buffer, 0x00, sizeof(OLED_Buffer));
}

/**
 * @brief 全屏填充
 */
void OLED_Fill(uint8_t data)
{
    memset(OLED_Buffer, data, sizeof(OLED_Buffer));
}

/**
 * @brief 将显存刷新到 OLED
 */
void OLED_Refresh(void)
{
    /* 设置为页寻址模式 */
    OLED_WriteCmd(0x20);  // 内存寻址模式
    OLED_WriteCmd(0x02);  // 页寻址模式

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        OLED_WriteCmd(0xB0 + page);   // 设置页地址
        OLED_WriteCmd(0x00);          // 列低地址
        OLED_WriteCmd(0x10);          // 列高地址

        /* 发送整页数据 (128字节) */
        OLED_WriteDataBurst(&OLED_Buffer[page * OLED_WIDTH], OLED_WIDTH);
    }
}

/**
 * @brief 画点
 * @param x     横坐标 (0~127)
 * @param y     纵坐标 (0~63)
 * @param color 1=点亮, 0=熄灭
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    uint8_t page  = y / 8;
    uint8_t bit   = y % 8;
    uint16_t idx  = page * OLED_WIDTH + x;

    if (color)
        OLED_Buffer[idx] |= (1 << bit);
    else
        OLED_Buffer[idx] &= ~(1 << bit);
}

/**
 * @brief 画线 (Bresenham 算法)
 */
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
    int16_t dx = abs((int16_t)x1 - (int16_t)x0);
    int16_t dy = -abs((int16_t)y1 - (int16_t)y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (1)
    {
        OLED_DrawPoint(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/**
 * @brief 画矩形 (空心)
 */
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    OLED_DrawLine(x, y, x + w - 1, y, color);           // 上
    OLED_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // 下
    OLED_DrawLine(x, y, x, y + h - 1, color);           // 左
    OLED_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // 右
}

/* ==================== 字符显示 ==================== */

/**
 * @brief 显示 6x8 ASCII 字符
 * @param x   横坐标 (0~127)
 * @param y   纵坐标 (0~55, 8的倍数效果最好)
 * @param ch  字符
 * @param size 1=6x8, 2=12x16(放大)
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size)
{
    if (ch < ' ' || ch > '~') ch = ' ';   // 不可显示字符替换为空格
    uint8_t idx = ch - ' ';

    for (uint8_t i = 0; i < 6; i++)
    {
        uint8_t line = Font6x8[idx][i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (line & (1 << j))
            {
                if (size == 1)
                {
                    OLED_DrawPoint(x + i, y + j, 1);
                }
                else
                {
                    /* 放大2倍: 每个点变2×2 */
                    OLED_DrawPoint(x + i * 2,     y + j * 2,     1);
                    OLED_DrawPoint(x + i * 2 + 1, y + j * 2,     1);
                    OLED_DrawPoint(x + i * 2,     y + j * 2 + 1, 1);
                    OLED_DrawPoint(x + i * 2 + 1, y + j * 2 + 1, 1);
                }
            }
        }
    }
}

/**
 * @brief 显示字符串 (自动换行)
 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    uint8_t charWidth = (size == 1) ? 6 : 12;
    uint8_t charHeight = (size == 1) ? 8 : 16;

    while (*str != '\0')
    {
        if (*str == '\n')
        {
            x = 0;
            y += charHeight;
            str++;
            continue;
        }

        /* 超出屏幕宽度则换行 */
        if (x + charWidth > OLED_WIDTH)
        {
            x = 0;
            y += charHeight;
        }

        /* 超出屏幕高度则停止 */
        if (y + charHeight > OLED_HEIGHT) break;

        OLED_ShowChar(x, y, *str, size);
        x += charWidth;
        str++;
    }
}

/**
 * @brief 显示 16x16 中文字符
 * @param x     横坐标 (0~112)
 * @param y     纵坐标 (0~48)
 * @param index 汉字在 Chinese16x16 数组中的索引
 */
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index)
{
    for (uint8_t i = 0; i < 16; i++)
    {
        uint8_t highByte = Chinese16x16[index][i * 2];
        uint8_t lowByte  = Chinese16x16[index][i * 2 + 1];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (highByte & (0x80 >> j))
                OLED_DrawPoint(x + j, y + i, 1);
            if (lowByte & (0x80 >> j))
                OLED_DrawPoint(x + j + 8, y + i, 1);
        }
    }
}

/**
 * @brief OLED 格式化输出 (简易版, 限制64字符)
 */
void OLED_Printf(uint8_t x, uint8_t y, uint8_t size, const char *fmt, ...)
{
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OLED_ShowString(x, y, buf, size);
}

/**
 * @brief 显示整数 (右对齐)
 * @param len 显示位数 (不足补空格)
 */
void OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%*ld", len, (long)num);
    /* snprintf 右对齐会补空格, 直接显示 */
    OLED_ShowString(x, y, buf, size);
}

/**
 * @brief 显示浮点数 (保留decLen位小数)
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t intLen, uint8_t decLen, uint8_t size)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%*.*f", intLen + decLen + 1, decLen, num);
    OLED_ShowString(x, y, buf, size);
}

/* ==================== 界面布局 ==================== */

/**
 * @brief 显示开机Logo
 */
void OLED_ShowLogo(void)
{
    OLED_Clear();

    /* 第1行: 标题 */
    OLED_ShowChinese(16, 0,  FONT_WEN);    // 温
    OLED_ShowChinese(32, 0,  FONT_SHI);    // 湿
    OLED_ShowChinese(48, 0,  FONT_DU);     // 度
    OLED_ShowChinese(64, 0,  FONT_JIAN);   // 监
    OLED_ShowChinese(80, 0,  FONT_CE);     // 测
    OLED_ShowChinese(96, 0,  FONT_XI);     // 系
    OLED_ShowChinese(112,0,  FONT_TONG);   // 统

    /* 第2行: 版本信息 */
    OLED_ShowString(20, 20, "STM32F103C8T6", 1);
    OLED_ShowString(30, 32, "DHT22 + OLED", 1);

    /* 第3行: 初始化提示 */
    OLED_ShowString(10, 48, "Initializing...", 1);

    OLED_Refresh();
}

/**
 * @brief 显示主监测界面
 * @param temp  温度值
 * @param humi  湿度值
 * @param alarm 是否有报警 (1=有报警闪烁)
 */
void OLED_ShowMainUI(float temp, float humi, uint8_t alarm)
{
    OLED_Clear();

    /* ===== 顶部栏: 系统标题 ===== */
    OLED_ShowChinese(0, 0, FONT_WEN);    // 温
    OLED_ShowChinese(16,0, FONT_SHI);    // 湿
    OLED_ShowChinese(32,0, FONT_DU);     // 度
    OLED_ShowChinese(48,0, FONT_JIAN);   // 监
    OLED_ShowChinese(64,0, FONT_CE);     // 测

    /* 报警指示 (反白闪烁) */
    if (alarm)
    {
        OLED_ShowChinese(80, 0,  FONT_BAO);   // 报 - actually let me use ASCII
        OLED_ShowChinese(96, 0,  FONT_JING);  // 警
    }
    else
    {
        OLED_ShowString(80, 0, "  OK  ", 1);
    }

    /* 分割线 */
    OLED_DrawLine(0, 17, 127, 17, 1);

    /* ===== 中部: 温度显示 ===== */
    OLED_ShowString(0, 20, "Temp:", 1);
    OLED_ShowFloat(36, 20, temp, 3, 1, 2);  // 显示温度值 (放大2倍)

    /* 温度单位 */
    OLED_ShowString(100, 22, "'C", 1);

    /* ===== 中部: 湿度显示 ===== */
    OLED_ShowString(0, 38, "Humi:", 1);
    OLED_ShowFloat(36, 38, humi, 3, 1, 2);  // 显示湿度值 (放大2倍)

    /* 湿度单位 */
    OLED_ShowString(102, 40, "%RH", 1);

    /* ===== 底部栏: 按键提示 ===== */
    OLED_DrawLine(0, 55, 127, 55, 1);
    OLED_ShowString(0, 56, "SET:Menu", 1);

    OLED_Refresh();
}

/**
 * @brief 显示阀值设定界面
 * @param mode 当前设定模式 (设定哪个阀值)
 * @param thr  阀值结构体指针
 */
void OLED_ShowSetUI(SystemMode_t mode, Threshold_t *thr)
{
    OLED_Clear();

    /* 标题 */
    OLED_ShowChinese(0, 0,  FONT_FA);     // 阀
    OLED_ShowChinese(16,0,  FONT_ZHI);    // 值
    OLED_ShowChinese(32,0,  FONT_SHE);    // 设
    OLED_ShowChinese(48,0,  FONT_DING);   // 定

    OLED_DrawLine(0, 17, 127, 17, 1);

    /* 根据模式显示对应阀值, 当前编辑项前面加 ">" 标记 */
    char prefix[2] = {' ', '\0'};

    /* 温度上限 */
    prefix[0] = (mode == MODE_SET_TEMP_HIGH) ? '>' : ' ';
    OLED_ShowString(0, 20, prefix, 1);
    OLED_ShowChinese(8, 20, FONT_WEN);
    OLED_ShowChinese(24,20, FONT_DU);
    OLED_ShowChinese(40,20, FONT_SHANG);
    OLED_ShowString(56,20, ":", 1);
    OLED_ShowFloat(64, 20, thr->temp_high, 3, 1, 1);

    /* 温度下限 */
    prefix[0] = (mode == MODE_SET_TEMP_LOW) ? '>' : ' ';
    OLED_ShowString(0, 30, prefix, 1);
    OLED_ShowChinese(8, 30, FONT_WEN);
    OLED_ShowChinese(24,30, FONT_DU);
    OLED_ShowChinese(40,30, FONT_XIA);
    OLED_ShowString(56,30, ":", 1);
    OLED_ShowFloat(64, 30, thr->temp_low, 3, 1, 1);

    /* 湿度上限 */
    prefix[0] = (mode == MODE_SET_HUMI_HIGH) ? '>' : ' ';
    OLED_ShowString(0, 40, prefix, 1);
    OLED_ShowChinese(8, 40, FONT_SHI);
    OLED_ShowChinese(24,40, FONT_DU);
    OLED_ShowChinese(40,40, FONT_SHANG);
    OLED_ShowString(56,40, ":", 1);
    OLED_ShowFloat(64, 40, thr->humi_high, 3, 1, 1);

    /* 湿度下限 */
    prefix[0] = (mode == MODE_SET_HUMI_LOW) ? '>' : ' ';
    OLED_ShowString(0, 50, prefix, 1);
    OLED_ShowChinese(8, 50, FONT_SHI);
    OLED_ShowChinese(24,50, FONT_DU);
    OLED_ShowChinese(40,50, FONT_XIA);
    OLED_ShowString(56,50, ":", 1);
    OLED_ShowFloat(64, 50, thr->humi_low, 3, 1, 1);

    /* 底部操作提示 */
    OLED_DrawLine(0, 63, 127, 63, 1);
    /* 第8页只有第64行, 放不下文字, 刷新后再用Printf画 */
    OLED_Refresh();

    /* 底部提示 (覆盖显示) */
    OLED_ShowString(0, 56, "UP+ DOWN- SET:OK", 1);

    OLED_Refresh();
}
