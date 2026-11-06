set(COMPONENT_ADD_INCLUDEDIRS
    src
    )
file(GLOB SRCS
     src/*.cpp
     src/utility/*.cpp
     src/utility/imu/*.cpp
     src/utility/led/*.cpp
     src/utility/power/*.cpp
     src/utility/rtc/*.cpp
     )
set(COMPONENT_SRCS ${SRCS})

# ESP-IDF uses the directory name as the component name (case sensitive).
# Use "M5GFX" when a sibling components/M5GFX checkout exists, otherwise "m5gfx"
# (managed_components/m5stack__m5gfx is registered as the lower-case "m5gfx").
get_filename_component(_m5u_parent "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
if(WIN32)
    # Windows paths are case-insensitive, so the M5GFX test below also matches
    # a components/m5gfx checkout. ESP-IDF registers the real directory name.
    set(_m5gfx_name m5gfx)
elseif(EXISTS "${_m5u_parent}/M5GFX/CMakeLists.txt")
    set(_m5gfx_name M5GFX)
else()
    set(_m5gfx_name m5gfx)
endif()

if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
    set(COMPONENT_REQUIRES ${_m5gfx_name} esp_adc driver)
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
        # IDF 5.3+ split the driver component; list the HAL/LL drivers M5Unified
        # includes explicitly so the component validation accepts them.
        list(APPEND COMPONENT_REQUIRES
            esp_driver_gpio
            esp_driver_i2c
            esp_driver_i2s
            esp_driver_rmt
            esp_driver_touch_sens)
    endif()
else()
    set(COMPONENT_REQUIRES ${_m5gfx_name} esp_adc_cal)
endif()

### If you use arduino-esp32 components, please activate next comment line.
# list(APPEND COMPONENT_REQUIRES arduino-esp32)


message(STATUS "M5Unified use component = ${COMPONENT_REQUIRES}" )


register_component()

