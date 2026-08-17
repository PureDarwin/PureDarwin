set(_puredarwin_prebuilt_libsystem_root "${PUREDARWIN_PREBUILT_LIBSYSTEM_ROOT}")

set(_puredarwin_prebuilt_libsystem_b "${_puredarwin_prebuilt_libsystem_root}/usr/lib/libSystem.B.dylib")
set(_puredarwin_prebuilt_libsystem "${_puredarwin_prebuilt_libsystem_root}/usr/lib/libSystem.dylib")
set(_puredarwin_prebuilt_libdyld "${_puredarwin_prebuilt_libsystem_root}/usr/lib/system/libdyld.dylib")
set(_puredarwin_prebuilt_libsystem_kernel "${_puredarwin_prebuilt_libsystem_root}/usr/lib/system/libsystem_kernel.a")
set(_puredarwin_prebuilt_libsyscall_traps "${_puredarwin_prebuilt_libsystem_root}/usr/lib/system/syscalls.a")
set(_puredarwin_prebuilt_dyld "${_puredarwin_prebuilt_libsystem_root}/usr/lib/dyld")

# libSystem.B.dylib reexports /usr/lib/system/libdyld.dylib. When linking
# against a prebuilt libSystem with an SDK sysroot, ld otherwise resolves
# that install name to the SDK's arm64/x86_64 TBD stub instead of the
# architecture-matched dyld built alongside libSystem.
add_link_options(
    "-Wl,-L,${_puredarwin_prebuilt_libsystem_root}/usr/lib/system"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${_puredarwin_prebuilt_libdyld}")

foreach(_puredarwin_prebuilt_path
        "${_puredarwin_prebuilt_libsystem_b}"
        "${_puredarwin_prebuilt_libsystem}"
        "${_puredarwin_prebuilt_libdyld}"
        "${_puredarwin_prebuilt_dyld}")
    if(NOT EXISTS "${_puredarwin_prebuilt_path}")
        message(FATAL_ERROR "Missing prebuilt libSystem artifact: ${_puredarwin_prebuilt_path}")
    endif()
endforeach()

add_library(libSystem_B_stub SHARED IMPORTED GLOBAL)
set_target_properties(libSystem_B_stub PROPERTIES
    IMPORTED_LOCATION "${_puredarwin_prebuilt_libsystem_b}")

add_library(libdyld SHARED IMPORTED GLOBAL)
set_target_properties(libdyld PROPERTIES
    IMPORTED_LOCATION "${_puredarwin_prebuilt_libdyld}")

if(EXISTS "${_puredarwin_prebuilt_libsystem_kernel}")
    add_library(libsystem_kernel_static STATIC IMPORTED GLOBAL)
    set_target_properties(libsystem_kernel_static PROPERTIES
        IMPORTED_LOCATION "${_puredarwin_prebuilt_libsystem_kernel}")
endif()

if(EXISTS "${_puredarwin_prebuilt_libsyscall_traps}")
    add_library(libsyscall_traps STATIC IMPORTED GLOBAL)
    set_target_properties(libsyscall_traps PROPERTIES
        IMPORTED_LOCATION "${_puredarwin_prebuilt_libsyscall_traps}")
endif()

add_executable(dyld IMPORTED GLOBAL)
set_target_properties(dyld PROPERTIES
    IMPORTED_LOCATION "${_puredarwin_prebuilt_dyld}")
