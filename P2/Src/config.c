/**
 * @file    config.c
 * @brief   阀值配置 Flash 存储管理
 *
 * STM32F103C8T6:
 *   - Flash: 64KB, 每页 1KB
 *   - 使用最后一页 (0x0800FC00) 存储配置
 *   - 存储格式: [魔数 2B][阀值数据 16B][CRC16 2B] = 20字节
 *
 * 写入前需擦除整页, 所以只适合保存少量配置数据.
 *
 * 注意: HAL Flash 操作会禁用中断, 写入期间可能影响系统实时性.
 *        写入耗时约 40ms (页擦除) + 编程时间.
 */

#include "config.h"

/* Flash 存储魔数 (用于判断是否已初始化) */
#define CONFIG_MAGIC            0xA5C3

/* 存储结构体 */
typedef struct {
    uint16_t magic;             // 魔数 2B
    float    temp_high;         // 温度上限 4B
    float    temp_low;          // 温度下限 4B
    float    humi_high;         // 湿度上限 4B
    float    humi_low;          // 湿度下限 4B
    uint16_t crc16;             // CRC16 校验 2B
} FlashConfig_t;  // 共 20 字节

/**
 * @brief 简易 CRC16 (Modbus 多项式)
 */
static uint16_t CRC16_Calc(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/**
 * @brief 从 Flash 加载阀值配置
 * @param thr 输出: 阀值
 * @return 1=成功, 0=失败(配置无效,使用默认值)
 */
uint8_t Config_Load(Threshold_t *thr)
{
    FlashConfig_t flashCfg;

    /* 读取 Flash 存储区域 */
    uint32_t *src = (uint32_t *)FLASH_CONFIG_ADDR;
    uint32_t *dst = (uint32_t *)&flashCfg;

    for (uint8_t i = 0; i < sizeof(FlashConfig_t) / 4; i++)
    {
        dst[i] = src[i];
    }

    /* 检查魔数 */
    if (flashCfg.magic != CONFIG_MAGIC)
    {
        Config_ResetDefault(thr);
        return 0;
    }

    /* CRC16 校验 (不包含CRC字段本身) */
    uint16_t crcCalc = CRC16_Calc((uint8_t *)&flashCfg, sizeof(FlashConfig_t) - 2);
    if (crcCalc != flashCfg.crc16)
    {
        Config_ResetDefault(thr);
        return 0;
    }

    /* 数据有效, 加载 */
    thr->temp_high = flashCfg.temp_high;
    thr->temp_low  = flashCfg.temp_low;
    thr->humi_high = flashCfg.humi_high;
    thr->humi_low  = flashCfg.humi_low;

    return 1;
}

/**
 * @brief 保存阀值配置到 Flash
 * @param thr 阀值
 * @return 1=成功, 0=失败
 */
uint8_t Config_Save(Threshold_t *thr)
{
    FlashConfig_t flashCfg;
    HAL_StatusTypeDef status;
    uint32_t pageError = 0;

    /* 构造存储结构体 */
    flashCfg.magic     = CONFIG_MAGIC;
    flashCfg.temp_high = thr->temp_high;
    flashCfg.temp_low  = thr->temp_low;
    flashCfg.humi_high = thr->humi_high;
    flashCfg.humi_low  = thr->humi_low;
    flashCfg.crc16     = CRC16_Calc((uint8_t *)&flashCfg, sizeof(FlashConfig_t) - 2);

    /* 解锁 Flash */
    HAL_FLASH_Unlock();

    /* 擦除最后一页 */
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase   = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = FLASH_CONFIG_ADDR;
    eraseInit.NbPages     = 1;

    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0;
    }

    /* 写入数据 (按32位字写入) */
    uint32_t *src = (uint32_t *)&flashCfg;
    uint32_t addr = FLASH_CONFIG_ADDR;

    for (uint8_t i = 0; i < sizeof(FlashConfig_t) / 4; i++)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0;
        }
        addr += 4;
    }

    /* 锁定 Flash */
    HAL_FLASH_Lock();

    return 1;
}

/**
 * @brief 恢复默认阀值
 */
void Config_ResetDefault(Threshold_t *thr)
{
    thr->temp_high = TEMP_HIGH_DEFAULT;
    thr->temp_low  = TEMP_LOW_DEFAULT;
    thr->humi_high = HUMI_HIGH_DEFAULT;
    thr->humi_low  = HUMI_LOW_DEFAULT;
}
