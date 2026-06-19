# Proteus 8 仿真搭建指南

## 快速开始

### 方法一: 打开工程文件 (推荐)
1. 打开 Proteus 8 Professional
2. `File` → `Open Project` → 选择 `STM32_TempHumidity.pdsprj`
3. 双击 STM32F103C8, 在 `Program File` 中加载编译好的 `.hex` 文件
4. 点击 ▶ 开始仿真

### 方法二: 手动搭建原理图
如果 .pdsprj 文件版本不兼容, 按以下步骤手动搭建。

---

## 元器件清单

| 编号 | 器件 | Proteus 关键字 | 数量 |
|------|------|---------------|------|
| U1 | STM32F103C8T6 | `STM32F103C8` | 1 |
| U2 | DHT22 传感器 | `DHT22` (或 `DHT11` 替代) | 1 |
| U3 | SSD1306 OLED | `SSD1306` (或 `LCD 20x4` 替代) | 1 |
| SW1-3 | 轻触按键 | `BUTTON` | 3 |
| Q1 | NPN三极管 | `S8050` 或 `2N2222` | 1 |
| LS1 | 有源蜂鸣器 | `BUZZER` 或 `SPEAKER` | 1 |
| R1 | 4.7KΩ 电阻 | `RES` | 1 |
| R2-4 | 10KΩ 电阻 | `RES` | 3 |
| R5 | 1KΩ 电阻 | `RES` | 1 |
| VT1 | 虚拟终端 | `VIRTUAL TERMINAL` | 1 |

---

## 接线表

### STM32F103C8T6 引脚分配

```
                        ┌──────────────────┐
                        │  STM32F103C8T6   │
        DHT22 DATA ◄─── │ PA0              │
         BUTTON UP ◄─── │ PA1              │
       BUTTON DOWN ◄─── │ PA2              │
         BUTTON SET ◄─── │ PA3              │
      BUZZER (1K) ◄─── │ PA4              │
           UART TX ◄─── │ PA9      ────────┤─── VT1 RXD
           UART RX ◄─── │ PA10     ────────┤─── VT1 TXD
          OLED SCL ◄─── │ PB6      ────────┤─── SSD1306 SCL
          OLED SDA ◄─── │ PB7      ────────┤─── SSD1306 SDA
            +3.3V ◄─── │ VDD/VDDA/VBAT  │
              GND ◄─── │ VSS/VSSA       │
              SWD ◄─── │ PA13/PA14      │  (调试接口,悬空)
                        └──────────────────┘
```

### 详细接线

#### 1. DHT22 传感器
```
DHT22 VCC  ─── +3.3V
DHT22 GND  ─── GND
DHT22 DATA ─── PA0 ─── 4.7KΩ ─── +3.3V  (上拉电阻)
```

#### 2. OLED SSD1306 (I2C)
```
SSD1306 VCC  ─── +3.3V
SSD1306 GND  ─── GND
SSD1306 SCL  ─── PB6
SSD1306 SDA  ─── PB7
```

#### 3. 按键 (内部下拉模式)
```
SW_UP    P1 ─── PA1
SW_UP    P2 ─── +3.3V
PA1 ─── 10KΩ ─── GND  (下拉电阻)

SW_DOWN  P1 ─── PA2
SW_DOWN  P2 ─── +3.3V
PA2 ─── 10KΩ ─── GND

SW_SET   P1 ─── PA3
SW_SET   P2 ─── +3.3V
PA3 ─── 10KΩ ─── GND
```

#### 4. 蜂鸣器驱动
```
PA4 ─── 1KΩ ─── Q1(B)  (基极限流)
Q1(E) ─── GND
Q1(C) ─── BUZZER_N
BUZZER_P ─── +3.3V
```

#### 5. 串口虚拟终端
```
VT1 RXD ─── PA9 (UART1 TX)
VT1 TXD ─── PA10 (UART1 RX)
VT1 属性: Baud=115200, Data=8, Parity=None, Stop=1
```

---

## Proteus 仿真步骤

### Step 1: 创建工程
1. 打开 Proteus 8
2. `New Project` → 命名 `STM32_TempHumidity`
3. 选择 `Schematic Design`
4. 选择 `No Firmware Project` (稍后手动加载hex)

### Step 2: 放置元器件
- 点击 `P` (Pick Devices) 搜索并放置上述所有元器件
- 按 `PAGE UP` / `PAGE DOWN` 旋转元件

### Step 3: 连线
- 点击元件引脚, 拖拽到目标引脚
- 用 `WIRE LABEL` (网络标签) 连接不相邻的元件

### Step 4: 加载固件
1. 双击 `STM32F103C8` 元件
2. `Program File` → 浏览到 `../MDK-ARM/STM32_TempHumidity/STM32_TempHumidity.hex`
3. `Clock Frequency` → 输入 `72000000`

### Step 5: 配置虚拟终端
1. 双击 `VIRTUAL TERMINAL`
2. 设置 `Baud Rate = 115200`
3. 设置 `Data Bits = 8`
4. 设置 `Parity = NONE`
5. 设置 `Stop Bits = 1`

### Step 6: 运行仿真
1. 点击 ▶ (Play) 开始
2. 观察 OLED 显示温湿度
3. 点击按键测试设定模式
4. 查看虚拟终端接收的串口数据

---

## 注意

1. **DHT22 在 Proteus 中**: 如果没有 DHT22 模型, 可用 `DHT11` 替代(代码兼容)
2. **SSD1306 在 Proteus 中**: 如果没有 OLED 模型, 可用 `LM044L` (20×4 字符LCD) 替代测试, 或使用 `I2C DEBUGGER` 观察 I2C 通信
3. **HEX 文件**: 必须先用 Keil 编译生成 `.hex` 文件才能仿真
4. **仿真速度**: Proteus 模拟 STM32 可能较慢, 点击 `System` → `Animation Options` 调整帧率

---

## 替代元器件 (Proteus 库中找不到时)

| 原器件 | 替代品 |
|--------|--------|
| STM32F103C8 | STM32F103C6 (32KB Flash) |
| DHT22 | DHT11 |
| SSD1306 OLED | LM044L (20×4 LCD) + PCF8574 (I2C) |
| S8050 | 2N2222 / BC547 |

---

## 调试技巧

### 1. 查看串口数据
在 Proteus 中弹出虚拟终端窗口, 可以看到:
```
[AA 55 01 05 00 FF 00 64 01 XX ...] ← 温湿度数据帧
[AA 55 04 03 01 01 5E XX ...]      ← 报警通知帧
```

### 2. 逻辑分析仪
添加 `LOGIC ANALYSER` 连接到 UART TX/RX, 可直观观察串口波形和时序。

### 3. 电压探针
在 PA0 (DHT22) 添加 `VOLTAGE PROBE`, 观察单总线通信时序。
