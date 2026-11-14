idf_component_register(
  SRCS
    "src/opna/opna_timer.c"
    "src/driver/pmd_load.c"
    "src/driver/player.c"
    "src/driver/vendor/fmdriver/fmdriver_common.c"
    "src/driver/vendor/fmdriver/fmdriver_pmd.c"
    "src/driver/vendor/fmdriver/ppz8.c"
  INCLUDE_DIRS
    "include"
    "src/driver/vendor"
    "src/driver/vendor/fmdriver"
  REQUIRES opna_core
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
