{ stdenv
, lib
, openssl
, zlib
, libxml2
}:

let
  cctools = ../../tools/cctools;
  xar = ../../tools/xar;

  otoolSrcs = [
    "main.c" "ofile_print.c" "m68k_disasm.c" "m88k_disasm.c" "i386_disasm.c"
    "i860_disasm.c" "ppc_disasm.c" "hppa_disasm.c" "sparc_disasm.c"
    "arm_disasm.c" "arm64_disasm.c"
    # print_objc*.c need Apple's real objc/runtime.h and are only ever
    # called under #if USE_LIBOBJC (undefined here) - skip them entirely.
    "print_bitcode.c" "coff_print.c" "dyld_bind_info.c" "host_stubs.c"
  ];

  libstuffSrcs = [
    "allocate.c" "apple_version.c" "arch_usage.c" "arch.c" "args.c"
    "best_arch.c" "breakout.c" "bytesex.c" "checkout.c" "coff_bytesex.c"
    "crc32.c" "dylib_table.c" "dylib_roots.c" "errors.c" "execute.c"
    "fatal_arch.c" "fatals.c" "get_arch_from_host.c" "get_toc_byte_sex.c"
    "guess_short_name.c" "hash_string.c" "hppa.c" "llvm.c" "lto.c"
    "macosx_deployment_target.c" "ofile_error.c" "ofile_get_word.c" "ofile.c"
    "port.c" "print.c" "reloc.c" "rnd.c" "seg_addr_table.c"
    "set_arch_flag_name.c" "swap_headers.c" "symbol_list.c" "SymLoc.c"
    "unix_standard_mode.c" "version_number.c" "write64.c"
    "writeout.c"
  ];

  libxarSrcs = [
    "archive.c" "arcmod.c" "b64.c" "bzxar.c" "darwinattr.c" "data.c" "ea.c"
    "err.c" "ext2.c" "fbsdattr.c" "filetree.c" "hash.c" "io.c" "linuxattr.c"
    "lzmaxar.c" "macho.c" "script.c" "signature.c" "stat.c" "subdoc.c"
    "util.c" "zxar.c"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-host-otool";
  version = "0.1";

  dontUnpack = true;

  nativeBuildInputs = [ ];
  buildInputs = [ openssl zlib libxml2 ];

  buildPhase = ''
    runHook preBuild

    mkdir -p obj

    CFLAGS="-std=gnu99 -I${cctools}/include -I${cctools}/include/foreign -I${xar}/include -DPROGRAM_PREFIX=\"\" -D__LITTLE_ENDIAN__=1 -D__private_extern__=__attribute__((visibility(\"hidden\"))) -include mach/i386/_structs.h -w"

    for f in ${lib.concatStringsSep " " otoolSrcs}; do
      $CC $CFLAGS -c "${cctools}/otool/$f" -o "obj/otool_$f.o"
    done
    for f in ${lib.concatStringsSep " " libstuffSrcs}; do
      $CC $CFLAGS -c "${cctools}/libstuff/$f" -o "obj/libstuff_$f.o"
    done
    for f in ${lib.concatStringsSep " " libxarSrcs}; do
      $CC $CFLAGS -I${openssl.dev}/include -I${libxml2.dev}/include/libxml2 -c "${xar}/lib/$f" -o "obj/libxar_$f.o"
    done

    $CC obj/otool_*.o obj/libstuff_*.o obj/libxar_*.o -o host_otool \
      -Wl,--allow-multiple-definition \
      -L${openssl.out}/lib -L${zlib.out}/lib -L${libxml2.out}/lib \
      -lcrypto -lssl -lz -lxml2 -lm -lpthread

    # install_name_tool: our own darwin cross toolchain's install_name_tool
    # is just a wrapper around llvm-install-name-tool, which has a real
    # parsing bug on some of our own dylibs' load commands ("unsupported
    # load command"). Build the genuine cctools implementation instead,
    # reusing the libstuff objects already compiled above.
    $CC $CFLAGS -c "${cctools}/misc/install_name_tool.c" -o obj/int_install_name_tool.c.o
    $CC $CFLAGS -c "${cctools}/libmacho/arch.c" -o obj/libmacho_arch.c.o
    $CC obj/int_install_name_tool.c.o obj/libmacho_arch.c.o obj/libstuff_*.o \
      -o host_install_name_tool \
      -Wl,--allow-multiple-definition \
      -lm -lpthread

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    install -m755 host_otool $out/bin/otool
    install -m755 host_install_name_tool $out/bin/install_name_tool
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin otool and install_name_tool, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
