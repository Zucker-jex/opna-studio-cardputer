# 双屏 UI 与按键

## 外屏 320×240：主工作站

```text
y   0..17    状态栏：OPNA STUDIO / PLAY / BPM / PAT / SD / MOD
y  18..47    页标题 + 当前上下文
y  48..206   页面内容
y 207..223   快捷键提示
y 224..239   五个页签：SEQ SYN MIX VIS SYS
```

### SEQ

- 10 轨 × 16 steps；
- pattern A–P，超过 `Pages` 上限的 pattern 变暗；
- 当前 pattern 金色，当前 step 白色框；
- footer 显示 VEL / GATE / PEN；
- 超过 pattern 长度的 step 变暗。

### SYN

- 8 个 patch 卡片；
- 两八度键盘，字母/黑键直接演奏；
- 顶部显示 `PATCH / TRK / OCT / PEN`；
- 底部一行 OCT 选择条：`-2 -1 0 +1 +2`；
- `Fn+↑↓` 修改当前 SEQ FM 轨的 patch 并写回；
- 按住 `Enter` 或直接按字母键 live；
- `[` / `]` 切换 ARP / LATCH。

### MIX

- 10 轨 + MASTER；
- 电平条、MUTE；
- L/R VU 每 UI tick 更新；
- MASTER 可选中并用 `< >` 调整。

### VIS

- 64-bin FFT / 触发 scope；
- `Enter` 切换；
- 底部显示 CPU / UND / FPS。

### SYS

滚动列表，10 行可见。顺序：

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

- `^ v` 选行；
- `< >` 改值；
- `Enter` 执行；
- `Output` 循环 Speaker ↔ Headphones（只改耳机音量预设；喇叭静音由硬件 Q4 完成）；
- `HP Gain` 循环 50 / 25 / 12 / 6 / 3 / 1%，默认 3%；
- `Length` 当前 pattern 长度 1–16；
- `Copy` 当前 pattern → 下一个；
- `Clear` 清空当前 pattern；
- `Export MID` 写 `/sdcard/opna2.mid`；
- `Song` 载入 Fracture Protocol；
- `Shutdown` 关机。

## 内屏 240×135：演奏 HUD

```text
y   0..15    当前页 / PLAY / SD
y  16..110   页面相关 HUD
y 111..134   快捷键提示
```

- SEQ：10 条轨电平 + step/pattern；
- SYN：patch 名、键位、OCT、ARP/LATCH；
- MIX：L/R VU + master；
- VIS：与外屏相反的 FFT/scope；
- SYS：SD、chain、状态。

## 按键

### 全局

```text
Tab         下一页
Backspace   上一页
; . , /     上 / 下 / 左 / 右（始终有效）
Enter       执行 / live
Space       播放 / 停止
Fn          切换 MOD
Fn + 1..0   选择 pattern A–J
`           返回 / Esc
```

非 SYN 页 `1`–`5` 直接切页。  
SYN 页数字是钢琴键，所以切页用 `Tab` / `Backspace`。

### SYN

```text
字母键       钢琴
2 3 5 6 7    高八度黑键
1 4 8 9 0    不切页、不导航
↑↓           八度 -2..+2
Fn + ↑↓      选 patch
←→           键位
按住 Enter   live
[ ]          ARP / LATCH
```

钢琴映射：

```text
低八度：Z S X D C V G B H N J M
高八度：Q 2 W 3 E R 5 T 6 Y 7 U
```

W/A/S/D 不再是方向键；W/S/D 是钢琴键，A 忽略。

### SEQ

```text
↑↓           选轨
←→           选 step
Enter        放/删音块
Fn + ↑↓      调 VEL
Fn + ←→      调 GATE
按住 Fn + 数字  切 pattern
```

### MIX / SYS

- MIX：`↑↓` 选轨，`←→` 调电平，`Enter` 静音；
- SYS：`↑↓` 选行，`←→` 改值，`Enter` 执行。

## 颜色

| 名称 | RGB | 用途 |
|---|---|---|
| BG | 8,10,18 | 背景 |
| PANEL | 20,26,46 | 面板 |
| GOLD | 255,212,94 | 主色 / 选中 |
| CYAN | 99,217,255 | 数值 / 播放 |
| GREEN | 99,242,166 | 播放 / SYN |
| RED | 255,92,110 | 静音 / 错误 |
| PURPLE | 184,140,255 | SYS 标记 |

## 刷新模型

- 外屏：200 ms 或按键后全量重绘；320×240 @20 MHz 约 61 ms；
- 内屏：50 ms；
- 音频任务固定在 core 1，UI/显示在 core 0；
- UI 与引擎修改通过 `studio_lock()` 串行化。
