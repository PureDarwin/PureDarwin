add_library(xnu.ptrauth INTERFACE)
target_compile_options(xnu.ptrauth INTERFACE -include ${CMAKE_CURRENT_SOURCE_DIR}/base.h)
