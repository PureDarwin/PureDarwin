# cctools. This includes a bunch of misc. tools for dealing with binaries, etc.
#   there is one static libraries and most tools are just a single source on
#   top of that.

# The following tools can be built from these sources (those marked with * are in usr/local/bin normally)
# ar
# as{,-arm,-i386,-x86_64}
# bitcode_strip
# check_dylib* (REMOVED)
# checksyms*
# cmpdylib
# codesign_allocate
# ctf_insert
# depinfo*
# diagtest*
# indr (REMOVED)
# install_name_tool
# libtool
# lipo
# llvm-otool
# mtoc*
# mtor*
# nm-classic
# nmedit
# otool-classic
# pagestuff
# redo_prebinding (REMOVED)
# seg_addr_table* (REMOVED)
# seg_hack*
# segedit
# size-classic
# strings
# strip
# vtool

# BEGIN STATIC LIBRARY libstuff

add_library(libstuff STATIC)

target_sources(libstuff PRIVATE
    src/libstuff/swap_headers.c
    src/libstuff/rnd.c
    src/libstuff/hash_string.c
    src/libstuff/arch_usage.c
    src/libstuff/hppa.c
    src/libstuff/symbol_list.c
    src/libstuff/checkout.c
    src/libstuff/arch.c
    src/libstuff/crc32.c
    src/libstuff/set_arch_flag_name.c
    src/libstuff/fatals.c
    src/libstuff/align.c
    src/libstuff/breakout.c
    src/libstuff/guess_short_name.c
    src/libstuff/print.c
    src/libstuff/xcode.c
    src/libstuff/errors.c
    src/libstuff/fatal_arch.c
    src/libstuff/vm_flush_cache.c
    src/libstuff/ofile.c
    src/libstuff/allocate.c
    src/libstuff/get_toc_byte_sex.c
    src/libstuff/apple_version.c
    src/libstuff/lto.c
    src/libstuff/coff_bytesex.c
    src/libstuff/best_arch.c
    src/libstuff/unix_standard_mode.c
    src/libstuff/dylib_roots.c
    src/libstuff/args.c
    src/libstuff/llvm.c
    src/libstuff/diagnostics.c
    src/libstuff/macosx_deployment_target.c
    src/libstuff/writeout.c
    src/libstuff/bytesex.c
    src/libstuff/version_number.c
    src/libstuff/SymLoc.c
    src/libstuff/ofile_get_word.c
    src/libstuff/execute.c
    src/libstuff/get_arch_from_host.c
    src/libstuff/depinfo.c
    src/libstuff/ofile_error.c
    src/libstuff/write64.c
    src/libstuff/reloc.c
)

# cmake can't recurse this I don't believe...
target_include_directories(libstuff PUBLIC
    src/include/architecture/i386
    src/include/architecture/m88k
    src/include/architecture/nrw
    src/include/architecture/sparc
    src/include/cbt
    src/include/coff
    src/include/driverkit
    src/include/elf
    src/include/gnu
    src/include/i386
    src/include/llvm-c
    src/include/mach
    src/include/mach/arm
    src/include/mach/hppa
    src/include/mach/i386
    src/include/mach/i860
    src/include/mach/m68k
    src/include/mach/m88k
    src/include/mach/ppc
    src/include/mach/sparc
    src/include/mach-o
    src/include/mach-o/arm
    src/include/mach-o/arm64
    src/include/mach-o/hppa
    src/include/mach-o/i386
    src/include/mach-o/i860
    src/include/mach-o/m68k
    src/include/mach-o/m88k
    src/include/mach-o/ppc
    src/include/mach-o/sparc
    src/include/mach-o/x86_64
    src/include/modules
    src/include/opcode
    src/include/standalone
    src/include/stuff
    src/include/sys
    src/include/xar
    src/include
)

# libstuff needs to find the host toolchain llvm-c/lto.h header, which we install here
target_include_directories(libstuff PRIVATE ${RC_HOST_BIN}/../include)

# TODO: Better way to track cctools version
target_compile_definitions(libstuff PRIVATE CURRENT_PROJECT_VERSION=\"973.0.1\")
target_compile_definitions(libstuff PUBLIC LTO_SUPPORT)
target_compile_options(libstuff PUBLIC -Wno-ambiguous-macro -flto)
target_link_options(libstuff PUBLIC -flto)

# END STATIC LIBRARY libstuff

# Macros to build simple tools from a single source file

macro(install_link_tool named target)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${target} ${CMAKE_INSTALL_PREFIX}/${named})")
    install(CODE "message(\"-- Create tool alias: ${CMAKE_INSTALL_PREFIX}/${named} --> ${target}\")")
endmacro()

# Create executable target with no sources
macro(add_tool_skel named)
    add_executable(${named})

    target_link_libraries(${named} PRIVATE libstuff)
    target_compile_definitions(${named} PRIVATE LTO_SUPPORT)

    install(TARGETS ${named} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)
endmacro()

# Create executable tool with one source file in ${source_dir}
macro(add_tool named source_dir)
    add_tool_skel(${named})

    target_sources(${named} PRIVATE src/${source_dir}/${named}.c)
endmacro()

macro(add_misc_tool named)
    add_tool(${named} misc)
endmacro()

## Various tools

add_tool(mtoc efitools)
add_tool(mtor efitools)

add_misc_tool(bitcode_strip)
add_misc_tool(checksyms)
add_misc_tool(cmpdylib)
add_misc_tool(ctf_insert)
add_misc_tool(depinfo)
add_misc_tool(diagtest)
add_misc_tool(install_name_tool)
add_misc_tool(libtool)
add_misc_tool(lipo)
add_misc_tool(pagestuff)
add_misc_tool(seg_hack)
add_misc_tool(segedit)
add_misc_tool(strings)
add_misc_tool(vtool)

## tools which need a bit of extra effort...

add_misc_tool(strip)

# strip needs to link against libprunetrie from ld64
target_link_libraries(strip PRIVATE libstuff libprunetrie)
target_compile_definitions(strip PRIVATE TRIE_SUPPORT)

add_misc_tool(codesign_allocate)

# codesign_allocate has this plist embedded in the executable image
target_link_options(codesign_allocate PRIVATE -sectcreate __TEXT __info_plist ${CMAKE_CURRENT_SOURCE_DIR}/src/misc/codesign_allocate-Info.plist)

add_tool_skel(ar)

target_sources(ar PRIVATE
    src/ar/print.c
    src/ar/misc.c
    src/ar/ar.c
    src/ar/extract.c
    src/ar/contents.c
    src/ar/archive.c
    src/ar/append.c
    src/ar/move.c
    src/ar/replace.c
    src/ar/delete.c
)

add_tool_skel(as)
target_sources(as PRIVATE src/as/driver.c)

add_tool_skel(llvm-otool)
target_compile_definitions(llvm-otool PRIVATE LLVM_OTOOL EFI_SUPPORT)
target_sources(llvm-otool PRIVATE src/otool/main.c)

add_tool_skel(nm-classic)
target_sources(nm-classic PRIVATE src/misc/nm.c)

# nmedit is built from the same source as strip, but with a macro defined.
add_tool_skel(nmedit)
target_sources(nmedit PRIVATE src/misc/strip.c)
target_compile_definitions(nmedit PRIVATE NMEDIT)

add_tool_skel(otool-classic)
target_compile_definitions(otool-classic PRIVATE OTOOL EFI_SUPPORT)
target_include_directories(otool-classic PRIVATE src/otool)
target_link_options(otool-classic PRIVATE -lc++) # For __cxa_demangle

target_sources(otool-classic PRIVATE
    src/otool/print_objc2_util.c
    src/otool/main.c
    src/otool/i386_disasm.c
    src/otool/print_objc2_32bit.c
    src/otool/arm_disasm.c
    src/otool/i860_disasm.c
    src/otool/ofile_print.c
    src/otool/print_objc2_64bit.c
    src/otool/m88k_disasm.c
    src/otool/ppc_disasm.c
    src/otool/print_bitcode.c
    src/otool/dyld_bind_info.c
    src/otool/sparc_disasm.c
    src/otool/m68k_disasm.c
    src/otool/arm64_disasm.c
    src/otool/hppa_disasm.c
    src/otool/coff_print.c
    src/otool/print_objc.c

    src/libstuff/ofile.c
)

add_tool_skel(size-classic)
target_sources(size-classic PRIVATE src/misc/size.c)

macro(add_assembler arch)
    add_executable(as-${arch})

    target_link_libraries(as-${arch} PRIVATE libstuff)
    target_compile_definitions(as-${arch} PRIVATE LTO_SUPPORT NeXT_MOD)
    target_include_directories(as-${arch} PRIVATE src/as)

    target_sources(as-${arch} PRIVATE
        src/as/hash.c
        src/as/messages.c
        src/as/xmalloc.c
        src/as/app.c
        src/as/flonum-copy.c
        src/as/layout.c
        src/as/as.c
        src/as/frags.c
        src/as/atof-ieee.c
        src/as/flonum-mult.c
        src/as/symbols.c
        src/as/dwarf2dbg.c
        src/as/expr.c
        src/as/input-scrub.c
        src/as/atof-generic.c
        src/as/fixes.c
        src/as/read.c
        src/as/write_object.c
        src/as/flonum-const.c
        src/as/obstack.c
        src/as/sections.c
        src/as/hex-value.c
        src/as/input-file.c
    )

    # These are installed into a very specific location... (note: they should just be named as, but this is hard to achieve with cmake)
    install(TARGETS as-${arch} RENAME as DESTINATION ${CMAKE_INSTALL_PREFIX}/../libexec/as/${arch} COMPONENT HostTools)
endmacro()

add_assembler(arm)
target_compile_definitions(as-arm PRIVATE ARM)
target_sources(as-arm PRIVATE src/as/arm.c)

add_assembler(i386)
target_compile_definitions(as-i386 PRIVATE I386 i486 i586 i686)
target_sources(as-i386 PRIVATE src/as/i386.c)

add_assembler(x86_64)
target_compile_definitions(as-x86_64 PRIVATE I386 i486 i586 i686 ARCH64)
target_sources(as-x86_64 PRIVATE src/as/i386.c)

## symlinks

# These tools are just linked to other names
install_link_tool(nm llvm-nm)
install_link_tool(otool llvm-otool)
install_link_tool(size size-classic)
install_link_tool(ranlib libtool)
install_link_tool(objdump llvm-objdump)
install_link_tool(codesign_allocate-p codesign_allocate)

# Help as find these
install_link_tool(../libexec/as/arm/as as-arm)
install_link_tool(../libexec/as/i386/as as-i386)
install_link_tool(../libexec/as/x86_64/as as-x86_64)
