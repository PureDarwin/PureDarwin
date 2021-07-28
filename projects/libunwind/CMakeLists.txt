add_darwin_shared_library(libunwind NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
set_property(TARGET libcppabi PROPERTY PREFIX lib)
set_property(TARGET libcppabi PROPERTY OUTPUT_NAME unwind)

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
target_link_options(libunwind PRIVATE -umbrella System)

add_firstpass_library(libunwind)
#target_link_firstpass_siblings(libunwind UPWARD libcppabi libsystem_compiler_rt)
#target_link_firstpass_siblings(libunwind libc libdyld libsystem_malloc libplatform libpthread)
