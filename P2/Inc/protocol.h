/**
 * @file    protocol.h
 * @brief   串口通信协议模块
 *
 * 协议格式:
 *   | Sync1 | Sync2 | CMD | Length | Data[0..N] | Checksum |
 *   | 0xAA  | 0x55  | 1B  | 1B     | N bytes    | 1B       |
 *
 *   Checksum = Sync1 ^ Sync2 ^ CMD ^ Length ^ Data[0] ^ ... ^ Data[N-1]
 *
 * 命令字:
 *   0x01 - 读取温湿度 (上位机 → 设备)
 *   0x02 - 读取阀值配置
 *   0x03 - 设置阀值
 *   0x04 - 报警通知 (设备 → 上位机, 主动上报)
 *   0x80 - ACK 应答
 *   0xFF - NACK 错误
 *
 * 接线: UART1 → PA9(TX), PA10(RX), 波特率 115200-8-N-1
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "main.h"

/* 帧缓冲区大小 */
#define PROTOCOL_RX_BUF_SIZE    64
#define PROTOCOL_TX_BUF_SIZE    64

/* 协议状态机 */
typedef enum {
    PROTO_STATE_SYNC1 = 0,    // 等待同步头1
    PROTO_STATE_SYNC2,        // 等待同步头2
    PROTO_STATE_CMD,          // 接收命令字
    PROTO_STATE_LENGTH,       // 接收数据长度
    PROTO_STATE_DATA,         // 接收数据
    PROTO_STATE_CHECKSUM,     // 校验
} ProtoState_t;

/**
 * @brief 初始化协议模块 (UART1已由HAL初始化)
 */
void Protocol_Init(void);

/**
 * @brief 处理接收到的字节 (在UART接收中断回调中调用)
 * @param byte 接收到的单字节
 */
void Protocol_RxHandler(uint8_t byte);

/**
 * @brief 主动上报当前温湿度数据
 * @param data 传感器数据指针
 */
void Protocol_UploadData(SensorData_t *data);

/**
 * @brief 主动上报报警信息
 * @param type 报警类型
 * @param value 当前超限数值 (温度或湿度)
 */
void Protocol_UploadAlarm(AlarmType_t type, float value);

/**
 * @brief 发送阀值配置应答
 * @param thr 当前阀值配置
 */
void Protocol_SendThreshold(Threshold_t *thr);

/**
 * @brief 发送ACK
 */
void Protocol_SendAck(void);

/**
 * @brief 发送NACK
 * @param errorCode 错误码
 */
void Protocol_SendNack(uint8_t errorCode);

/**
 * @brief 协议任务处理 (放在主循环中调用)
 */
void Protocol_Process(void);

#endif /* __PROTOCOL_H */
