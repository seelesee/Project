/**
 * @file    stm32f1xx_hal_conf.h
 * @brief   HAL 库模块配置头文件
 *
 * 本文件用于启用/禁用 HAL 库的各个外设驱动模块。
 * 禁用不需要的模块可以减小固件体积。
 */

#ifndef __STM32F1XX_HAL_CONF_H
#define __STM32F1XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 模块启用开关 ==================== */
/* 注释掉不需要的模块以减小编译体积 */

#define HAL_MODULE_ENABLED
// #define HAL_ADC_MODULE_ENABLED
// #define HAL_CAN_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
// #define HAL_CRC_MODULE_ENABLED
// #define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
// #define HAL_I2C_MODULE_ENABLED
// #define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
// #define HAL_RTC_MODULE_ENABLED
// #define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
// #define HAL_WWDG_MODULE_ENABLED

/* ==================== 时钟配置 ==================== */
#define HSE_VALUE            8000000UL   /* 外部高速晶振 8MHz */
#define HSE_STARTUP_TIMEOUT  100U
#define HSI_VALUE            8000000UL   /* 内部高速 RC 8MHz */
#define LSE_VALUE            32768UL     /* 外部低速晶振 (未使用) */
#define LSI_VALUE            40000UL     /* 内部低速 RC (未使用) */
#define EXTERNAL_CLOCK_VALUE 72000000UL  /* 系统时钟 72MHz */

/* ==================== HAL 断言 ==================== */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr)  ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr)  ((void)0U)
#endif /* USE_FULL_ASSERT */

/* ==================== 外设中断优先级 ==================== */
#define TICK_INT_PRIORITY             0x0FU  /* SysTick (最低) */
#define UART_PRIORITY                 0x01U  /* UART1 */
#define EXTI_PRIORITY                 0x00U  /* 外部中断 (最高) */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1XX_HAL_CONF_H */
