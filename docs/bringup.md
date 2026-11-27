# 上板验证清单

## 构建与烧录

```bat
tools\idf.bat build
tools\idf.bat -p COMx flash monitor
```

一键镜像：

```text
build\opna_studio_cardputer_merged.bin  (0x0)
```

## 启动

预期日志：

```text
external ILI9341 320x240 on SPI2 @20MHz
SD mounted at /sdcard
SD write probe: wrote=4 flush=0 close=0
ES8311 I2S 48k stereo ready
TCA8418 ready
persist: load v4 /sdcard/OPNA/TEST.P2S
auto-load: applied last project
phase 3 ready
OPNA engine task started
```

如果没有存档：

```text
auto-load: no save, fracture
```

## 硬件

1. 内外屏都点亮；
2. 外屏 UI 320×240 完整；
3. SD 挂载，`SD write probe` 成功；
4. 扬声器有声；
5. 键盘有 `kbd:` 日志；
6. 耳机插入后有声；
7. 倾斜机身时 SYN live 有弯音；
8. 电池百分比合理。

## UI / 音频

1. 外屏 SEQ/SYN/MIX/VIS/SYS 五页；
2. 内屏 HUD 跟随；
3. MIX L/R VU 动态；
4. VIS CPU/UND/FPS 有数值；
5. SYN 页字母/黑键发声；
6. Fn 切换 MOD；
7. Tab / Backspace 翻页；
8. 直接 `; . , /` 移动，不需要 Fn；
9. 非 SYN 页直接 1–5 切页；
10. SYN 页 Tab/Backspace 翻页。

## SEQ

1. Enter 放/删音块；
2. Fn+↑↓ 改 VEL，预览音量变化；
3. Fn+←→ 改 GATE，预览音长变化；
4. 播放时 VEL/GATE 生效；
5. SYN 页弹一个音，回 SEQ 看 `PEN` 是否同步；
6. pattern A–P、Pages、Length、Copy、Clear；
7. Copy 复制到下一个 pattern；
8. Clear 清空当前 pattern。

## SYN

1. 字母/黑键钢琴；
2. `↑↓` 八度；
3. `Fn+↑↓` patch；
4. `←→` 键位；
5. `Enter` live；
6. `[` / `]` ARP/LATCH；
7. patch 修改后回 SEQ 播放应改变音色；
8. W/A/S/D 不再方向。

## SYS

顺序：

```text
BPM / Swing / Chain / Pattern / Pages / Length / Copy / Clear
Output / Volume / HP Gain / Backlight
Save / Load / Export MID / Demo / Song / Battery / Tilt / Engine / Shutdown
```

1. Volume / Backlight；
2. Output 手动 Speaker / Headphones（只改音量预设）；
3. HP Gain 3% / 1%；
4. Save 后重启恢复；
5. Export MID 写 `/sdcard/OPNA/opna2.mid`；
6. Song 载入 Fracture Protocol；
7. Shutdown 最后测试。

## 耳机

ADV 连接座没有耳机检测 GPIO，软件不检测插拔；喇叭静音由硬件 Q4 完成。

1. 插入耳机，确认喇叭自动静音；
2. SYS → Output 切到 `HEADPHONES`（只改变软件音量预设）；
3. SYS → HP Gain 调到 3% 或 1%；
4. 串口应出现：

```text
phones volume mode -> HEADPHONES
audio vol=8/10 gain=24/1000 hp=1 override=1 att=3%
```

拔掉耳机后喇叭恢复；需要时把 Output 切回 `Speaker`。
AMP_EN 始终由 MCU 保持高，插入耳机时由 Q4 拉低。

## PMD / FILES

1. 将 `TH04_07.M` 放到 `/sdcard`；
2. 按 `6` / Tab 进入 FILES；
3. 选中 `.M`，Enter 播放；
4. SEQ 页显示 PMD 只读监视器；
5. Space 暂停/继续，仍停留在 PMD；
5. 打开 `.P2S` 或 SYS Demo/Song 才回到 STUDIO；
5a. SEQ 监视器不应把持续音重复铺满 16 列；
5b. VIS 页应显示 PMD 输出波形；
5c. VIS 页应显示 RAM free/total/blk；
5d. MIX 页应显示 PMD 19 行（10 行可见，↑↓ 滚动）与 L/R VU；SSG3-T/N 分别反映 tone/noise，BD/SD/TOP/HH/TOM/RIM 独立开关；
5e. `/sdcard/OPNA/COLORS.CFG` 应自动生成，修改后重启生效；
6. PPZ8/ADPCM 声部保持静音，这是与 pocket-fmdsp 一致的行为。

## Save As / 最后项目

1. SYS → `Save As`；
2. 输入 8.3 名称，Enter 保存；
3. 确认 `/sdcard/OPNA/NAME.P2S` 和 `/sdcard/OPNA/opna.cfg`；
4. 重启应加载最后项目；
5. 缺少时回退 `/sdcard/OPNA/opna2.p2s`，再回退旧根目录 `opna2.p2s`。

## MIDI

- SYS → Export MID；
- 确认 `/sdcard/OPNA/opna2.mid`；
- DAW 打开，检查 16 patterns、鼓通道 10。

## 设计说明 / 注意事项

- 耳机音量预设为手动选择；喇叭静音由 Q4 硬件完成。
- 手动 `Output` 与 `HP Gain` 是设计路径。
- 全通道 Fracture Protocol 会增加音频负载；必要时在 MIX 静音通道。
- 512 KB SRAM 无 PSRAM，双 canvas 约 218 KB。
