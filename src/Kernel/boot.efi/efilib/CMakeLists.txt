add_darwin_static_library(libefi)
set_property(TARGET libefi PROPERTY OUTPUT_NAME "efi")
target_include_directories(libefi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# __stack_chk_fail and __stack_chk_guard do not exist in EFI land.
# EFI also assumes short wchar (it is int by default).
# These need to be PUBLIC so that targets that link to us do this as well.
target_compile_options(libefi PUBLIC -ffreestanding -fno-stack-protector)
target_compile_options(libefi PUBLIC -fshort-wchar)

# For stdint.h.
target_link_libraries(libefi PUBLIC xnu_kernel_headers)

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
