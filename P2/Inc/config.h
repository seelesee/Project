/**
 * @file    config.h
 * @brief   阀值配置管理模块
 *
 * 功能:
 *   - 阀值存储到 STM32 内部 Flash (最后1页)
 *   - 上电自动加载
 *   - 默认值恢复
 *
 * STM32F103C8T6 Flash: 64KB, 每页1KB, 最后一页 0x0800FC00
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "main.h"

/* STM32F103C8T6 Flash 最后一页起始地址 */
#define FLASH_CONFIG_ADDR       0x0800FC00
#define FLASH_PAGE_SIZE         1024    // 1KB

/**
 * @brief 从Flash加载阀值配置
 * @param thr 输出: 阀值结构体
 * @return 1=成功, 0=校验失败(使用默认值)
 */
uint8_t Config_Load(Threshold_t *thr);

/**
 * @brief 保存阀值配置到Flash
 * @param thr 阀值结构体
 * @return 1=成功, 0=失败
 */
uint8_t Config_Save(Threshold_t *thr);

/**
 * @brief 恢复默认阀值
 * @param thr 输出: 默认阀值
 */
void Config_ResetDefault(Threshold_t *thr);

#endif /* __CONFIG_H */
