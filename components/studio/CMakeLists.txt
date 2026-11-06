idf_component_register(
  SRCS "engine.c" "fft.c" "patches.c"
  INCLUDE_DIRS "."
  REQUIRES opna_core
)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
