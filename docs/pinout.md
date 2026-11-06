# Cardputer ADV 引脚与总线

来源：`Sch_M5CardputerAdv_v1.0_2025_06_20_17_19_58.pdf`、
`Sch_StampS3_v0.3.3.pdf`、`CardPuter_Mp3_Adv-main`、`area512` 外部屏实现。

## 显示屏

### 内部 ST7789V2 240×135

- 由 `M5Cardputer.Display`（M5GFX）管理；
- **SPI3**；
- rotation 1；
- 应用层不直接配置引脚。

### 外部 ILI9341 320×240

| 信号 | GPIO |
|---|---|
| SCK | 40 |
| MOSI | 14 |
| MISO | 39（屏幕不使用） |
| CS | 5 |
| DC | 6 |
| RST | 3 |

- 总线：**SPI2**，20 MHz 写；
- `lgfx::Panel_ILI9341`，GRAM 240×320，rotation 1 → 320×240；
- `offset_rotation = 0`、`invert = false`、`rgb_order = false`；
- **与 SD 共用 SPI2**；
- 不要使用独立 `ILI9341_demo` 的 SPI3 / 40 MHz 配置。

## microSD

| 信号 | GPIO |
|---|---|
| SCK | 40 |
| MOSI | 14 |
| MISO | 39 |
| CS | 12 |

- SPI2，20 MHz；
- 必须先初始化外部屏（由它配置 SPI2），再挂载 SD；
- 一个 SPI host：LCD CS5，SD CS12。

## ES8311 音频

| 信号 | GPIO / 地址 |
|---|---|
| I2C SDA | 8 |
| I2C SCL | 9 |
| ES8311 地址 | 0x18 |
| I2S BCLK | 41 |
| I2S LRCK | 43 |
| I2S DOUT | 42 |
| 扬声器 AMP_EN | 46（HIGH = 开） |

- 扬声器：NS4150B + 8Ω 1W，单声道；
- 耳机：立体声；
- 不要初始化 `M5.Speaker`，使用自定义 I2S0；
- I2S：48 kHz / 16-bit stereo；
- 引擎原生 55 466 Hz，重采样到 48 kHz；
- **ADV 的 Stamp-S3 连接座没有引出耳机检测 GPIO**，本项目不做软件检测。
- MCU 始终把 `AMP_EN` 保持高；插入耳机时，耳机座第 7 脚脱离地，
  `HP_DET` 经 D3/R39 上拉到 3.3V，Q4 导通，把 AMP_EN 拉低，
  NS4150B 关闭，喇叭静音。音频 DAC_P 仍并联到耳机座。
- SYS 的 `Output` 只切换**软件音量预设**（Speaker/Headphones），
  不控制这个硬件静音；耳机音量由 `HP Gain` 控制。

## 键盘与传感器

| 设备 | 总线 / 地址 |
|---|---|
| TCA8418 键盘 | I2C 0x34，4×14 矩阵 |
| BMI270 IMU | I2C（M5Unified） |
| 电池 ADC | GPIO10（以 ADV 原理图为准） |
| 红外 LED | 本项目未使用 |

## 总线归属

```text
SPI3 ── 内部 ST7789 240×135
SPI2 ── 外部 ILI9341 320×240 + microSD（CS5 / CS12）
I2C  ── TCA8418 / ES8311 / BMI270
I2S0 ── ES8311（BCLK41 / LRCK43 / DOUT42）
```

外部屏不要接到 SPI3。独立 `ILI9341_demo` 使用 SPI3 + 40 MHz，是因为它没有内部屏和 SD，不能直接用于本项目双屏配置。
