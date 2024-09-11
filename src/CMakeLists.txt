# This warning is emitted by Clang for no good reason, given that it doesn't
# impact compilation in any way, and produces a lot of log spam.
add_compile_options(-Wno-undef-prefix)

add_definitions(-DTARGET_OS_MACOSX=1)

add_subdirectory(Kernel)
add_subdirectory(Libraries)
