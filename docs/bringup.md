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
persist: load v4 /sdcard/opna2.p2s
auto-load: applied /sdcard/opna2.p2s
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
5. Export MID 写 `/sdcard/opna2.mid`；
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

## MIDI

- SYS → Export MID；
- 确认 `/sdcard/opna2.mid`；
- DAW 打开，检查 16 patterns、鼓通道 10。

## 设计说明 / 注意事项

- 耳机音量预设为手动选择；喇叭静音由 Q4 硬件完成。
- 手动 `Output` 与 `HP Gain` 是设计路径。
- 全通道 Fracture Protocol 会增加音频负载；必要时在 MIX 静音通道。
- 512 KB SRAM 无 PSRAM，双 canvas 约 218 KB。
