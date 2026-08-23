function(add_kext_bundle name)
    cmake_parse_arguments(SL "KERNEL_PRIVATE" "MACOSX_VERSION_MIN;INFO_PLIST;BUNDLE_IDENTIFIER;BUNDLE_VERSION;MAIN_FUNCTION;ANTIMAIN_FUNCTION" "" ${ARGN})

    if(SL_MACOSX_VERSION_MIN)
        add_darwin_shared_library(${name} MODULE MACOSX_VERSION_MIN ${SL_MACOSX_VERSION_MIN})
    else()
        add_darwin_shared_library(${name} MODULE)
    endif()

    set_property(TARGET ${name} PROPERTY PREFIX "")
    set_property(TARGET ${name} PROPERTY SUFFIX "")

    target_compile_definitions(${name} PRIVATE TARGET_OS_OSX KERNEL)
    target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fapple-kext>)

    # Kernel code runs in an environment where the System V red zone is unsafe:
    # interrupts (and the kernel's own exception entry) push onto the current
    # stack without honoring the 128-byte red zone, so any leaf function that
    # keeps live data below %rsp will be corrupted the moment an IRQ lands in
    # it (observed as random frame/register smashes, e.g. HFS buildthreadkey).
    # The XNU kernel proper is built with -mno-red-zone; kexts must match.
    # The red zone is an x86-64 SysV ABI feature; -mno-red-zone is x86-only
    # (clang rejects it on arm64, which has no red zone to disable).
    if(NOT PUREDARWIN_ARM64 AND NOT PUREDARWIN_ARM32)
        target_compile_options(${name} PRIVATE -mno-red-zone)
    endif()

    # There is no __stack_chk_guard in the kernel, and on ARMv6 the reference
    # to it becomes a text relocation the bundle linker refuses outright. The
    # kernel proper is built without the stack protector; kexts must match.
    target_compile_options(${name} PRIVATE -fno-stack-protector)

    # Tentative definitions must be allocated in the kext, not left as common
    # symbols: a common symbol is N_UNDF with a size in n_value, so the kext
    # linker cannot tell it apart from a genuine import and binds it to the
    # panic trampoline instead of giving it storage.
    target_compile_options(${name} PRIVATE -fno-common)

    # ARMv6 only performs unaligned LDR/STR when SCTLR.U is set, and this port
    # leaves it clear, so an unaligned load silently returns the aligned word
    # rotated. Packed on-disk structures must be accessed byte-wise instead.
    if(PUREDARWIN_ARCH STREQUAL "armv6")
        target_compile_options(${name} PRIVATE -mno-unaligned-access)
    endif()


    if(CMAKE_HOST_APPLE)
        # Real Apple ld rejects a plain MH_BUNDLE (-bundle) unless it links
        # libSystem, which a kext must not do. Kexts need the dedicated
        # MH_KEXT_BUNDLE output (-kext), which has no such requirement.
        target_link_options(${name} PRIVATE "LINKER:-kext")
    else()
        target_link_options(${name} PRIVATE "LINKER:-bundle")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            # ld64 requires libSystem to accept -bundle; the SDK .tbd stub satisfies
            # the check without adding a real runtime dependency.
            target_link_options(${name} PRIVATE -lSystem)
        endif()
    endif()
    target_link_options(${name} PRIVATE "SHELL:-undefined dynamic_lookup")

    # ARMv6 has no movw/movt, so the compiler reaches other symbols through
    # literal pools, which ld64 sees as relocations in a read-only section and
    # refuses in a bundle. The kernel proper links with the same suppression
    # (LDFLAGS_KERNEL_GENARM in MakeInc.def.in); kexts are loaded the same way.
    if(PUREDARWIN_ARM32)
        target_link_options(${name} PRIVATE "SHELL:-Wl,-read_only_relocs,suppress")

        # ARMv6 has no integer divide instruction, so the compiler emits calls
        # to __udivsi3 and friends. The kernel keeps its copy of those private,
        # so give each kext its own rather than binding to nothing.
        if(PUREDARWIN_KERNEL_COMPILER_RT)
            target_link_options(${name} PRIVATE "${PUREDARWIN_KERNEL_COMPILER_RT}")
        endif()
    endif()

    if(SL_KERNEL_PRIVATE)
        target_compile_definitions(${name} PRIVATE KERNEL_PRIVATE)
        target_link_libraries(${name} PRIVATE xnu_kernel_private_headers)
    endif()

    target_link_libraries(${name} PRIVATE xnu_kernel_headers AvailabilityHeaders)

    set_property(TARGET ${name} PROPERTY BUNDLE TRUE)
    set_property(TARGET ${name} PROPERTY BUNDLE_EXTENSION kext)

    if(SL_INFO_PLIST)
        get_filename_component(SL_INFO_PLIST ${SL_INFO_PLIST} ABSOLUTE)
        set_property(TARGET ${name} PROPERTY MACOSX_BUNDLE_INFO_PLIST ${SL_INFO_PLIST})
    else()
        message(SEND_ERROR "INFO_PLIST argument must be provided to add_darwin_kext()")
    endif()

    if(SL_BUNDLE_IDENTIFIER)
        set_property(TARGET ${name} PROPERTY MACOSX_BUNDLE_GUI_IDENTIFIER ${SL_BUNDLE_IDENTIFIER})
    endif()
    if(SL_BUNDLE_VERSION)
        set_property(TARGET ${name} PROPERTY MACOSX_BUNDLE_BUNDLE_VERSION ${SL_BUNDLE_VERSION})
    endif()

    add_kmod_info(${name} MAIN_FUNCTION ${SL_MAIN_FUNCTION} ANTIMAIN_FUNCTION ${SL_ANTIMAIN_FUNCTION})
endfunction()

function(add_kmod_info target)
    cmake_parse_arguments(KEXT "" "IDENTIFIER;VERSION;MAIN_FUNCTION;ANTIMAIN_FUNCTION" "" ${ARGN})

    if(NOT KEXT_IDENTIFIER)
        get_property(KEXT_IDENTIFIER TARGET ${target} PROPERTY MACOSX_BUNDLE_GUI_IDENTIFIER)
    endif()
    if(NOT KEXT_IDENTIFIER)
        message(SEND_ERROR "MACOSX_BUNDLE_GUI_IDENTIFIER is not set on kext target ${target}")
        return()
    endif()

    if(NOT KEXT_VERSION)
        get_property(KEXT_VERSION TARGET ${target} PROPERTY MACOSX_BUNDLE_BUNDLE_VERSION)
    endif()
    if(NOT KEXT_VERSION)
        message(SEND_ERROR "MACOSX_BUNDLE_BUNDLE_VERSION is not set on kext target ${target}")
        return()
    endif()

    if(KEXT_MAIN_FUNCTION)
        set(KEXT_MAIN_FUNCTION_DECL "extern kern_return_t ${KEXT_MAIN_FUNCTION}(kmod_info_t *ki, void *data);")
    else()
        set(KEXT_MAIN_FUNCTION "0")
    endif()

    if(KEXT_ANTIMAIN_FUNCTION)
        set(KEXT_ANTIMAIN_FUNCTION_DECL "extern kern_return_t ${KEXT_ANTIMAIN_FUNCTION}(kmod_info_t *ki, void *data);")
    else()
        set(KEXT_ANTIMAIN_FUNCTION "0")
    endif()

    configure_file(${PUREDARWIN_SOURCE_DIR}/cmake/templates/kmod_info.c.in ${CMAKE_CURRENT_BINARY_DIR}/kmod_info.c)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/kmod_info.c)
    target_link_libraries(${target} PRIVATE libkmod)
endfunction()
