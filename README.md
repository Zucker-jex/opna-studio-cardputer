# OPNA Studio Cardputer

> **面向 M5Stack Cardputer ADV 的双屏 OPNA FM 工作站**  
> 版本 **3.0** · 2026-11-27

OPNA Studio Cardputer 是把 **OPNA Studio P2** 的 OPNA 引擎、音序器和像素视觉语言，移植到 Cardputer ADV 的独立项目。

它不是一个简单的音频 demo：外屏 320×240 是主工作站，内屏 240×135 是演奏 HUD；支持 16 个 pattern、每轨音色、混音、双屏示波/FFT、SD 存档、MIDI 导出和完整键盘演奏。

---

## 当前状态

已确认或已实现：

- Cardputer ADV 双屏：内部 ST7789V2 240×135 + 外部 ILI9341 320×240；
- 双屏 canvas 使用 8-bit RGB332 双缓冲（256 色，复古但无闪屏，约 109 KB）；
- 外部屏与 SD 共用 SPI2，SD 挂载正常；
- ES8311 / I2S 48 kHz 立体声输出，扬声器与耳机均有声音；
- TCA8418 键盘；
- OPNA 引擎 55 466 Hz 原生渲染 → 48 kHz 重采样；
- 16 patterns、SEQ 编辑、MIX、SYS、SD 存档、MIDI 文件导出；
- **FILES 第六页**：SD 目录浏览、`.M` PMD 播放、`.P2S` 项目加载；
- **PMD 播放器**（pocket-fmdsp driver）：FM6/SSG3/Rhythm；
- **Save As**：命名保存并记录最后项目 `/sdcard/OPNA/opna.cfg`；
- PMD 播放时 SEQ 页显示只读监视器；
- 内置 16-pattern 曲目 **Fracture Protocol**（F# minor，132 BPM，全通道）。

当前固件构建：

```text
ESP-IDF 5.5.6 / esp32s3
App:    947 136 B (0xE73C0)
Merged: 1 012 672 B (0xF73C0)
```

> 原有双屏、音频、键盘、SEQ/MIX/SYS、存档和 MIDI 已在开发样机验证。
> v3.0 新增 PMD 播放、FILES 页、Save As 和日文标题，已编译通过并做过 host 侧 PMD 样本测试，
> 仍需在 Cardputer 上完成最终回归。
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
- `SYS`：播放、pattern 工具、音频、存档、导出、曲目和电源；
- `FILES`：SD 目录、`.M` 播放、`.P2S` 加载、PMD 状态。

**内部 240×135：演奏 HUD**

- 当前页、播放状态、SD；
- SEQ 轨电平 / 当前 step；
- SYN patch、键位、ARP/LATCH；
- MIX L/R VU；
- VIS 的反向视图（外屏 FFT ↔ HUD scope）；
- SYS/状态提示；
- FILES：当前目录、文件数、PMD/STUDIO 状态。

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

### PMD `.M` 播放

- 文件管理器打开 `.M`；
- 使用 pocket-fmdsp 的 PMD driver；
- 播放 FM1–FM6、SSG1–SSG3、Rhythm；
- 原生 55 466 Hz，继续走 48 kHz 重采样；
- PMD 模式在 SEQ 页显示只读监视器：
  - MIX 共 19 行（SSG3 拆 T/N，鼓拆 6 个声部），10 行可见，↑↓ 滚动：
    - FM1–FM6 + FM3B/FM3C/FM3D（共 9 个 FM 逻辑通道）；
    - SSG1/SSG2；
    - SSG3-T / SSG3-N（同一物理 SSG3 的 tone/noise 分量，共享 mute）；
    - BD / SD / TOP / HH / TOM / RIM（6 个独立鼓声部，独立 mute）；
  - 16 列：最近 16 个时间格；
  - 显示标题、活动音高；
  - 不能编辑；
- PMD 的 SEQ 监视器只记录音高变化事件，不再把持续音重复铺满 16 列；
- PMD 暂停时 SEQ 历史立即冻结，不再继续滚动到空白；
- VIS 页在 PMD 模式下显示 PMD 输出波形；
- VIS 页显示当前 RAM free/total/largest block；
- MIX 页在 PMD 模式下显示全部 19 行（SSG3 拆 T/N、鼓拆 6 声部），以及 L/R 输出 VU；
- PMD MIX 中 `↑↓` 选通道，`Enter` 开关 FM1–6 / SSG1–3 / DRM；
- `Space` 暂停/继续 PMD，仍停留在 PMD 模式；
- 只有打开 `.P2S`、SYS Load 或 Demo/Song 才回到 STUDIO；
- **PPZ8/ADPCM 不加载**，与 pocket-fmdsp 一致：PPZ8 静音，ADPCM stub。

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

### PMD 日文标题

PMD 标题使用 **Shift-JIS / CP932**。固件不再把非 ASCII 字符替换成 `?`，而是：

- 保留原始 CP932 标题；
- 用从 **Fusion Pixel 12px monospaced ja** 生成的 12×12 1bpp 字模；
- 在 PMD 的 FILES / SEQ 标题栏右对齐显示；
- 覆盖 9 762 个 CP932 字形；
- 未知字符回退为 `?`。

字模生成在：

```text
tools/gen_fusion_pixel_12_ja.py
main/jp_font_data.h
```

源码 TTF 不进入运行固件，生成后已从仓库删除；字体授权为 SIL OFL 1.1，见 `docs/FUSION-PIXEL-OFL.txt`。

### 配色文件

首次启动会自动生成：

```text
/sdcard/OPNA/COLORS.CFG
```

- 文件存在时，启动会读取并应用；
- 文件不存在时，使用内置配色并自动生成；
- 编辑后重启生效；
- 文件格式：

```text
# NAME RRGGBB
BG 101826
PANEL 202E42
TEXT D8DCE8
GOLD E0B060
TRK0 E0B060
ACC0 E0B060
```

可用名称：

```text
BG BG2 PANEL PANEL2 LINE TEXT DIM DIMMER
GOLD CYAN GREEN RED ORANGE PURPLE WHITE YELLOW
TRK0..TRK9
ACC0..ACC5
```

### 存储

- SD 路径：`/sdcard`；
- 固件输出目录：`/sdcard/OPNA`；
- 存档：`/sdcard/OPNA/opna2.p2s` 或 `Save As` 命名文件；
- 最后项目：`/sdcard/OPNA/opna.cfg`；
- 当前格式 **V4**，7 739 B；
- 保存 song、16 patterns、长度、BPM、swing、chain、pattern、pages、音量、背光、HP Gain；
- 开机优先加载 `OPNA/opna.cfg` 记录的项目，失败回退 `OPNA/opna2.p2s`，再回退旧根目录 `opna2.p2s`；
- V3/V2 会自动迁移；
- `SYS → Save As` 使用 8.3 大写文件名和 `.P2S` 扩展名。

### MIDI

- SYS → `Export MID`；
- 输出 `/sdcard/OPNA/opna2.mid`；
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
| 非 SYN 页面 `1`–`6` | 直接切页（6 = FILES） |
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

### FILES

- `↑↓` 选择文件；
- `Enter` 进入目录 / 打开 `.M` / 加载 `.P2S`；
- `Backspace` 返回上级目录；
- `Tab` 下一页；
- `.M` 打开后进入 PMD 播放；
- `.P2S` 加载项目并回到 SEQ；
- PMD 播放时 `Space` 暂停/继续，仍停留在 PMD；
- 打开 `.P2S` 或 SYS `Demo`/`Song` 才回到 STUDIO。

### MIX / SYS

- MIX：`↑↓` 选轨，`←→` 调电平，`Enter` 静音；MASTER 行可调 master；
- SYS：`↑↓` 选行，`←→` 改值，`Enter` 执行；
- SYS → `Save As` 进入文件名编辑：
  - 字母/数字/下划线；
  - `Enter` 保存；
  - `Backspace` 删除；
  - `` ` `` 取消。

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
Save As
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

### GitHub Actions

工作流：

```text
.github/workflows/release.yml
```

触发：

- push tag `v3.0`、`3.0`、`v0.5.0` 或 `1.0`；
- 手动 `workflow_dispatch`（只生成 artifact，不创建 release）。

流水线使用 ESP-IDF 5.5.6 / esp32s3，执行：

```text
idf.py set-target esp32s3
idf.py build
idf.py merge-bin
```

发布内容：

- `opna-studio-cardputer-<version>-merged.bin`（0x0 烧录）；
- app bin；
- bootloader / partition table；
- `SHA256SUMS`；
- 包含 README/LICENSE/NOTICE 的 zip。

---

## 目录

```text
opna-studio-cardputer/
├── .github/workflows/release.yml
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── main.cpp            显示/音频/SD/开机恢复/主循环
│   ├── ui.cpp              双屏 UI
│   ├── keyboard.cpp        TCA8418 映射
│   ├── persist.cpp         V4 存档 + Save As + last cfg
│   ├── file_manager.cpp    SD 目录浏览
│   ├── pmd_audio.cpp       PMD .M 播放桥
│   ├── midi.cpp            SMF 导出
│   ├── stats.h             CPU/UND/FPS/耳机状态
│   └── Panel_ILI9341.hpp
├── components/
│   ├── M5Unified/
│   ├── m5gfx/
│   ├── opna_core/          YM2608 核心
│   ├── pmd_core/           pocket-fmdsp PMD driver
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
- PMD 的 PPZ8/ADPCM 声部不加载：PPZ8 静音，ADPCM stub；
- FILES 只浏览 `/sdcard`，8.3 文件名优先；
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
