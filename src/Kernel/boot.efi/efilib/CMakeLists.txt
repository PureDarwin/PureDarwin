add_darwin_static_library(libefi)
target_include_directories(libefi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_compile_definitions(libefi PRIVATE _M_X64)

# For stdint.h.
target_link_libraries(libefi PRIVATE xnu_kernel_headers)

target_sources(libefi PRIVATE
    lib/boxdraw.c
    lib/cmdline.c
    lib/console.c
    lib/crc.c
    lib/data.c
    lib/debug.c
    lib/dpath.c
    lib/error.c
    lib/event.c
    lib/exit.c
    lib/guid.c
    lib/hand.c
    lib/hw.c
    lib/init.c
    lib/lock.c
    lib/misc.c
    lib/pause.c
    lib/print.c
    lib/smbios.c
    lib/sread.c
    lib/str.c

    lib/runtime/efirtlib.c
    lib/runtime/rtdata.c
    lib/runtime/rtlock.c
    lib/runtime/rtstr.c
    lib/runtime/vm.c

    lib/x86_64/callwrap.c
    lib/x86_64/efi_stub.S
    lib/x86_64/initplat.c
    lib/x86_64/math.c
    lib/x86_64/setjmp.S
)
