/**
 * @file    oled.h
 * @brief   SSD1306 OLED 128x64 驱动 (软件I2C)
 *
 * 特性:
 *   - 分辨率: 128x64 像素
 *   - 接口: 软件模拟I2C (SCL:PB6, SDA:PB7)
 *   - 支持: 6x8/8x16 ASCII字符, 16x16 中文汉字
 *   - 支持: 任意位置画点、画线、画矩形、显示图片
 *
 * 接线:
 *   SCL → PB6
 *   SDA → PB7
 *   VCC → 3.3V
 *   GND → GND
 */

#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* OLED 分辨率 */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          8       // 64/8 = 8页

/* ======================== 基础API ======================== */

/** 初始化 OLED */
void OLED_Init(void);

/** 清屏 */
void OLED_Clear(void);

/** 全屏填充 */
void OLED_Fill(uint8_t data);

/** 刷新显存到屏幕 */
void OLED_Refresh(void);

/** 设置光标位置 (页地址, 列地址) */
void OLED_SetCursor(uint8_t page, uint8_t col);

/** 在指定位置画点 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);

/** 画线 (Bresenham算法) */
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/** 画矩形 */
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/* ======================== 字符显示API ======================== */

/** 显示一个 6x8 ASCII 字符 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size);

/** 显示字符串 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);

/** 显示一个 16x16 汉字 (内码索引) */
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index);

/** 显示格式化字符串 (简易,限制64字节) */
void OLED_Printf(uint8_t x, uint8_t y, uint8_t size, const char *fmt, ...);

/** 显示有符号整数 */
void OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);

/** 显示浮点数 (保留1位小数) */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t intLen, uint8_t decLen, uint8_t size);

/* ======================== 界面布局API ======================== */

/** 显示主界面 - 温湿度实时数据 */
void OLED_ShowMainUI(float temp, float humi, uint8_t alarm);

/** 显示阀值设定界面 */
void OLED_ShowSetUI(SystemMode_t mode, Threshold_t *thr);

/** 显示开机Logo */
void OLED_ShowLogo(void);

#endif /* __OLED_H */
