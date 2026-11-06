cmake_minimum_required(VERSION 3.16)

# Local components: M5Unified, m5gfx and the future opna/studio/cardputer_hal.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(opna_studio_cardputer)
