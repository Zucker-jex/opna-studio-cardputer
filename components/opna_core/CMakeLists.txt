idf_component_register(
  SRCS
    "src/opna.c"
    "src/opna_fm.c"
    "src/opna_ssg.c"
    "src/opna_drum.c"
  INCLUDE_DIRS "include"
  REQUIRES esp_common
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
