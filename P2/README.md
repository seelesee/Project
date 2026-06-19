# STM32 智能温湿度监测系统

> 🌡️ STM32F103C8T6 + DHT22 + SSD1306 OLED + 按键阀值设定 + 蜂鸣器报警 + 串口上传

## 开发工具链

| 工具 | 版本 | 用途 |
|------|------|------|
| **STM32CubeMX** | 6.11+ | 引脚配置 / 时钟树 / 生成 HAL 代码框架 |
| **Keil MDK uVision5** | 5.38+ | 代码编辑 / 编译 / 链接 / 烧录 |
| **Proteus 8** | 8.x Professional | 原理图仿真 / 虚拟终端 / 逻辑分析仪 |

---

## 快速启动 (3步走)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  STM32CubeMX    │────▶│  Keil uVision5  │────▶│  Proteus 8      │
│  引脚+时钟配置   │     │  编译生成 HEX    │     │  加载仿真验证    │
│  生成代码框架    │     │  调试优化代码    │     │  观察运行效果    │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### 第 1 步: STM32CubeMX 配置

```
1. 双击打开 STM32_TempHumidity.ioc
2. 检查引脚配置:
   PA0  → DHT22 DATA  (开漏输出)
   PA1  → KEY_UP      (输入+下拉)
   PA2  → KEY_DOWN    (输入+下拉)
   PA3  → KEY_SET     (输入+下拉)
   PA4  → BUZZER      (推挽输出)
   PA9  → USART1_TX   (复用推挽)
   PA10 → USART1_RX   (浮空输入)
   PB6  → OLED_SCL    (推挽输出)
   PB7  → OLED_SDA    (推挽输出)
3. 检查时钟: HSE 8MHz → PLL ×9 → 72MHz
4. Project Manager → Toolchain: MDK-ARM V5
5. 点击 GENERATE CODE 生成代码
```

### 第 2 步: Keil MDK 编译

```
方法 A - 直接打开工程:
  双击 MDK-ARM\STM32_TempHumidity.uvprojx → Build (F7)

方法 B - 命令行编译:
  双击运行 MDK-ARM\build.bat

生成 HEX 文件:
  MDK-ARM\STM32_TempHumidity\STM32_TempHumidity.hex
```

### 第 3 步: Proteus 仿真

```
1. 打开 Proteus 8
2. File → Open Project → Proteus\STM32_TempHumidity.pdsprj
3. 双击 STM32F103C8 元件 → Program File 加载 .hex 文件
4. 点击 ▶ 开始仿真
5. 观察 OLED 显示, 点击按键, 查看虚拟终端串口数据
```

---

## 功能特性

| 功能 | 说明 |
|------|------|
| 🌡️ **温湿度采集** | DHT22 数字传感器，精度 ±0.5°C / ±2%RH，2秒周期，DWT精确时序 |
| 📺 **OLED显示** | SSD1306 128×64 软件I2C，显存缓冲刷新，6x8 ASCII + 16x16 中文 |
| 🔘 **按键设定** | 3按键 (SET/UP/DOWN)，事件队列，单击/长按，4项阀值轮转设定 |
| 🔊 **蜂鸣器报警** | 非阻塞状态机，间歇报警(200ms/500ms)，按键确认音，一键静音 |
| 📡 **串口上传** | UART1 115200bps，自定义 AA55 帧协议，5秒定时+报警即时上报 |
| 💾 **配置存储** | STM32内部Flash最后一页，魔数+CRC16校验，掉电不丢失 |

---

## 硬件接线

```
                        STM32F103C8T6 (Blue Pill)
                     ┌──────────────────────────┐
     DHT22 DATA ─────│ PA0              PB6 ────│─── OLED SCL
     4.7K to 3.3V    │                  PB7 ────│─── OLED SDA
     KEY_UP ─────────│ PA1              PA9 ────│─── UART TX (→VT RXD)
     KEY_DOWN ───────│ PA2              PA10────│─── UART RX (→VT TXD)
     KEY_SET ────────│ PA3                       │
                     │ PA4 ───[1K]─── B(8050)   │
                     │              E─GND C─LS1  │
                     │ PA13(SWDIO)   PA14(SWCLK) │  (仅调试)
                     │ VDD/VBAT → 3.3V          │
                     │ VSS → GND                │
                     └──────────────────────────┘

  蜂鸣器驱动: PA4 → 1KΩ → S8050(B) → E=GND, C=蜂鸣器_负, 蜂鸣器_正=3.3V
```

| 外设 | 引脚 | 模式 | 外部电路 |
|------|------|------|----------|
| DHT22 DATA | PA0 | 开漏输出/输入切换 | **4.7KΩ 上拉**到 3.3V |
| OLED SCL | PB6 | 推挽输出 | 直连 |
| OLED SDA | PB7 | 推挽输出 | 直连 |
| KEY UP | PA1 | 输入(内部下拉) | 按键 → 3.3V |
| KEY DOWN | PA2 | 输入(内部下拉) | 按键 → 3.3V |
| KEY SET | PA3 | 输入(内部下拉) | 按键 → 3.3V |
| BUZZER | PA4 | 推挽输出 | 1KΩ → NPN基极 |
| UART TX | PA9 | 复用推挽 | → USB-TTL RX |
| UART RX | PA10 | 浮空输入 | ← USB-TTL TX |

---

## 项目结构

```
STM32_TempHumidity/
│
├── STM32_TempHumidity.ioc        ← CubeMX 工程 (引脚/时钟/外设配置)
│
├── main.c                        ← 主程序 (系统初始化 + 主循环 + 中断服务)
├── main.h                        ← 主头文件 (引脚宏/数据结构/枚举/函数声明)
│
├── Inc/                          ← 头文件目录
│   ├── stm32f1xx_hal_conf.h      ← HAL 库模块配置
│   ├── dht22.h                   ← DHT22 传感器
│   ├── oled.h                    ← SSD1306 OLED
│   ├── oled_font.h               ← ASCII + 中文字库
│   ├── button.h                  ← 按键驱动
│   ├── buzzer.h                  ← 蜂鸣器
│   ├── protocol.h                ← 串口协议
│   └── config.h                  ← Flash 存储
│
├── Src/                          ← 源文件目录
│   ├── dht22.c                   ← DHT22 单总线驱动 (DWT精确定时)
│   ├── oled.c                    ← OLED 软件I2C驱动 (画点/线/字符/中文)
│   ├── button.c                  ← 按键扫描 (事件队列/消抖/长按)
│   ├── buzzer.c                  ← 蜂鸣器状态机 (连续/间歇/单次)
│   ├── protocol.c                ← 串口协议 (帧解析/校验/上报)
│   └── config.c                  ← Flash配置管理 (页擦除/CRC16)
│
├── MDK-ARM/                      ← Keil uVision5 工程
│   ├── STM32_TempHumidity.uvprojx  ← 工程文件 (源文件/编译设置)
│   ├── STM32_TempHumidity.uvoptx   ← 选项文件 (优化/调试设置)
│   ├── STM32F103C8Tx_FLASH.sct    ← 链接器分散加载文件
│   └── build.bat                   ← Windows 编译脚本
│
├── Proteus/                      ← Proteus 8 仿真
│   ├── STM32_TempHumidity.pdsprj   ← 仿真原理图
│   └── README_Proteus.md           ← 仿真搭建详细说明
│
└── README.md                     ← 本文件
```

---

## 新建工程完整流程 (从零开始)

### 1. 打开 CubeMX 配置

```
① 双击 STM32_TempHumidity.ioc
② 检查 Pinout 视图: 确认 PA0~PA10 / PB6~PB7 的引脚配置
③ 检查 Clock Configuration: HSE(8MHz) → PLL(×9) → SYSCLK(72MHz)
④ Project Manager 标签:
   - Project Name: STM32_TempHumidity
   - Toolchain / IDE: MDK-ARM V5
   - Code Generator: 勾选 "Generate peripheral initialization as C code"
⑤ 点击右上角 GENERATE CODE
```

### 2. 添加应用层源码到 Keil

CubeMX 生成代码后，把本项目的应用层文件加入工程:

```
① 打开 MDK-ARM\STM32_TempHumidity.uvprojx
② 在 Project 窗口中右键添加 Groups:
   - Application/User     → main.c, main.h
   - Drivers/Sensor       → Src/dht22.c, Inc/dht22.h
   - Drivers/Display      → Src/oled.c, Inc/oled.h, Inc/oled_font.h
   - Drivers/Button       → Src/button.c, Inc/button.h
   - Drivers/Buzzer       → Src/buzzer.c, Inc/buzzer.h
   - Drivers/Protocol     → Src/protocol.c, Inc/protocol.h
   - Drivers/Config       → Src/config.c, Inc/config.h
③ Options for Target → C/C++ → Include Paths:
   添加 ..\Inc
④ Options for Target → Output → 勾选 "Create HEX File"
⑤ 按 F7 编译
```

### 3. 在 Proteus 中仿真

```
① 打开 Proteus\STM32_TempHumidity.pdsprj
② 双击 STM32F103C8 → Program File → 选择编译好的 .hex
③ 点击 ▶ 开始仿真
④ 弹出虚拟终端查看串口数据
```

---

## 使用说明

### 🟢 正常运行模式

| 操作 | 功能 |
|------|------|
| 上电启动 | OLED 显示 Logo 2秒 → 进入监测主界面 |
| 正常显示 | 实时温湿度 + 状态 (OK/报警) |
| 超限报警 | 蜂鸣器间歇鸣响 + OLED显示报警 + 串口即时上报 |
| 按 **UP** | 手动触发一次串口数据上传 |
| 按 **DOWN** | 静音当前报警 |
| 长按 **SET** | 恢复默认阀值 |

### 🔵 阀值设定模式

```
按 SET 进入 →  温度上限 (T↑)
    ↓ SET
              温度下限 (T↓)
    ↓ SET
              湿度上限 (H↑)
    ↓ SET
              湿度下限 (H↓)
    ↓ SET →  保存到Flash + 返回正常模式

按 UP/DOWN: 当前项 ±0.5 步进
长按 SET:   保存并退出
当前编辑项: 闪烁提示
```

---

## 串口协议

### 帧格式

```
┌──────┬──────┬──────┬──────┬─────────────────┬──────────┐
│ 0xAA │ 0x55 │ CMD  │ LEN  │  DATA[0..LEN-1] │ CHECKSUM │
│  1B  │  1B  │  1B  │  1B  │     N bytes      │    1B    │
└──────┴──────┴──────┴──────┴─────────────────┴──────────┘

CHECKSUM = 0xAA ^ 0x55 ^ CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[N-1]
```

### 命令字

| 命令 | 值 | 方向 | DATA 格式 |
|------|-----|------|-----------|
| READ_DATA | 0x01 | 上位机→设备 | (空) |
| READ_DATA+ACK | 0x81 | 设备→上位机 | T_high T_low H_high H_low valid |
| READ_THRESHOLD | 0x02 | 上位机→设备 | (空) |
| SET_THRESHOLD | 0x03 | 上位机→设备 | THhi THlo TLhi TLlo HHhi HHlo HLhi HLlo |
| ALARM_NOTIFY | 0x04 | 设备→上位机 | alarm_type value_high value_low |
| ACK | 0x80 | 双向 | 0x00 |
| NACK | 0xFF | 双向 | error_code |

### Python 上位机示例

```python
import serial, struct, time

ser = serial.Serial('COM3', 115200, timeout=2)

def read_sensor():
    """读取温湿度"""
    chk = 0xAA ^ 0x55 ^ 0x01 ^ 0x00
    ser.write(bytes([0xAA, 0x55, 0x01, 0x00, chk]))
    resp = ser.read(10)
    if len(resp) >= 9 and resp[0] == 0xAA and resp[1] == 0x55:
        temp = ((resp[4] << 8) | resp[5]) / 10.0  # 有符号
        humi = ((resp[6] << 8) | resp[7]) / 10.0
        valid = resp[8]
        return temp, humi, valid
    return None, None, 0

def set_threshold(thigh, tlow, hhigh, hlow):
    """设置阀值"""
    data = struct.pack('>hhhh', int(thigh*10), int(tlow*10),
                       int(hhigh*10), int(hlow*10))
    chk = 0xAA ^ 0x55 ^ 0x03 ^ 8
    for b in data: chk ^= b
    ser.write(bytes([0xAA, 0x55, 0x03, 8]) + data + bytes([chk]))
    resp = ser.read(5)

# 使用示例
t, h, v = read_sensor()
print(f"温度: {t}°C  湿度: {h}%RH  有效: {v}")
```

---

## 默认阀值

| 项目 | 默认值 | 范围 |
|------|--------|------|
| 温度上限 | 35.0°C | -40 ~ 80°C |
| 温度下限 | 5.0°C | -40 ~ 80°C |
| 湿度上限 | 80.0%RH | 0 ~ 100%RH |
| 湿度下限 | 20.0%RH | 0 ~ 100%RH |

---

## 常见问题

### Q: CubeMX 打开 .ioc 报错?
A: 请使用 CubeMX 6.11 以上版本打开。如版本不兼容，新建工程 → 选择 STM32F103C8Tx → 手动按上表配置引脚。

### Q: Keil 编译提示找不到 HAL 库文件?
A: 需要在 `Options → C/C++ → Include Paths` 中添加 CubeMX 生成的 Drivers 路径:
```
../Drivers/CMSIS/Include
../Drivers/CMSIS/Device/ST/STM32F1xx/Include
../Drivers/STM32F1xx_HAL_Driver/Inc
```

### Q: Proteus 中找不到 SSD1306 OLED?
A: 用 `LM044L` (20×4字符LCD) + `PCF8574` (I2C扩展) 替代测试，或使用 `I2C DEBUGGER` 观察 I2C 数据。

### Q: DHT22 在 Proteus 中读不出数据?
A: Proteus 中 DHT22 模型需要正确的时序。确保 STM32 时钟配置为 72MHz，DWT 计数器正常工作。

### Q: 串口虚拟终端接收到乱码?
A: 确认虚拟终端波特率设为 **115200-8-N-1**，与代码中 USART1 配置一致。

---

## 注意事项

1. ⚡ **DHT22 上拉**: DATA 引脚必须外接 4.7KΩ~10KΩ 电阻上拉到 3.3V
2. ⏱️ **采样间隔**: DHT22 最低 2 秒采样间隔，代码已按此配置
3. 🔊 **蜂鸣器驱动**: IO 口驱动能力有限，必须用 NPN 三极管 (S8050/2N2222)
4. 💾 **Flash 寿命**: STM32F103 Flash 擦写约 1万次，设定模式退出时才保存
5. 🔤 **中文字库**: `oled_font.h` 中的中文字库为占位数据，请用 PCtoLCD2002 取模替换
6. 🔌 **调试接口**: PA13(SWDIO) / PA14(SWCLK) 保留给 ST-Link 调试，不要用作 GPIO

---

## License

MIT License - 仅供学习和参考使用
