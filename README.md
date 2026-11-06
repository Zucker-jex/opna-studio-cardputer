# OPNA Studio Cardputer

> **面向 M5Stack Cardputer ADV 的双屏 OPNA FM 工作站**  
> 版本 **0.4.0** · 2026-11-07

OPNA Studio Cardputer 是把 **OPNA Studio P2** 的 OPNA 引擎、音序器和像素视觉语言，移植到 Cardputer ADV 的独立项目。

它不是一个简单的音频 demo：外屏 320×240 是主工作站，内屏 240×135 是演奏 HUD；支持 16 个 pattern、每轨音色、混音、双屏示波/FFT、SD 存档、MIDI 导出和完整键盘演奏。

---

## 当前状态

已确认或已实现：

- Cardputer ADV 双屏：内部 ST7789V2 240×135 + 外部 ILI9341 320×240；
- 外部屏与 SD 共用 SPI2，SD 挂载正常；
- ES8311 / I2S 48 kHz 立体声输出，扬声器与耳机均有声音；
- TCA8418 键盘；
- OPNA 引擎 55 466 Hz 原生渲染 → 48 kHz 重采样；
- 16 patterns、SEQ 编辑、MIX、SYS、SD 存档、MIDI 文件导出；
- 内置 16-pattern 曲目 **Fracture Protocol**（F# minor，132 BPM，全通道）。

当前固件构建：

```text
ESP-IDF 5.5.6 / esp32s3
App:    606 976 B (0x94300)
Merged: 672 512 B (0xA4300)
```

> 当前功能已在开发样机上验证通过。
> ADV 的 Stamp-S3 连接座没有引出耳机检测 GPIO，因此本项目**不实现软件自动检测**。
> 耳机插入时由硬件 Q4 将 AMP_EN 拉低，自动关闭 NS4150B；MCU 始终把 AMP_EN 保持高。
> SYS 的 `Output` 只是耳机音量预设，不控制这个硬件静音。

---

## 功能

### 双屏 UI

**外部 320×240：主工作站**

- `SEQ`：10 轨 × 16 steps，pattern A–P，播放头、VEL/GATE/PEN；
- `SYN`：8 个 FM patch、两八度钢琴、八度、live、ARP/LATCH；
- `MIX`：10 轨电平/静音、master、L/R VU；
- `VIS`：64-bin FFT / 触发示波器 + CPU/UND/FPS；
- `SYS`：播放、pattern 工具、音频、存档、导出、曲目和电源。

**内部 240×135：演奏 HUD**

- 当前页、播放状态、SD；
- SEQ 轨电平 / 当前 step；
- SYN patch、键位、ARP/LATCH；
- MIX L/R VU；
- VIS 的反向视图（外屏 FFT ↔ HUD scope）；
- SYS/状态提示。

### 音序与 Pattern

- 16 个 pattern：A–P；
- 每 pattern 长度 1–16 steps；
- `Copy`：复制当前 pattern 到下一个；
- `Clear`：清空当前 pattern；
- 每步可编辑音符、VEL、GATE；
- `PEN` 记录 SYN 页最后弹奏的音高，回 SEQ 放音块时使用。

### 音色

- 8 个内置 FM patch：
  `BASS / LEAD / PAD / PLUCK / BELL / BRASS / ORGAN / GLASS`；
- FM1–FM6 每轨独立 patch；
- SYN 页 `Fn`+`↑↓` 修改**当前 SEQ FM 轨**的 patch，并写回序列；
- 目前没有外部音色包加载器。

### 内置曲目

**Fracture Protocol**

- 16 patterns，F# minor；
- 132 BPM；
- 16 个不同的 lead motif；
- 使用 FM1–FM6、SSG1/SSG2、NOISE、DRM；
- 段落：intro/build → verse → chorus/climax → breakdown/outro；
- 和声：

```text
F#m – D – Bm – C#
F#m – D – A  – E
Bm  – D – A  – E
F#m – D – Bm – C#
```

SYS → `Song` 可重新载入。`Demo` 仍保留原始 demo。

### 音频

- OPNA 原生 **55 466 Hz**；
- 48 kHz 立体声 I2S0；
- 自定义 ES8311 初始化，不使用 `M5.Speaker`；
- 耳机音量：SYS 手动选择 Speaker / Headphones 预设；
- AMP_EN GPIO46：MCU 保持高；插入耳机时 Q4 硬件拉低，NS4150B 静音；
- ADV 连接座无耳机检测 GPIO；
- 手动 `Output` 选择 Speaker/Headphones；`HP Gain`：`50/25/12/6/3/1%`，默认 **3%**；
- 数字音量 + master；MIX 的 MASTER 与 SYS Volume 共用；
- BMI270 tilt 可弯 live voice。

> 注意：ADV 连接座没有耳机检测 GPIO；扬声器静音由 Q4 硬件完成。
> 软件只用 `SYS → Output → Speaker / Headphones` 选择音量预设，
> 用 `HP Gain` 降低耳机音量。

### 存储

- SD 路径：`/sdcard`；
- 存档：`/sdcard/opna2.p2s`；
- 当前格式 **V4**，7 739 B；
- 保存 song、16 patterns、长度、BPM、swing、chain、pattern、pages、音量、背光、HP Gain；
- 开机自动加载；
- V3/V2 会自动迁移。

### MIDI

- SYS → `Export MID`；
- 输出 `/sdcard/opna2.mid`；
- SMF format 0，480 PPQ，16 patterns；
- 鼓在 MIDI channel 10；
- 当前是 **MIDI 文件导出**，不是 USB MIDI 输入/输出。

---

## 硬件

| 项目 | Cardputer ADV |
|------|---------------|
| SoC | ESP32-S3FN8，双核 Xtensa LX7 @ 240 MHz |
| 内存 | 8 MB Flash，512 KB SRAM，**无 PSRAM** |
| 内屏 | ST7789V2 240×135，SPI3 |
| 外屏 | ILI9341 320×240，SPI2，20 MHz |
| 外屏 + SD | SPI2：SCK 40 / MOSI 14 / MISO 39 / LCD CS 5 / SD CS 12 |
| 音频 | ES8311；I2S BCLK 41 / LRCK 43 / DOUT 42 |
| 扬声器 | NS4150B，单声道；AMP_EN 46（Q4 硬件静音） |
| 耳机 | 立体声；硬件自动静音喇叭，软件手动音量预设 |
| 键盘 | TCA8418，I2C 8/9 |
| IMU | BMI270 |
| 电池 | 1500 mAh |

详细接线见 [`docs/pinout.md`](docs/pinout.md)。

---

## 按键

### 全局

| 按键 | 行为 |
|---|---|
| `Tab` | 下一页 |
| `Backspace` | 上一页 |
| `;` `.` `,` `/` | 上 / 下 / 左 / 右，始终有效 |
| `Enter` | 执行 / 进入 / SYN live |
| `Space` | 播放 / 停止 |
| `Fn` | 切换 MOD |
| 按住 `Fn` + `1`–`0` | 选择 pattern A–J |
| 非 SYN 页面 `1`–`5` | 直接切页 |
| `` ` `` | 返回 / Esc |

### SYN 页

SYN 页数字是钢琴黑键，因此切页用 `Tab` / `Backspace`。

| 按键 | 行为 |
|---|---|
| 字母键 | 钢琴 |
| `2` `3` `5` `6` `7` | 高八度黑键 |
| `1` `4` `8` `9` `0` | 不切页、不导航 |
| `↑` `↓` | 八度 −2..+2 |
| `Fn` + `↑` `↓` | 选 patch |
| `←` `→` | 移动键位 |
| 按住 `Enter` | live note |
| `[` `]` | ARP / LATCH |

钢琴映射：

```text
低八度：Z S X D C V G B H N J M
高八度：Q 2 W 3 E R 5 T 6 Y 7 U
```

W/A/S/D 不再作为方向键；W/S/D 是钢琴键，A 忽略。

### SEQ 页

- `↑↓`：选轨；
- `←→`：选 step；
- `Enter`：放/删音块；
- `Fn+↑↓`：调 VEL；
- `Fn+←→`：调 GATE；
- 按住 `Fn` + 数字：切 pattern；
- 预览会使用当前 step 的 VEL/GATE。

### MIX / SYS

- MIX：`↑↓` 选轨，`←→` 调电平，`Enter` 静音；MASTER 行可调 master；
- SYS：`↑↓` 选行，`←→` 改值，`Enter` 执行。

### SYS 顺序

```text
BPM
Swing
Chain
Pattern
Pages
Length
Copy
Clear
Output
Volume
HP Gain
Backlight
Save
Load
Export MID
Demo
Song        FRACTURE
Battery
Tilt
Engine
Shutdown
```

---

## 构建

环境：

```text
ESP-IDF 5.5.6
D:\esp\v5.5\esp-idf
target: esp32s3
```

构建：

```bat
tools\idf.bat set-target esp32s3
tools\idf.bat build
```

`tools\idf.bat` 会清理 MSYS 环境、设置 `IDF_TOOLS_PATH=D:\esp\.tools`，再调用 IDF。

### 烧录

```bat
tools\idf.bat -p COMx flash monitor
```

一键镜像：

```text
build\opna_studio_cardputer_merged.bin  (0x0)
```

---

## 目录

```text
opna-studio-cardputer/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── main.cpp            显示/音频/SD/开机恢复/主循环
│   ├── ui.cpp              双屏 UI
│   ├── keyboard.cpp        TCA8418 映射
│   ├── persist.cpp         V4 存档 + V2/V3 迁移
│   ├── midi.cpp            SMF 导出
│   ├── stats.h             CPU/UND/FPS/耳机状态
│   └── Panel_ILI9341.hpp
├── components/
│   ├── M5Unified/
│   ├── m5gfx/
│   ├── opna_core/          YM2608 核心
│   └── studio/             engine / FFT / patches
├── docs/
│   ├── ui.md
│   ├── bringup.md
│   └── pinout.md
├── tools/idf.bat
├── README.md
├── DESIGN.md               内部设计说明（英文）
├── NOTICE.md
└── LICENSE                 Apache-2.0
```

---

## 已知限制

- 耳机音量预设为手动选择（硬件自动静音喇叭；ADV 无检测 GPIO）；
- SYN 页数字键用于钢琴，切页用 `Tab`/`Backspace`（设计行为）；
- 当前没有 USB MIDI 输入/输出，只有 `.mid` 文件导出（功能边界）；
- 没有外部音色包加载器（功能边界）；
- 512 KB SRAM 无 PSRAM，双 canvas 约 218 KB（硬件限制）；
- 全通道曲目会增加音频负载，必要时在 MIX 静音 FM5/FM6/SSG2（性能提示）。

---

## 调试

串口日志标签：

```text
opna-cardputer  启动、SD、音频、自动加载
kbd             键盘原始字符
piano           SYN 钢琴
persist         存档读写
midi            MIDI 导出
ui              SYS/耳机音量预设
```

SYS 手动切换后应出现：

```text
output -> HEADPHONES
audio vol=8/10 gain=24/1000 hp=1 override=1 att=3%
```

---

## 致谢

- **OPNA Studio P2**：引擎、音序器、视觉语言与移植基础；
- **pocket-fmdsp**：OPNA/PMD 语义与整数实现参考（Apache-2.0）；
- **area512**：Cardputer ADV 外部 ILI9341 + 共享 SPI2/SD 架构；
- **CardPuter_Mp3_Adv**：ES8311 / 耳机；
- **M5Unified / M5GFX / LovyanGFX**：M5 与显示基础库。

第三方代码与修改说明见 [`NOTICE.md`](NOTICE.md)。  
本项目使用 **Apache License 2.0**，见 [`LICENSE`](LICENSE)。
