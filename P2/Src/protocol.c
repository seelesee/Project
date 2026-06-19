/**
 * @file    protocol.c
 * @brief   串口通信协议实现
 *
 * 帧格式:
 *   | 0xAA | 0x55 | CMD | LEN | DATA[0..LEN-1] | CHECKSUM |
 *   |  1B  |  1B  | 1B  | 1B  |    N bytes     |    1B    |
 *
 * 校验和 = 所有字节异或 (SYNC1 ^ SYNC2 ^ CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[N-1])
 *
 * 数据上传帧 (CMD=0x01 应答):
 *   DATA = temp_high | temp_low | humi_high | humi_low | valid_flag
 *   温度/湿度 = (high<<8 | low) / 10.0
 *
 * 报警通知帧 (CMD=0x04):
 *   DATA = alarm_type(1B) | value_high(1B) | value_low(1B)
 */

#include "protocol.h"

/* 接收缓冲区 */
static uint8_t g_rxBuf[PROTOCOL_RX_BUF_SIZE];
static uint8_t g_rxIndex = 0;
static uint8_t g_rxLength = 0;

/* 协议状态机 */
static ProtoState_t g_protoState = PROTO_STATE_SYNC1;

/* 当前接收帧信息 */
static uint8_t g_frameCmd    = 0;
static uint8_t g_frameLength = 0;
static uint8_t g_frameData[32];

/* 接收完成标志 */
static volatile uint8_t g_frameReady = 0;

/**
 * @brief 初始化协议模块
 */
void Protocol_Init(void)
{
    g_protoState = PROTO_STATE_SYNC1;
    g_rxIndex    = 0;
    g_frameReady = 0;
    memset(g_frameData, 0, sizeof(g_frameData));
}

/**
 * @brief 计算校验和
 */
static uint8_t CalcChecksum(uint8_t sync1, uint8_t sync2, uint8_t cmd,
                             uint8_t len, uint8_t *data)
{
    uint8_t checksum = sync1 ^ sync2 ^ cmd ^ len;
    for (uint8_t i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief 发送一帧数据
 */
static void Protocol_SendFrame(uint8_t cmd, uint8_t *data, uint8_t len)
{
    uint8_t txBuf[PROTOCOL_TX_BUF_SIZE];
    uint8_t txLen = 0;

    if (len > 32) return;  // 数据过长

    txBuf[txLen++] = PROTOCOL_SYNC1;
    txBuf[txLen++] = PROTOCOL_SYNC2;
    txBuf[txLen++] = cmd;
    txBuf[txLen++] = len;

    for (uint8_t i = 0; i < len; i++)
    {
        txBuf[txLen++] = data[i];
    }

    txBuf[txLen++] = CalcChecksum(PROTOCOL_SYNC1, PROTOCOL_SYNC2, cmd, len, data);

    /* 通过 UART1 发送 */
    HAL_UART_Transmit(&DEBUG_UART, txBuf, txLen, 100);
}

/**
 * @brief 字节接收处理 (在 HAL_UART_RxCpltCallback 中调用)
 * @param byte 接收到的一字节
 */
void Protocol_RxHandler(uint8_t byte)
{
    switch (g_protoState)
    {
        case PROTO_STATE_SYNC1:
            if (byte == PROTOCOL_SYNC1)
            {
                g_protoState = PROTO_STATE_SYNC2;
            }
            break;

        case PROTO_STATE_SYNC2:
            if (byte == PROTOCOL_SYNC2)
            {
                g_protoState = PROTO_STATE_CMD;
            }
            else
            {
                g_protoState = PROTO_STATE_SYNC1;  // 错误, 回退
            }
            break;

        case PROTO_STATE_CMD:
            g_frameCmd    = byte;
            g_protoState  = PROTO_STATE_LENGTH;
            break;

        case PROTO_STATE_LENGTH:
            g_frameLength = byte;
            g_rxIndex     = 0;

            if (g_frameLength == 0)
            {
                g_protoState = PROTO_STATE_CHECKSUM;
            }
            else if (g_frameLength > 32)
            {
                /* 长度异常, 丢弃 */
                g_protoState = PROTO_STATE_SYNC1;
            }
            else
            {
                g_protoState = PROTO_STATE_DATA;
            }
            break;

        case PROTO_STATE_DATA:
            g_frameData[g_rxIndex++] = byte;
            if (g_rxIndex >= g_frameLength)
            {
                g_protoState = PROTO_STATE_CHECKSUM;
            }
            break;

        case PROTO_STATE_CHECKSUM:
        {
            /* 校验 */
            uint8_t expected = CalcChecksum(PROTOCOL_SYNC1, PROTOCOL_SYNC2,
                                            g_frameCmd, g_frameLength, g_frameData);
            if (byte == expected)
            {
                g_frameReady = 1;   // 帧接收成功
            }
            /* 校验失败则静默丢弃 */
            g_protoState = PROTO_STATE_SYNC1;
            break;
        }

        default:
            g_protoState = PROTO_STATE_SYNC1;
            break;
    }
}

/**
 * @brief 协议任务处理 (放在主循环中)
 *
 * 处理接收到的帧, 执行对应命令
 */
void Protocol_Process(void)
{
    if (!g_frameReady) return;
    g_frameReady = 0;

    switch (g_frameCmd)
    {
        case CMD_READ_DATA:
        {
            /* 上位机请求当前数据 */
            uint8_t rawTemp = (uint8_t)(g_sensorData.temperature * 10);
            uint8_t rawHumi = (uint8_t)(g_sensorData.humidity * 10);

            uint8_t resp[5];
            resp[0] = (rawTemp >> 8) & 0xFF;   // 温度高字节
            resp[1] = rawTemp & 0xFF;          // 温度低字节
            resp[2] = (rawHumi >> 8) & 0xFF;   // 湿度高字节
            resp[3] = rawHumi & 0xFF;          // 湿度低字节
            resp[4] = g_sensorData.valid;      // 数据有效标志

            Protocol_SendFrame(CMD_READ_DATA | CMD_ACK, resp, 5);
            break;
        }

        case CMD_READ_THRESHOLD:
        {
            /* 上位机请求阀值配置 */
            uint8_t resp[12];
            /* 温度上限 */
            int16_t temp = (int16_t)(g_threshold.temp_high * 10);
            resp[0] = (temp >> 8) & 0xFF;
            resp[1] = temp & 0xFF;
            /* 温度下限 */
            temp = (int16_t)(g_threshold.temp_low * 10);
            resp[2] = (temp >> 8) & 0xFF;
            resp[3] = temp & 0xFF;
            /* 湿度上限 */
            int16_t humi = (int16_t)(g_threshold.humi_high * 10);
            resp[4] = (humi >> 8) & 0xFF;
            resp[5] = humi & 0xFF;
            /* 湿度下限 */
            humi = (int16_t)(g_threshold.humi_low * 10);
            resp[6] = (humi >> 8) & 0xFF;
            resp[7] = humi & 0xFF;

            Protocol_SendFrame(CMD_READ_THRESHOLD | CMD_ACK, resp, 8);
            break;
        }

        case CMD_SET_THRESHOLD:
        {
            /* 上位机设置阀值: data[8] = th/tl/hh/hl各2字节 */
            if (g_frameLength >= 8)
            {
                g_threshold.temp_high = (int16_t)((g_frameData[0] << 8) | g_frameData[1]) / 10.0f;
                g_threshold.temp_low  = (int16_t)((g_frameData[2] << 8) | g_frameData[3]) / 10.0f;
                g_threshold.humi_high = (int16_t)((g_frameData[4] << 8) | g_frameData[5]) / 10.0f;
                g_threshold.humi_low  = (int16_t)((g_frameData[6] << 8) | g_frameData[7]) / 10.0f;

                /* 保存到 Flash */
                Config_Save(&g_threshold);

                Protocol_SendAck();
            }
            else
            {
                Protocol_SendNack(0x01);  // 数据长度错误
            }
            break;
        }

        default:
            Protocol_SendNack(0x02);  // 未知命令
            break;
    }
}

/**
 * @brief 主动上报温湿度数据
 */
void Protocol_UploadData(SensorData_t *data)
{
    if (!data->valid) return;

    int16_t rawTemp = (int16_t)(data->temperature * 10);
    int16_t rawHumi = (int16_t)(data->humidity * 10);

    uint8_t payload[6];
    payload[0] = (rawTemp >> 8) & 0xFF;
    payload[1] = rawTemp & 0xFF;
    payload[2] = (rawHumi >> 8) & 0xFF;
    payload[3] = rawHumi & 0xFF;
    payload[4] = data->valid;
    payload[5] = (data->timestamp >> 24) & 0xFF;  // 时间戳高字节 (s)

    Protocol_SendFrame(CMD_READ_DATA | CMD_ACK, payload, 6);
}

/**
 * @brief 主动上报报警信息
 */
void Protocol_UploadAlarm(AlarmType_t type, float value)
{
    int16_t rawValue = (int16_t)(value * 10);

    uint8_t payload[3];
    payload[0] = (uint8_t)type;
    payload[1] = (rawValue >> 8) & 0xFF;
    payload[2] = rawValue & 0xFF;

    Protocol_SendFrame(CMD_ALARM_NOTIFY, payload, 3);
}

/**
 * @brief 发送阀值应答
 */
void Protocol_SendThreshold(Threshold_t *thr)
{
    int16_t temp;

    uint8_t payload[8];

    temp = (int16_t)(thr->temp_high * 10);
    payload[0] = (temp >> 8) & 0xFF;
    payload[1] = temp & 0xFF;

    temp = (int16_t)(thr->temp_low * 10);
    payload[2] = (temp >> 8) & 0xFF;
    payload[3] = temp & 0xFF;

    temp = (int16_t)(thr->humi_high * 10);
    payload[4] = (temp >> 8) & 0xFF;
    payload[5] = temp & 0xFF;

    temp = (int16_t)(thr->humi_low * 10);
    payload[6] = (temp >> 8) & 0xFF;
    payload[7] = temp & 0xFF;

    Protocol_SendFrame(CMD_READ_THRESHOLD | CMD_ACK, payload, 8);
}

/**
 * @brief 发送ACK
 */
void Protocol_SendAck(void)
{
    uint8_t dummy = 0;
    Protocol_SendFrame(CMD_ACK, &dummy, 1);
}

/**
 * @brief 发送NACK
 */
void Protocol_SendNack(uint8_t errorCode)
{
    Protocol_SendFrame(CMD_NACK, &errorCode, 1);
}
