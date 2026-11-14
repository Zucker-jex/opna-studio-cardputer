# OPNA Studio Cardputer — engineering design

**v0.5.0** · M5Stack Cardputer ADV · ESP32-S3FN8 · external ILI9341
320×240 · internal ST7789 240×135.

This document is the plan for the Cardputer port. The reference P2 repository
is frozen; all new work lives in this folder.

---

## 1. Hardware and buses

### 1.1 Displays

| Display | Controller | Resolution | Bus | Notes |
|---------|------------|------------|-----|-------|
| Internal | ST7789V2 | 240×135 | **SPI3** | `M5Cardputer.Display`, rotation 1 |
| External | ILI9341 | 320×240 | **SPI2** | `lgfx::Panel_ILI9341`, rotation 1, 20 MHz |

External / SD pins:

```
SCK   40
MOSI  14
MISO  39
LCD CS 5
LCD DC 6
LCD RST 3
SD CS  12
```

The external display and SD card **share SPI2**. This is the area512-proven
arrangement. The standalone `ILI9341_demo` uses SPI3 at 40 MHz and no SD; it is
only a panel/rotation reference, not the dual-screen bus plan.

### 1.2 Audio

| Signal | Pin |
|--------|-----|
| I2C SDA / SCL | 8 / 9 |
| ES8311 address | 0x18 |
| I2S BCLK | 41 |
| I2S LRCK | 43 |
| I2S DOUT | 42 |
| Headphone detect | 17 (LOW = inserted) |
| Speaker AMP_EN | 46 (HIGH = enabled) |

The ES8311 is the ADV codec; the speaker is driven by NS4150B (mono). The
headphone jack is stereo. Do **not** initialize `M5.Speaker`: it takes an I2S
peripheral and conflicts with the custom ES8311 path.

### 1.3 Input / sensors

- Keyboard: TCA8418, I2C 0x34, 4×14 matrix.
- IMU: BMI270 (optional tilt in SYN).
- IR LED, microphone: not used by OPNA Studio v0.4.
- Battery ADC: GPIO10 (verify against the ADV schematic before shipping a
  battery gauge).

---

## 2. Display pipeline

### 2.1 Logical screens

Both screens are landscape and have different content:

```
External 320×240                  Internal 240×135
┌──────────────────────────┐      ┌────────────────────┐
│ OPNA STUDIO  ▶ PLAY ...  │      │ SEQ   ▶   CPU 42%  │
│ SEQ                      │      │ track  03 step 07  │
│ 10 tracks × 16 steps     │      │ ████ ███ ██ ...    │
│                          │      │ pattern A–P  BPM 118 │
│ hint / tabs              │      │ hint               │
└──────────────────────────┘      └────────────────────┘
```

- **External** is the full workstation UI.
- **Internal** is a performance HUD with page-specific secondary data.
- Both use the P2 palette and 8×8 Misaki font; the external may use 2× titles.

### 2.2 Framebuffers / memory

Target: **512 KB SRAM, no PSRAM**.

| Buffer | Bytes |
|--------|-------|
| External 320×240 RGB565 | 153 600 |
| Internal 240×135 RGB565 | 64 800 |
| OPNA engine + song + FFT | ~55 000 |
| audio ring / stacks / M5/SD | budget ~80 000 |
| **total** | **~355 000 / 524 288** |

This is feasible, but leaves limited headroom. If memory becomes tight:

1. keep the external framebuffer full;
2. draw the internal HUD directly or in row strips;
3. avoid a second full internal framebuffer;
4. do not allocate large `M5Canvas` objects per frame.

### 2.3 Refresh

- External SPI2 at 20 MHz: full 320×240 ≈ **61 ms**; page changes only.
- Internal SPI3: much faster; HUD at 20 fps.
- Input/animation: dirty rectangles only (P2 `ui_draw_dirty` model).
- External FFT/VU: partial bars, not full content.
- If 40 MHz proves stable with SD idle, it can cut the full-frame time to
  ~30 ms, but **20 MHz is the default**.

---

## 3. UI design

### 3.1 External 320×240

Vertical regions:

| Y | Region |
|---|--------|
| 0–15 | status: title, transport, BPM, CPU, UND, SD, battery |
| 16–43 | header: 2× page title + context; accent line at 43 |
| 44–208 | page content |
| 209–222 | hint |
| 223–239 | five tabs, 64 px each |

- **SEQ**: track labels 0–55; 16 steps at 16 px; 10 tracks at 14 px; pattern
  bar; ruler; footer velocity/gate.
- **SYN**: 8 patch cards; two-octave keyboard; ARP/LATCH/TILT; mini scope.
- **MIX**: 10 track rows + master; per-track mute/level; L/R VU.
- **VIS**: 64 FFT bars across 320 px (4 px/bar) + peak hold; triggered scope
  mode; CPU/FM/SSG/DRM/UND.
- **SYS**: 13 rows in two columns if useful; volume, output, backlight, tempo,
  swing, chain, pattern, save/load, demo, MEMS, touch-cal (removed), shutdown.

### 3.2 Internal 240×135 HUD

| Y | Region |
|---|--------|
| 0–15 | page, play/stop, CPU, UND |
| 16–110 | page-specific secondary view |
| 111–134 | hint / status |

Per page:

- **SEQ** — 10 horizontal track-level bars, current track/step/pattern, BPM.
- **SYN** — large note name, octave, patch, ARP/LATCH, mini scope, tilt.
- **MIX** — L/R VU + master; muted tracks compact list.
- **VIS** — the analysis mode not shown on the external screen (FFT ↔ scope).
- **SYS** — SD, battery, volume, backlight, key help.

The HUD never accepts input; keyboard actions are global.

---

## 4. Keyboard

Cardputer-native mapping (final v0.4 policy):

| Key | Action |
|-----|--------|
| `;` / `.` / `,` / `/` | Up / Down / Left / Right |
| `Enter` | primary action |
| `Space` | play / stop |
| `Tab` | next page |
| `Backspace` | previous page |
| `Fn` | MOD toggle |
| hold `Fn` + `1`–`0` | pattern A–J |
| `1`–`5` | page select on non-SYN pages |
| `Z S X D C V G B H N J M` | SYN lower octave |
| `Q 2 W 3 E R 5 T 6 Y 7 U` | SYN upper octave |

Page actions:

- **SEQ**: arrows select; `Enter` place/delete; hold `Fn`+1–0 pattern A–P;
  `-`/`=` BPM; `Fn` MOD for VEL/GATE edits.
- **SYN**: piano keys direct; `Tab`/`Backspace` pages; `Fn` MOD for patch;
  `Space` play/stop; `[`/`]` arp/latch. SYN digits are piano black keys.
- **MIX**: arrows row/level; `Enter` mute; master at the last row.
- **VIS**: `Enter` toggle FFT/scope.
- **SYS**: arrows row; left/right value; `Enter` execute.

No P2 triple-C requirement; Tab/Backspace are deterministic page keys, Fn is the MOD modifier.

---

## 5. Audio pipeline

```
OPNA engine 55 466 Hz stereo int16
        │
        ├─ 4-tap / linear 55 466 → 48 000 resampler
        │
        └─ I2S0 48 kHz 16-bit stereo
              │
        ES8311 codec ── headphone (stereo)
              └──────── NS4150B (mono, AMP_EN 46)
```

- Engine stays native 55 466 Hz; do not regenerate `notes.h` or drum PCM.
- Resampler runs in the audio task; audio task pin core 1, UI core 0.
- I2S DMA: 4–6 × 256 frames; ~21–32 ms latency.
- ES8311 init: use the `CardPuter_Mp3_Adv` register sequence.
- The ADV connector has no headphone-detect GPIO. The MCU holds AMP_EN high;
  the jack's Q4 pulls it low on insertion, muting NS4150B in hardware.
  SYS Output is only a software volume preset; HP Gain sets headphone level.
- `M5.Speaker` is disabled; it conflicts with the custom ES8311/I2S path.
- Test with 440 Hz/1 kHz before the OPNA engine; verify pitch, stereo,
  manual speaker/headphone output and AMP_EN gating.

---

## 6. Engine port

Reuse from P2:

- `src/opna/`, `include/pfm/`;
- `firmware/studio/engine.c`, `patches.c`, `notes.h`, `fft.c`;
- host renderer/tests;
- `/sdcard/opna2.p2s` V4 (7 739 B, 16 patterns + lengths); V2 (1 963 B) auto-migrates.

Platform work:

- `PFM_HOT` → `IRAM_ATTR` or empty; do not link P2 `.ramfunc`;
- `pfm_prof` DWT → `esp_cpu_get_cycle_count()`;
- FreeRTOS tasks: audio core 1, UI/display core 0;
- `board_*` replaced by Cardputer BSP;
- remove touch ADC, calibration, BKP, joystick;
- audio output 48 kHz via resampler;
- SD `/opna2.p2s` on `/sdcard` VFS.

The OPNA core is freestanding C and has no ARM assembly; the main risks are
the platform macros and cache/IRAM behavior, not the DSP.

---

## 7. SD and persistence

- SD on SPI2, CS12, shared with the external display;
- mount after the external display initializes the bus;
- use ESP-IDF `sdspi` / VFS (area512 approach) or an equivalent shared-bus
  driver; do **not** initialize a second SPI host on the same pins;
- `/opna2.p2s` remains the format; writes pause audio/mute to avoid underruns;
- no BKP: volume/backlight etc. go into the same file;
- no touch calibration.

---

## 8. Build framework

Chosen baseline: **ESP-IDF 5.5.6 + M5Unified/M5GFX components**, built with
the local `D:\esp` toolchain.

Why:

- `D:\esp` is complete and offline; PlatformIO's package download was the
  build blocker;
- `area512` is already an ESP-IDF project built with this exact toolchain and
  supplies the external ILI9341 + shared SPI2/SD architecture;
- the P2 engine is C/FreeRTOS/CMake and fits ESP-IDF naturally;
- M5Unified/M5GFX are vendored under `components/` (from area512), so no
  network is needed;
- `CardPuter_Mp3_Adv` supplies the ADV ES8311/headphone/amp register sequence,
  ported to ESP-IDF I2S;
- the project can pin audio to core 1 and UI to core 0.

The Arduino `M5Cardputer` keyboard library is not available; use the
TCA8418 driver pattern from area512 instead.

Build wrapper `tools/idf.bat`:

```bat
@echo off
set "MSYSTEM="
set "MINGW_PREFIX="
set "IDF_TOOLS_PATH=D:\esp\.tools"
call "D:\esp5.5\esp-idf\export.bat"
cd /d "%~dp0.."
idf.py %*
```

---

## 9. Phase plan

### Phase 0 — bring-up

- internal 240×135 and external 320×240 both draw; ✅ build verified
- external panel rotation/color correct; hardware test pending
- SD mount on the shared SPI2; hardware test pending
- ES8311 48 kHz test tone, headphone/amp gating; hardware confirmed
- TCA8418 key scan (raw codes logged); hardware test pending
- OPNA engine imported: native 55 466 Hz → linear 48 kHz resampler → I2S0
- external 320×240 UI + internal 240×135 HUD: hardware confirmed
- interactive SEQ/MIX/SYS + `/sdcard/opna2.p2s` save/load: build verified

### Phase 1 — engine/audio

- OPNA core build;
- 55 466 → 48 000 resampler;
- 6 FM CPU/UND measurement;
- SD load/save.

### Phase 2 — external UI

- 320×240 five pages;
- P2 palette/font;
- dirty rendering.

### Phase 3 — internal HUD

- 240×135 HUD;
- keyboard mapping;
- dual-screen refresh.

### Phase 4 — release

- audio tuning, power, battery, polish;
- documentation/media.

---

## 10. Known risks

1. **SPI2 sharing** between ILI9341 and SD; CS/transaction correctness.
2. **20 MHz full-frame** is ~61 ms; full-screen animation must be avoided.
3. **ES8311 at 48 kHz** needs validation (the reference test used 44.1 kHz).
4. **No PSRAM**: two full framebuffers plus engine is tight but feasible.
5. **ADV keyboard** TCA8418 final mapping has been exercised on hardware; still do a full regression before release.
6. **No touch/joystick**: all P2 gesture semantics are replaced by keys.
7. **M5.Speaker conflict**: must stay disabled.
