/**
 * @file main.cpp
 * @brief OPNA Studio Cardputer board bring-up and runtime plumbing.
 *
 * Responsibilities:
 *   - internal ST7789V2 (M5GFX/SPI3) and external ILI9341 (SPI2, 20 MHz);
 *   - microSD on the same SPI2 bus (LCD CS5, SD CS12);
 *   - ES8311 + I2S0 at 48 kHz and the 55,466 -> 48,000 resampler;
 *   - TCA8418 polling, auto-load of /sdcard/opna2.p2s;
 *   - engine mutex shared by the core-1 audio task and the core-0 UI.
 *
 * Boot order is deliberate: SD -> audio -> studio -> persist -> UI -> audio
 * task, so the first HUD frame already shows the restored song.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#include <M5Unified.hpp>
#include <M5GFX.h>

#include "Panel_ILI9341.hpp"
#include "studio.h"
#include "ui.h"
#include "keyboard.h"
#include "studio_guard.h"
#include "persist.h"
#include "stats.h"
#include "freertos/semphr.h"

static const char *TAG = "opna-cardputer";

static lgfx::Bus_SPI      s_ext_bus;
static lgfx::Panel_ILI9341 s_ext_panel;
static lgfx::LGFX_Device  s_ext;

static constexpr int PIN_EXT_SCK  = 40;
static constexpr int PIN_EXT_MOSI = 14;
static constexpr int PIN_EXT_MISO = 39;
static constexpr int PIN_EXT_CS   = 5;
static constexpr int PIN_EXT_DC   = 6;
static constexpr int PIN_EXT_RST  = 3;
static constexpr int PIN_SD_CS    = 12;

static sdmmc_card_t *s_card = nullptr;
volatile int g_volume = 8;
volatile int g_backlight = 180;
volatile int g_cpu_pct = 0;
volatile int g_underruns = 0;
volatile int g_fps = 0;
volatile int g_tilt_x = 0;
volatile int g_tilt_y = 0;
volatile int g_headphones = 0;
volatile int g_hp_override = 0;  /* manual: 0=speaker, 1=headphones */
volatile int g_hp_att = 3;  /* headphone gain, percent of speaker scale */
static SemaphoreHandle_t s_studio_mtx = nullptr;

void studio_guard_init(void) {
  s_studio_mtx = xSemaphoreCreateMutex();
}
void studio_lock(void) {
  if (s_studio_mtx) xSemaphoreTake(s_studio_mtx, portMAX_DELAY);
}
void studio_unlock(void) {
  if (s_studio_mtx) xSemaphoreGive(s_studio_mtx);
}
static i2s_chan_handle_t s_i2s_tx = nullptr;
static bool s_audio_ok = false;

/* No headphone-detect GPIO is routed on the ADV Stamp-S3 connector.
 * AMP_EN is held HIGH by the MCU; the jack's Q4 pulls it LOW in hardware
 * when headphones are inserted. SYS "Output" is only a software volume
 * preset (Speaker / Headphones); it must not fight the hardware gate. */
static constexpr int PIN_AMP_EN     = 46;  /* HIGH = speaker amp on */
static constexpr uint8_t ES8311_ADDR = 0x18;
static constexpr uint8_t TCA8418_ADDR = 0x34;

static bool init_external_display() {
  auto b = s_ext_bus.config();
  b.spi_host   = SPI2_HOST;
  b.spi_mode   = 0;
  b.freq_write = 20000000;
  b.freq_read  = 16000000;
  b.spi_3wire  = false;
  b.use_lock   = true;
  b.dma_channel = 1;
  b.pin_sclk = PIN_EXT_SCK;
  b.pin_mosi = PIN_EXT_MOSI;
  b.pin_miso = PIN_EXT_MISO;
  b.pin_dc   = PIN_EXT_DC;
  s_ext_bus.config(b);
  s_ext_panel.setBus(&s_ext_bus);

  auto p = s_ext_panel.config();
  p.pin_cs        = PIN_EXT_CS;
  p.pin_rst       = PIN_EXT_RST;
  p.bus_shared    = true;
  p.readable      = false;
  p.invert        = false;
  p.rgb_order     = false;
  p.dlen_16bit    = false;
  p.memory_width  = 240;
  p.memory_height = 320;
  p.panel_width   = 240;
  p.panel_height  = 320;
  p.offset_x      = 0;
  p.offset_y      = 0;
  p.offset_rotation = 0;
  s_ext_panel.config(p);

  s_ext.setPanel(&s_ext_panel);
  if (!s_ext.init()) {
    ESP_LOGE(TAG, "external ILI9341 init failed");
    return false;
  }
  s_ext.setColorDepth(16);
  s_ext.setRotation(1);       /* 320x240 landscape */
  s_ext.fillScreen(0x0000);
  ESP_LOGI(TAG, "external ILI9341 320x240 on SPI2 @20MHz");
  return true;
}


/* ------------------------------------------------------------------------- */
/* ES8311 / I2S test tone                                                    */
/* ------------------------------------------------------------------------- */
static bool es8311_write(uint8_t reg, uint8_t value) {
  return M5.In_I2C.writeRegister8(ES8311_ADDR, reg, value, 400000);
}

static bool init_es8311() {
  static const uint8_t seq[][2] = {
    {0x00, 0x80}, {0x01, 0xB5}, {0x02, 0x18}, {0x0D, 0x01},
    {0x12, 0x00}, {0x13, 0x10}, {0x32, 0xBF}, {0x37, 0x08},
  };
  bool ok = true;
  for (auto& r : seq) ok = es8311_write(r[0], r[1]) && ok;
  return ok;
}

static void init_amp_gpio() {
  gpio_config_t out = {};
  out.pin_bit_mask = 1ULL << PIN_AMP_EN;
  out.mode = GPIO_MODE_OUTPUT;
  gpio_config(&out);
  gpio_set_level((gpio_num_t)PIN_AMP_EN, 1);  /* Q4 pulls low on HP insert */
}

static void update_amp() {
  int hp = g_hp_override ? 1 : 0;
  if (hp != g_headphones) {
    ESP_LOGI(TAG, "phones volume mode -> %s",
             hp ? "HEADPHONES" : "SPEAKER");
    g_headphones = hp;
  }
  /* Hardware Q4 owns the actual speaker mute on jack insertion. */
  gpio_set_level((gpio_num_t)PIN_AMP_EN, 1);
}

static bool init_audio() {
  init_amp_gpio();
  if (!init_es8311()) ESP_LOGW(TAG, "ES8311 init failed");

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  if (i2s_new_channel(&chan_cfg, &s_i2s_tx, nullptr) != ESP_OK) {
    ESP_LOGE(TAG, "i2s_new_channel failed");
    return false;
  }

  i2s_std_config_t cfg = {};
  cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000);
  cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                I2S_SLOT_MODE_STEREO);
  cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  cfg.gpio_cfg.bclk = (gpio_num_t)41;
  cfg.gpio_cfg.ws   = (gpio_num_t)43;
  cfg.gpio_cfg.dout = (gpio_num_t)42;
  cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;

  if (i2s_channel_init_std_mode(s_i2s_tx, &cfg) != ESP_OK ||
      i2s_channel_enable(s_i2s_tx) != ESP_OK) {
    ESP_LOGE(TAG, "I2S init/enable failed");
    return false;
  }
  update_amp();
  s_audio_ok = true;
  ESP_LOGI(TAG, "ES8311 I2S 48k stereo ready");
  return true;
}

static void play_test_tone(uint32_t hz, uint32_t ms) {
  if (!s_audio_ok) return;
  static int16_t buf[256 * 2];
  float phase = 0.0f;
  float inc = 2.0f * 3.14159265f * (float)hz / 48000.0f;
  uint32_t chunks = (ms * 48000u) / (1000u * 256u);
  for (uint32_t c = 0; c < chunks; c++) {
    for (int i = 0; i < 256; i++) {
      int16_t v = (int16_t)(8000.0f * sinf(phase));
      phase += inc;
      if (phase > 6.2831853f) phase -= 6.2831853f;
      buf[2 * i] = v;
      buf[2 * i + 1] = v;
    }
    size_t written = 0;
    i2s_channel_write(s_i2s_tx, buf, sizeof(buf), &written, portMAX_DELAY);
  }
}

static bool mount_sd() {
  // The external display has already initialized SPI2.  Use the same host
  // with a separate CS (12) as area512 does.
  gpio_set_pull_mode((gpio_num_t)PIN_EXT_MISO, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)PIN_EXT_MOSI, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)PIN_EXT_SCK,  GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)PIN_SD_CS,    GPIO_PULLUP_ONLY);

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI2_HOST;
  host.max_freq_khz = 20000;

  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = (gpio_num_t)PIN_SD_CS;
  slot.host_id = SPI2_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
  mount_cfg.format_if_mount_failed = false;
  mount_cfg.max_files = 4;
  mount_cfg.allocation_unit_size = 16 * 1024;

  esp_err_t err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot,
                                          &mount_cfg, &s_card);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(err));
    s_card = nullptr;
    return false;
  }
  ESP_LOGI(TAG, "SD mounted at /sdcard (%s)", s_card->cid.name);

  FILE *probe = fopen("/sdcard/OPNA.TXT", "wb");
  if (!probe) {
    ESP_LOGE(TAG, "SD write probe open failed errno=%d", errno);
  } else {
    size_t n = fwrite("OPNA", 1, 4, probe);
    int fr = fflush(probe);
    int fc = fclose(probe);
    ESP_LOGI(TAG, "SD write probe: wrote=%u flush=%d close=%d", (unsigned)n,
             fr, fc);
    remove("/sdcard/OPNA.TXT");
  }
  return true;
}


/* ------------------------------------------------------------------------- */
/* Audio task (core 1): engine -> 55,466 -> 48,000 resampler -> I2S0         */
/* The engine lock is held only while the native block is rendered; I2S     */
/* writes and statistics stay outside it.                                    */
/* ------------------------------------------------------------------------- */
static void studio_audio_task(void *arg) {
  (void)arg;
  static int16_t native[1024 * 2];
  static int16_t src[(1024 + 1) * 2];
  static int16_t out[900 * 2];
  int16_t last_l = 0, last_r = 0;
  uint32_t phase = 0;
  const uint32_t step = 75728;  /* 55466/48000 * 65536 */

  ESP_LOGI(TAG, "OPNA engine task started: 55466 Hz -> 48000 Hz");

  while (true) {
    uint32_t t0 = (uint32_t)esp_timer_get_time();
    studio_lock();
    bool idle = studio_is_idle();
    if (!idle) studio_render(native, 1024);
    int vol = g_volume;
    int hp = (g_headphones || g_hp_override == 1) ? 1 : 0;
    int gain_num = vol * (hp ? g_hp_att : 100);  /* 0..1000 */
    studio_unlock();
    uint32_t used = (uint32_t)esp_timer_get_time() - t0;
    int pct = (int)((used * 100u) / 18460u);
    if (pct > 999) pct = 999;
    g_cpu_pct = pct;
    static int last_gain = -1;
    if (gain_num != last_gain) {
      last_gain = gain_num;
      ESP_LOGI(TAG, "audio vol=%d/10 gain=%d/1000 hp=%d override=%d att=%d%%",
               vol, gain_num, (int)g_headphones, (int)g_hp_override,
               (int)g_hp_att);
    }
    if (used > 18460u) g_underruns = g_underruns + 1;
    if (idle) memset(native, 0, sizeof(native));
    src[0] = last_l;
    src[1] = last_r;
    memcpy(src + 2, native, sizeof(native));

    int outn = 0;
    while ((phase >> 16) < 1024 && outn < 900) {
      int i = (int)(phase >> 16);
      int32_t f = (int32_t)(phase & 0xFFFF);
      int32_t l = src[2 * i] +
                  (((int32_t)(src[2 * (i + 1)] - src[2 * i]) * f) >> 16);
      int32_t r = src[2 * i + 1] +
                  (((int32_t)(src[2 * (i + 1) + 1] - src[2 * i + 1]) * f) >> 16);
      l = (l * gain_num) / 1000;
      r = (r * gain_num) / 1000;
      out[2 * outn] = (int16_t)l;
      out[2 * outn + 1] = (int16_t)r;
      outn++;
      phase += step;
    }
    last_l = src[2 * 1024];
    last_r = src[2 * 1024 + 1];
    phase -= 1024u << 16;

    size_t written = 0;
    if (i2s_channel_write(s_i2s_tx, out, (size_t)outn * 4, &written,
                          portMAX_DELAY) != ESP_OK) {
      g_underruns = g_underruns + 1;
      ESP_LOGW(TAG, "i2s write failed");
    }
  }
}

extern "C" void app_main(void) {
  auto cfg = M5.config();
  cfg.internal_spk = false;   /* custom ES8311/I2S path in phase 1 */
  cfg.internal_mic = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(g_backlight);

  studio_guard_init();
  if (!M5.Imu.begin()) ESP_LOGW(TAG, "IMU begin failed");
  bool ext_ok = init_external_display();
  bool sd_ok = mount_sd();
  (void)sd_ok;

  init_audio();
  keyboard_init();

  studio_lock();
  studio_init();
  int load_vol = g_volume;
  int load_bl = g_backlight;
  int loaded = persist_load(&load_vol, &load_bl);
  if (loaded <= 0) {
    studio_song_fracture();
    ESP_LOGI(TAG, "auto-load: %s", loaded == 0 ? "no save, fracture" : "save invalid, fracture");
  } else {
    g_volume = load_vol;
    g_backlight = load_bl;
    M5.Display.setBrightness((uint8_t)g_backlight);
    ESP_LOGI(TAG, "auto-load: applied /sdcard/opna2.p2s");
  }
  studio_set_analysis(0);
  studio_play(1);
  studio_unlock();

  ui_init(&s_ext, &M5.Display, s_card != nullptr);
  ui_tick();

  play_test_tone(440, 300);

  if (s_audio_ok) {
    xTaskCreatePinnedToCore(studio_audio_task, "opna_audio", 8192, nullptr, 5,
                            nullptr, 1);
  } else {
    ESP_LOGE(TAG, "audio not ready; OPNA engine not started");
  }

  ESP_LOGI(TAG, "phase 3 ready: internal=240x135 ext=%s sd=%s",
           ext_ok ? "320x240" : "FAIL", s_card ? "OK" : "N/A");

  uint32_t last_amp = 0;
  while (true) {
    keyboard_poll();
    ui_tick();
    if (xTaskGetTickCount() - last_amp > pdMS_TO_TICKS(200)) {
      last_amp = xTaskGetTickCount();
      update_amp();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
