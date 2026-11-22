{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, libcxxDylib
, libzDylib
, bison
, flex
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# dtrace(1) and libdtrace from Apple's dtrace-413, CDDL with some APSL files, kept as their own component.
# Built without CoreSymbolication, as Apple's dtrace_host configuration is. libdwarf and ctfconvert are not built.
stdenv.mkDerivation {
  pname = "puredarwin-dtrace";
  version = "413";

  src = ../../../src/Userspace/dtrace;

  nativeBuildInputs = [ bison flex ];

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    PDLS="${../../../src/Libraries/libSystem}"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"

    mkdir -p gen obj compat/System/sys
    cp ${../../../src/Kernel/xnu/bsd/sys/kas_info.h} compat/System/sys/kas_info.h
    cp ${../../../src/Kernel/xnu/bsd/sys/proc_info.h} compat/System/sys/proc_info.h

    bison -d -y -t -o gen/dt_grammar.c lib/libdtrace/common/dt_grammar.y
    cp gen/dt_grammar.h gen/y.tab.h
    flex -l -o gen/dt_lex.c lib/libdtrace/common/dt_lex.l

    INCS="-I$PDLS/libsystem_kernel/private_include -I$PDLS/libc -I$PDLS/pthread/private -I$PDLS/libplatform/private \
      -Ilib/libdtrace/common -Ilib/libdtrace/apple -Ilib/libdtrace/i386 -Ilib/libctf/common -Ilib/libelf -Ilib/libproc \
      -Icompat/opensolaris -Icompat/opensolaris/sys -Iinclude -Igen -Icompat \
      -I${libSystem}/usr/include -I${corefoundation}/include"
    CFLAGS="-include pd/pd_dtrace_prefix.h -isysroot $SDK -F$SDK/System/Library/Frameworks -O2 -std=gnu11 \
      -DNDEBUG -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $INCS -w"

    for f in lib/libelf/*.c lib/libctf/common/*.c lib/libproc/*.c lib/libdtrace/common/*.c \
             lib/libdtrace/apple/*.c lib/libdtrace/i386/*.c gen/dt_grammar.c gen/dt_lex.c \
             compat/opensolaris/darwin_shim.c cmd/dtrace/dtrace.c; do
      $CC $CFLAGS -c $f -o obj/$(basename $f .c).o
    done
    $CXX ''${CFLAGS/-std=gnu11/-std=gnu++14} -c lib/libdtrace/apple/dt_ld.cpp -o obj/dt_ld.o

    LDF="-isysroot $SDK -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${libcxxDylib}/usr/lib -L${libzDylib}/usr/lib \
      -F${iokit}/System/Library/Frameworks \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5"

    $CXX $LDF -dynamiclib -install_name /usr/lib/libdtrace.dylib \
      -Wl,-exported_symbols_list,lib/libdtrace/exports \
      -o libdtrace.dylib $(ls obj/*.o | grep -v /dtrace.o) -lc++ -lz -lCoreFoundation -lSystem
    $CC $LDF -o dtrace obj/dtrace.o -L. -ldtrace -framework IOKit -lCoreFoundation -lSystem

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin $out/usr/lib $out/usr/share/man/man1 $out/usr/share/licenses/dtrace
    mkdir -p $out/usr/lib/dtrace
    cp pd/pd_user_types.d $out/usr/lib/dtrace/
    cp dtrace $out/usr/sbin/dtrace
    cp libdtrace.dylib $out/usr/lib/libdtrace.dylib
    cp cmd/dtrace/dtrace.1 $out/usr/share/man/man1/
    cp APPLE_LICENSE FileOrigins $out/usr/share/licenses/dtrace/
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Apple dtrace-413 userland: dtrace(1) and libdtrace, CDDL, without CoreSymbolication and CTF tools";
    license = [ licenses.cddl licenses.apsl20 ];
    platforms = platforms.unix;
  };
}
