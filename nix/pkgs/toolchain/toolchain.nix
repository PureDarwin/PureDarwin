{ lib
, stdenv
, writeShellScriptBin
, symlinkJoin
, llvmPackages_21
, cctools
, callPackage
  # otool/install_name_tool. The vendored cross-build is for a Linux host; a
  # Darwin host has the real ones already and cannot compile that copy anyway
  # (its ofile_print.c wants legacy types like i386_float_state_t that the macOS
  # SDK no longer declares).
, hostOtool ? (if stdenv.hostPlatform.isDarwin
               then cctools
               else callPackage ./host-otool.nix { })
, nativeLd ? null
, target ? "x86_64-apple-darwin20.4"
, clangTarget ? "x86_64-apple-macosx11.0"
, defaultSdkRoot ? "/usr/local/osxcross/SDK/MacOSX11.3.sdk"
}:

let
  clang = llvmPackages_21.clang-unwrapped;
  linkerArg = if nativeLd != null
              then "-fuse-ld=${nativeLd}/bin/ld"
              else "-fuse-ld=lld";
  lld = llvmPackages_21.lld;
  bintools = llvmPackages_21.bintools-unwrapped;

  # The ISA baseline for generated code.
  #
  # clang's default CPU for arm64-apple-macosx11.0 is an Apple Silicon Mac
  # (ARMv8.5), so it freely emits the ARMv8.1 large-system-extension atomics -
  # ldadd, cas and friends. The oldest hardware PureDarwin targets is Hurricane
  # (A10), which is ARMv8.0 and traps those as undefined instructions: the
  # symptom is SIGILL in the first process to execute one. Note -march=armv8-a
  # does NOT prevent this on Apple targets; only -mcpu does.
  baselineCpu = if lib.hasPrefix "arm64-" clangTarget then [ "-mcpu=apple-a10" ] else [ ];

  compilerWrapper = name: realBin: writeShellScriptBin "${target}-${name}" ''
    SDK="''${DARWIN_SDK_ROOT:-${defaultSdkRoot}}"
    export PATH="${lld}/bin:$PATH"
    fuseld=(${linkerArg})
    cpu=(${lib.escapeShellArgs baselineCpu})
    prev=
    for a in "$@"; do
      case "$a" in
        -fuse-ld=*) fuseld=() ;;
        -c|-E|-S|-fsyntax-only) fuseld=() ;;
      esac
      # An explicit -arch overrides the arch in -target, and -mcpu is rejected
      # outright for a target it does not apply to. mig preprocesses its .defs
      # with -arch x86_64 whatever the real target is, so this is not
      # hypothetical.
      if [ "$prev" = -arch ] && [ "$a" != arm64 ]; then
        cpu=()
      fi
      prev="$a"
    done
    exec ${realBin} \
      -target ${clangTarget} \
      "''${cpu[@]}" \
      -isysroot "$SDK" \
      "''${fuseld[@]}" \
      "$@"
  '';

  simpleWrapper = name: realBin: writeShellScriptBin "${target}-${name}" ''
    exec ${realBin} "$@"
  '';

  ldWrapper = writeShellScriptBin "${target}-ld" ''
    args=()
    for a in "$@"; do
      case "$a" in
        -kernel) ;;
        *) args+=("$a") ;;
      esac
    done
    ${if nativeLd != null then ''
    exec ${nativeLd}/bin/ld "''${args[@]}"
    '' else ''
    exec ${lld}/bin/ld64.lld \
      -platform_version macos 11.0 11.3 \
      "''${args[@]}"
    ''}
  '';

  bareDsymutil = writeShellScriptBin "dsymutil" ''
    exec ${bintools}/bin/dsymutil "$@"
  '';

  xcrunShim = writeShellScriptBin "xcrun" ''
    set -e
    BINDIR="$(cd "$(dirname "$0")" && pwd)"
    SDK="''${DARWIN_SDK_ROOT:-${defaultSdkRoot}}"
    while [ $# -gt 0 ]; do
      case "$1" in
        -sdk|--sdk) shift 2 ;;
        -show-sdk-path|--show-sdk-path) echo "$SDK"; exit 0 ;;
        -show-sdk-platform-path|--show-sdk-platform-path) echo "$SDK/.."; exit 0 ;;
        -show-sdk-version|--show-sdk-version) echo "11.3"; exit 0 ;;
        -find|--find)
          shift
          case "$1" in
            cc) find_name=clang ;;
            c++) find_name=clang++ ;;
            *) find_name="$1" ;;
          esac
          if [ -x "$BINDIR/${target}-$find_name" ]; then
            echo "$BINDIR/${target}-$find_name"
          elif command -v "${target}-$find_name" >/dev/null 2>&1; then
            command -v "${target}-$find_name"
          else
            command -v "$1"
          fi
          exit 0
          ;;
        -log|-v) shift ;;
        *) break ;;
      esac
    done
    tool="$1"; shift
    case "$tool" in
      cc) wrapped=clang ;;
      c++) wrapped=clang++ ;;
      *) wrapped="$tool" ;;
    esac

    if [ -x "$BINDIR/${target}-$wrapped" ]; then
      exec -- "$BINDIR/${target}-$wrapped" "$@"
    fi
    exec -- "$tool" "$@"
  '';

  tools = [
    (compilerWrapper "clang" "${clang}/bin/clang")
    (compilerWrapper "clang++" "${clang}/bin/clang++")
    (simpleWrapper "ar" "${bintools}/bin/llvm-ar")
    (simpleWrapper "ranlib" "${bintools}/bin/llvm-ranlib")
    (simpleWrapper "strip" "${bintools}/bin/llvm-strip")
    (simpleWrapper "nm" "${bintools}/bin/llvm-nm")
    (simpleWrapper "objdump" "${bintools}/bin/llvm-objdump")
    (simpleWrapper "otool" "${hostOtool}/bin/otool")
    (simpleWrapper "dsymutil" "${bintools}/bin/dsymutil")
    (simpleWrapper "install_name_tool" "${hostOtool}/bin/install_name_tool")
    (simpleWrapper "lipo" "${bintools}/bin/llvm-lipo")
    ldWrapper
  ];
in
symlinkJoin {
  name = "darwin-cross-toolchain-nix";
  paths = tools ++ [ xcrunShim bareDsymutil ];

  meta = with lib; {
    description = "Darwin-targeting toolchain built from nixpkgs' unwrapped LLVM, no osxcross build.sh required";
    # Not linux-only: on a Darwin host these same wrappers pin the target
    # triple, the SDK and the linker, which is exactly what consumers want
    # there too - it just is not a cross toolchain in that case.
    platforms = platforms.unix;
  };
}
