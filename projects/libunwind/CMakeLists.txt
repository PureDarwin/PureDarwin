add_darwin_shared_library(libunwind NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)

target_sources(libunwind PRIVATE
    src/libunwind.cpp
    src/Unwind_AppleExtras.cpp
    src/Unwind-EHABI.cpp
    src/Unwind-seh.cpp
    src/Unwind-sjlj.c
    src/UnwindLevel1-gcc-ext.c
    src/UnwindLevel1.c
    src/UnwindRegistersRestore.S
    src/UnwindRegistersSave.S
)
target_include_directories(libunwind PUBLIC include)
target_compile_definitions(libunwind PRIVATE _LIBUNWIND_IS_NATIVE_ONLY)
target_link_libraries(libunwind PRIVATE Libc_headers dyld_headers)
target_link_options(libunwind PRIVATE -umbrella System)
set_property(TARGET libunwind PROPERTY CXX_STANDARD 11)
