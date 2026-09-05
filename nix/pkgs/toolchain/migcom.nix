{ stdenv
, lib
, bison
, flex
, appleSdk
}:

let
  onDarwin = stdenv.hostPlatform.isDarwin;

  migcomSrcs = [
    "error.c" "global.c" "header.c" "mig.c" "routine.c" "server.c"
    "statement.c" "string.c" "type.c" "user.c" "utils.c"
  ];

  # On a Darwin host the compiler's own SDK already provides the real mach/,
  # i386/ and machine/ headers mig's sources expect, so none of the staging or
  # glibc-vs-Apple typedef reconciliation below is needed - just compile.
  sdkShimPhase = lib.optionalString (!onDarwin) ''
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Stage just the SDK's mach/ header subtree into an isolated shim dir
    # rather than adding the whole SDK usr/include to the search path -
    # see tools/mig/CMakeLists.txt for the full explanation.
    mkdir -p mach_shim
    cp -r "$SDK/usr/include/mach" mach_shim/mach
    # Both arch subtrees, because the SDK's machine/*.h dispatch on the host
    # arch and an aarch64 builder lands in arm/ where an x86 one lands in
    # i386/. They need the same glibc-vs-Apple typedef reconciliation.
    cp -r "$SDK/usr/include/i386" mach_shim/i386
    cp -r "$SDK/usr/include/arm" mach_shim/arm
    # The SDK is immutable in the Nix store; sed -i needs a writable
    # directory for its temporary replacement file.
    chmod -R u+w mach_shim
    sed -i \
      -e "/typedef long long *__int64_t;/d" \
      -e "/typedef unsigned long long *__uint64_t;/d" \
      -e "/^typedef union {/,/} __mbstate_t;/d" \
      mach_shim/i386/_types.h mach_shim/arm/_types.h
    mkdir -p mach_shim/sys
    cp "$SDK/usr/include/sys/_types.h" \
       "$SDK/usr/include/sys/_endian.h" \
       "$SDK/usr/include/sys/appleapiopts.h" \
       mach_shim/sys/
    cp -r "$SDK/usr/include/sys/_types" mach_shim/sys/_types
    # xnu 12377 also spells the fixed-width types out under sys/_types/, where
    # they collide with glibc's stdint. glibc already provides all of them.
    # Re-chmod: this tree was copied after the chmod above, so it is still
    # store-read-only and sed cannot write its temporary file.
    chmod -R u+w mach_shim/sys
    sed -i -e "/^typedef .*[[:space:]]u\?_\?int64_t;/d" \
      mach_shim/sys/_types/_int64_t.h mach_shim/sys/_types/_u_int64_t.h
    cp -r "$SDK/usr/include/sys/_pthread" mach_shim/sys/_pthread
    cp -r "$SDK/usr/include/machine" mach_shim/machine
    cp -r "$SDK/usr/include/architecture" mach_shim/architecture
    cp -r "$SDK/usr/include/_types" mach_shim/_types
    cp -r "$SDK/usr/include/libkern" mach_shim/libkern
    cp "$SDK/usr/include/Availability.h" \
       "$SDK/usr/include/AvailabilityInternal.h" \
       "$SDK/usr/include/AvailabilityInternalLegacy.h" \
       "$SDK/usr/include/AvailabilityVersions.h" \
       "$SDK/usr/include/AvailabilityMacros.h" \
       mach_shim/
    # Apple's limits.h sets MB_LEN_MAX to 6 (4 in machlimits.h); glibc's
    # bits/stdlib.h #errors unless it is glibc's own 16. glibc supplies it.
    chmod -R u+w mach_shim
    chmod -R u+w mach_shim/i386 mach_shim/arm mach_shim/machine
    sed -i "/^#define[[:space:]]*MB_LEN_MAX/d" \
      mach_shim/i386/limits.h mach_shim/arm/limits.h \
      mach_shim/i386/machlimits.h mach_shim/arm/machlimits.h \
      mach_shim/machine/i386/limits.h mach_shim/machine/arm/limits.h 2>/dev/null || true
    # xnu 12377 annotates headers with kalloc type-segregation attributes and
    # C23 enum helpers; compiled against glibc, xnu's cdefs.h never defines
    # them away, so supply portable equivalents.
    cat > mach_shim/pd_xnu_macros.h <<'XNUEOF'
#pragma once
#define __kernel_ptr_semantics
#define __kernel_data_semantics
#define __kernel_dual_semantics
#define __enum_decl(_name, _type, ...)           typedef _type _name; enum __VA_ARGS__
#define __enum_closed_decl(_name, _type, ...)    typedef _type _name; enum __VA_ARGS__
#define __options_decl(_name, _type, ...)        typedef _type _name; enum __VA_ARGS__
#define __options_closed_decl(_name, _type, ...) typedef _type _name; enum __VA_ARGS__
XNUEOF
  '';

  archDefines = lib.optionalString stdenv.hostPlatform.isAarch64 " -D__arm64__=1";

  hostCflags =
    if onDarwin
    then ''-I . -include sys/types.h''
    else ''-I . -I mach_shim -include sys/types.h -include bits/types/__mbstate_t.h${archDefines} -include pd_xnu_macros.h'';
in
stdenv.mkDerivation {
  pname = "puredarwin-migcom-native";
  version = "0.1";

  src = ../../../tools/mig;

  nativeBuildInputs = [ bison flex ];

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    yacc -d -b y parser.y
    lex -o lexxer.yy.c lexxer.l

    ${sdkShimPhase}

    mkdir -p migcom_native
    objs=""
    for src in ${lib.concatStringsSep " " migcomSrcs} y.tab.c lexxer.yy.c; do
      name="$(basename "$src" .c)"
      obj="migcom_native/$name.o"
      $CC -std=gnu89 -D__private_extern__= -D__LITTLE_ENDIAN__ -DMIG_VERSION=\"\" \
        ${hostCflags} \
        -c "$src" -o "$obj"
      objs="$objs $obj"
    done
    $CC -o migcom_native/migcom.native $objs
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp migcom_native/migcom.native $out/bin/migcom
    runHook postInstall
  '';

  meta = with lib; {
    description = "Native build of PureDarwin's tools/mig migcom, for use as a build-time host tool";
    platforms = platforms.unix;
  };
}
