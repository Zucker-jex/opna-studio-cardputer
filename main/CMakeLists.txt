idf_component_register(
  SRCS "main.cpp" "ui.cpp" "keyboard.cpp" "persist.cpp" "midi.cpp" "pmd_audio.cpp" "file_manager.cpp" "jp_font.cpp"
  INCLUDE_DIRS "."
  REQUIRES
    M5Unified
    m5gfx
    esp_driver_gpio
    esp_driver_spi
    esp_driver_sdspi
    sdmmc
    fatfs
    studio
    pmd_core
)
