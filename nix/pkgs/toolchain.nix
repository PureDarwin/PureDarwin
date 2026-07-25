{ lib
, writeShellScriptBin
, symlinkJoin
, llvmPackages_21
, target ? "x86_64-apple-darwin20.4"
, clangTarget ? "x86_64-apple-macosx11.0"
, defaultSdkRoot ? "/usr/local/osxcross/SDK/MacOSX11.3.sdk"
}:

let
  clang = llvmPackages_21.clang-unwrapped;
  lld = llvmPackages_21.lld;
  bintools = llvmPackages_21.bintools-unwrapped;

  compilerWrapper = name: realBin: writeShellScriptBin "${target}-${name}" ''
    SDK="''${DARWIN_SDK_ROOT:-${defaultSdkRoot}}"
    export PATH="${lld}/bin:$PATH"
    # Default to lld, but only if the caller didn't already pass their own
    # -fuse-ld (xnu's kernel Makefile does, pointed at the real cctools
    # ld64 - see native-ld.nix): CMake's own internal "does this compiler
    # work" sanity check (TryCompile, run at configure time, outside any
    # of our CMakeLists) doesn't pass -fuse-ld at all, and without lld as
    # a default there it falls back to plain "ld" (real ELF ld.bfd,
    # "unrecognised emulation mode: llvm"). But passing -fuse-ld=lld
    # AHEAD of a caller-supplied -fuse-ld=<real ld> on the same command
    # line reliably segfaulted clang++ instead of the later flag cleanly
    # overriding the earlier one.
    fuseld=(-fuse-ld=lld)
    for a in "$@"; do
      case "$a" in
        -fuse-ld=*) fuseld=() ;;
      esac
    done
    exec ${realBin} \
      -target ${clangTarget} \
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
    exec ${lld}/bin/ld64.lld \
      -platform_version macos 11.0 11.3 \
      "''${args[@]}"
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
    (simpleWrapper "dsymutil" "${bintools}/bin/dsymutil")
    (simpleWrapper "install_name_tool" "${bintools}/bin/llvm-install-name-tool")
    (simpleWrapper "lipo" "${bintools}/bin/llvm-lipo")
    ldWrapper
  ];
in
symlinkJoin {
  name = "darwin-cross-toolchain-nix";
  paths = tools ++ [ xcrunShim bareDsymutil ];

  meta = with lib; {
    description = "x86_64-apple-darwin cross toolchain built from nixpkgs' unwrapped LLVM, no osxcross build.sh required";
    platforms = platforms.linux;
  };
}
