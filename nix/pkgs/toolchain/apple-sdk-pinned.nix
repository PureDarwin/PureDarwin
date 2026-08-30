{ stdenvNoCC, lib, perl, bash, ed, unifdef, zlib, libxml2, clang, migcom ? null }:

let
  workspaceRoot = ../../..;
  root = lib.fileset.toSource {
    root = workspaceRoot;
    fileset = lib.fileset.unions (map (path: workspaceRoot + path) [
      /src/Kernel/xnu/EXTERNAL_HEADERS
      /src/Kernel/xnu/bsd/arm
      /src/Kernel/xnu/bsd/bsm
      /src/Kernel/xnu/bsd/i386
      /src/Kernel/xnu/bsd/kern/makesyscalls.sh
      /src/Kernel/xnu/bsd/kern/syscalls.master
      /src/Kernel/xnu/bsd/machine
      /src/Kernel/xnu/bsd/net
      /src/Kernel/xnu/bsd/netinet
      /src/Kernel/xnu/bsd/netinet6
      /src/Kernel/xnu/bsd/sys
      /src/Kernel/xnu/bsd/uuid/uuid.h
      /src/Kernel/xnu/iokit/IOKit
      /src/Kernel/xnu/libkern/libkern
      /src/Kernel/xnu/libkern/os/base.h
      /src/Kernel/xnu/libkern/os/base_private.h
      /src/Kernel/xnu/libkern/os/log.h
      /src/Kernel/xnu/libkern/os/log_private.h
      /src/Kernel/xnu/libkern/os/overflow.h
      /src/Kernel/xnu/libkern/os/reason_private.h
      /src/Kernel/xnu/osfmk/device/device_types.h
      /src/Kernel/xnu/osfmk/i386/eflags.h
      /src/Kernel/xnu/osfmk/i386/proc_reg.h
      /src/Kernel/xnu/osfmk/kern/cs_blobs.h
      /src/Kernel/xnu/osfmk/kern/kcdata.h
      /src/Kernel/xnu/osfmk/mach
      /src/Kernel/xnu/osfmk/mach_debug
      /src/Libraries/AvailabilityVersions/include
      /src/Libraries/CoreFoundation
      /src/Libraries/IOKit/iokituser/include/IOKit
      /src/Libraries/XPC/libinfo/aliasdb.h
      /src/Libraries/XPC/libinfo/printerdb.h
      /src/Libraries/XPC/libxpc/include
      /src/Libraries/XPC/notify/notify_keys.h
      /src/Libraries/dyld/upstream/include
      /src/Libraries/libSystem/libc
      /src/Libraries/libSystem/libdispatch/dispatch
      /src/Libraries/libSystem/libdispatch/os
      /src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block.h
      /src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block_private.h
      /src/Libraries/libSystem/libmalloc/include
      /src/Libraries/libSystem/libplatform/include
      /src/Libraries/libSystem/libsystem_kernel/Platforms/MacOSX/x86_64/syscall.map
      /src/Libraries/libSystem/libsystem_kernel/include
      /src/Libraries/libSystem/libsystem_kernel/mach
      /src/Libraries/libSystem/libsystem_kernel/os
      /src/Libraries/libSystem/libsystem_kernel/wrappers
      /src/Libraries/libSystem/pthread/include
      /src/Libraries/libSystem/stub/libSystem.exports
      /src/Libraries/libcxx/include
      /src/Libraries/libcxx/vendor/llvm/default_assertion_handler.in
      /src/Libraries/CommonCrypto/include/CommonCrypto
      /src/Libraries/libcxxabi/config/__config_site
      /src/Libraries/libcxxabi/include/cxxabi.h
      /src/Libraries/libcxxabi/include/__cxxabi_config.h
      /src/Libraries/libdarwin/pd-compat-include/bsm/libbsm.h
      /src/Libraries/libdarwin/pd-compat-include/mach-o/getsect.h
      /src/Libraries/libdarwin/pd-compat-include/mach-o/ldsyms.h
      /src/Libraries/libresolv
      /src/Libraries/objc4/runtime
      /src/Libraries/syslog/libsystem_asl.tproj/include
      /tools/cctools/include/mach-o
      /tools/mig/mig.sh
    ]);
  };
  sources = [
    [ "${root}/src/Kernel/xnu/EXTERNAL_HEADERS" "usr/include" ]
    [ "${root}/src/Kernel/xnu/bsd/arm" "usr/include/arm" ]
    [ "${root}/src/Kernel/xnu/bsd/bsm" "usr/include/bsm" ]
    [ "${root}/src/Kernel/xnu/bsd/i386" "usr/include/i386" ]
    [ "${root}/src/Kernel/xnu/bsd/machine" "usr/include/machine" ]
    [ "${root}/src/Kernel/xnu/bsd/net" "usr/include/net" ]
    [ "${root}/src/Kernel/xnu/bsd/netinet" "usr/include/netinet" ]
    [ "${root}/src/Kernel/xnu/bsd/netinet6" "usr/include/netinet6" ]
    [ "${root}/src/Kernel/xnu/bsd/sys" "usr/include/sys" ]
    [ "${root}/src/Kernel/xnu/iokit/IOKit" "System/Library/Frameworks/IOKit.framework/Headers" ]
    # IOKitUser's CF-shaped client headers are separate from XNU's kernel-side
    # IOKit headers, but the SDK framework must expose both surfaces.
    [ "${root}/src/Libraries/IOKit/iokituser/include/IOKit" "System/Library/Frameworks/IOKit.framework/Headers" ]
    [ "${root}/src/Kernel/xnu/libkern/libkern" "usr/include/libkern" ]
    [ "${root}/src/Kernel/xnu/osfmk/mach" "usr/include/mach" ]
    [ "${root}/src/Libraries/AvailabilityVersions/include" "usr/include" ]
    [ "${root}/src/Libraries/dyld/upstream/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/include/mach" "usr/include/mach" ]
    [ "${root}/src/Libraries/syslog/libsystem_asl.tproj/include" "usr/include" ]
    # servers/bootstrap.h, xpc/, launch.h, vproc.h; libsystem_kernel's servers/
    # has only the netname/ls/nm/key defs.
    [ "${root}/src/Libraries/XPC/libxpc/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/libdarwin/os-include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/libdarwin/sys-include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/os" "usr/include/os" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/dispatch" "usr/include/dispatch" ]
    [ "${root}/src/Libraries/libSystem/libsystem_kernel/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libmalloc/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libplatform/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/pthread/include" "usr/include" ]
    [ "${root}/src/Libraries/libcxx/include" "usr/include/c++/v1" ]
    # Apple ships CommonCrypto in /usr/include; python's mimalloc wants it.
    [ "${root}/src/Libraries/CommonCrypto/include/CommonCrypto" "usr/include/CommonCrypto" ]
    [ "${zlib.dev}/include" "usr/include" ]
    [ "${libxml2.dev}/include/libxml2" "usr/include/libxml2" ]
  ];
  files = [
    [ "${root}/src/Kernel/xnu/EXTERNAL_HEADERS/stdatomic.h" "usr/include/puredarwin/stdatomic.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/base.h" "usr/include/os/base.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/base_private.h" "usr/include/os/base_private.h" ]
    [ "${root}/src/Kernel/xnu/bsd/uuid/uuid.h" "usr/include/uuid/uuid.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/i386/eflags.h" "usr/include/i386/eflags.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/i386/proc_reg.h" "usr/include/i386/proc_reg.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/log.h" "usr/include/os/log.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/log_private.h" "usr/include/os/log_private.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/overflow.h" "usr/include/os/overflow.h" ]
    [ "${root}/src/Kernel/xnu/libkern/os/reason_private.h" "usr/include/os/reason_private.h" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/os/object.h" "usr/include/os/object.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/pthread.h" "usr/include/pthread.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/sched.h" "usr/include/sched.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/grp.h" "usr/include/grp.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/math.h" "usr/include/math.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/netdb.h" "usr/include/netdb.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/pwd.h" "usr/include/pwd.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/MacTypes.h" "usr/include/MacTypes.h" ]
    # The userspace mach.h, not xnu's osfmk one: only this declares slot_name and
    # pulls mach_traps.h. Installed here so it lands after headers.sh's copy.
    [ "${root}/src/Libraries/libSystem/libc/include/mach/mach.h" "usr/include/mach/mach.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/copyfile.h" "usr/include/copyfile.h" ]
    # Individually: pd-compat-include's mach-o/loader.h is worse than dyld's.
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/bsm/libbsm.h" "usr/include/bsm/libbsm.h" ]
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/mach-o/ldsyms.h" "usr/include/mach-o/ldsyms.h" ]
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/mach-o/getsect.h" "usr/include/mach-o/getsect.h" ]
    # FreeBSD's fenv.h renamed; reaches these by angle bracket. Only these - e.g.
    # openlibm_complex.h defines a bare "I".
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv.h" "usr/include/fenv.h" ]
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv_amd64.h" "usr/include/openlibm_fenv_amd64.h" ]
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv_arm.h" "usr/include/openlibm_fenv_arm.h" ]
    # From cctools, not xnu: xnu's copy has no FAT_MAGIC_64/fat_arch_64, and
    # still does not as of xnu-12377. Same for arch.h.
    [ "${root}/tools/cctools/include/mach-o/fat.h" "usr/include/mach-o/fat.h" ]
    [ "${root}/tools/cctools/include/mach-o/arch.h" "usr/include/mach-o/arch.h" ]
    [ "${root}/tools/cctools/include/mach-o/swap.h" "usr/include/mach-o/swap.h" ]
    [ "${root}/tools/cctools/include/mach-o/ranlib.h" "usr/include/mach-o/ranlib.h" ]
    [ "${root}/src/Libraries/libresolv/include/ifaddrs.h" "usr/include/ifaddrs.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/kern/kcdata.h" "usr/include/kern/kcdata.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/mach/mach_eventlink_types.h" "usr/include/mach/mach_eventlink_types.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/kern/cs_blobs.h" "usr/include/kern/cs_blobs.h" ]
    [ "${root}/src/Kernel/xnu/bsd/sys/fileport.h" "usr/include/sys/fileport.h" ]
    [ "${root}/src/Kernel/xnu/bsd/sys/codesign.h" "usr/include/sys/codesign.h" ]
    # MIG-generated device_user.h includes this.
    [ "${root}/src/Kernel/xnu/osfmk/device/device_types.h" "usr/include/device/device_types.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/sysdir.h" "usr/include/sysdir.h" ]
    [ "${root}/src/Libraries/XPC/notify/notify_keys.h" "usr/include/notify_keys.h" ]
    # Reconstructed; see the headers for where the layouts came from.
    [ "${root}/src/Libraries/XPC/libinfo/aliasdb.h" "usr/include/aliasdb.h" ]
    [ "${root}/src/Libraries/XPC/libinfo/printerdb.h" "usr/include/printerdb.h" ]
    # libdispatch's userspace BlocksRuntime, not xnu's libkern copy.
    [ "${root}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block.h" "usr/include/Block.h" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block_private.h" "usr/include/Block_private.h" ]
    [ "${root}/src/Libraries/libresolv/resolv.h" "usr/include/resolv.h" ]
    # Both spellings: resolv.h includes <nameser.h>, everyone else uses the
    # <arpa/nameser.h> Darwin ships (its nameser_compat.h sibling is already there).
    [ "${root}/src/Libraries/libresolv/nameser.h" "usr/include/nameser.h" ]
    [ "${root}/src/Libraries/libresolv/nameser.h" "usr/include/arpa/nameser.h" ]
    [ "${root}/src/Libraries/libresolv/dns.h" "usr/include/dns.h" ]
    [ "${root}/src/Libraries/libresolv/dns_util.h" "usr/include/dns_util.h" ]
    [ "${root}/src/Libraries/libSystem/libplatform/include/setjmp.h" "usr/include/setjmp.h" ]
    [ "${root}/src/Libraries/libcxxabi/config/__config_site" "usr/include/c++/v1/__config_site" ]
    [ "${root}/src/Libraries/libcxx/vendor/llvm/default_assertion_handler.in" "usr/include/c++/v1/__assertion_handler" ]
    # Apple ships these next to the libc++ headers, and ld64 includes <cxxabi.h>.
    [ "${root}/src/Libraries/libcxxabi/include/cxxabi.h" "usr/include/c++/v1/cxxabi.h" ]
    [ "${root}/src/Libraries/libcxxabi/include/__cxxabi_config.h" "usr/include/c++/v1/__cxxabi_config.h" ]
  ];
  libSystemSymbolSources = [
    "${root}/src/Libraries/libSystem/stub/libSystem.exports"
    "${root}/src/Libraries/libSystem/libc/scripts/legacy_alias.list"
    # x86_64, not i386: the tbd targets 64-bit, and only the i386 map carries the
    # 60 $UNIX2003 aliases that 64-bit does not have.
    "${root}/src/Libraries/libSystem/libsystem_kernel/Platforms/MacOSX/x86_64/syscall.map"
    "${./libSystem-compat.exports}"
  ];
  installSource = source: ''
    mkdir -p "$sdk/${source.target}"
    chmod -R u+w "$sdk/${source.target}"
    cp -RL ${source.path}/. "$sdk/${source.target}/"
  '';
  installFile = source: ''install -Dm644 ${source.path} "$sdk/${source.target}"'';
in
stdenvNoCC.mkDerivation {
  pname = "puredarwin-oss-sdk";
  version = "20.4";

  dontUnpack = true;
  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;
  nativeBuildInputs = [ perl bash ed unifdef ] ++ lib.optional (migcom != null) clang;

  installPhase = ''
    runHook preInstall
    sdk="$out/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${lib.concatMapStrings installSource (map (entry: { path = builtins.elemAt entry 0; target = builtins.elemAt entry 1; }) sources)}
    headers_script="$TMPDIR/headers.sh"
    cp ${root}/src/Libraries/libSystem/libc/scripts/headers.sh "$headers_script"
    features_script="$TMPDIR/generate_features.pl"
    cp ${root}/src/Libraries/libSystem/libc/scripts/generate_features.pl "$features_script"
    sed -i '1c#!${perl}/bin/perl' "$features_script"
    # The source script has a stale /usr/bin/perl shebang. Patch the private
    # copy instead of rewriting its command line; the latter is fragile because
    # SRCROOT is expanded by both Nix and the shell.
    sed -i '1c#!${perl}/bin/perl' "$headers_script"
    sed -i "s|''${SRCROOT}/scripts/generate_features.pl|$features_script|g" "$headers_script"
    sed -i "s|''${SRCROOT}/build/generate_features.pl|$features_script|g" "$headers_script"
    ARCHS=x86_64 SRCROOT=${root}/src/Libraries/libSystem/libc \
      GENERATE_FEATURES="$features_script" \
      DERIVED_FILES_DIR="$TMPDIR/libc-derived" VARIANT_PLATFORM_NAME=macosx \
      DEPLOYMENT_LOCATION=NO BUILT_PRODUCTS_DIR="$TMPDIR/libc-headers" \
      SDK_INSTALL_HEADERS_ROOT="" bash "$headers_script"
    chmod -R u+w "$sdk/usr/include"
    cp -RL "$TMPDIR/libc-headers/usr/include/." "$sdk/usr/include/"
    mkdir -p "$TMPDIR/xnu-syscalls"
    (
      cd "$TMPDIR/xnu-syscalls"
      bash ${root}/src/Kernel/xnu/bsd/kern/makesyscalls.sh \
        ${root}/src/Kernel/xnu/bsd/kern/syscalls.master header
    )
    install -Dm644 "$TMPDIR/xnu-syscalls/syscall.h" \
      "$sdk/usr/include/sys/syscall.h"
    # Hand-written and reconstructed SDK content, laid out as it lands. After
    # the generated headers above so it wins where both supply a file.
    cp -RL ${./oss-sdk}/. "$sdk/"
    chmod -R u+w "$sdk/usr"
    # Apple resolves the PLATFORM_* blocks when it builds an SDK; the xnu copy
    # has none defined, so this reads 0 and 64-bit calls asm-rename to $UNIX2003.
    sed -i '1i #define __DARWIN_ONLY_UNIX_CONFORMANCE 1' "$sdk/usr/include/sys/cdefs.h"
    # Quote-included by the openlibm_fenv_*.h above, and openlibm keeps them in
    # src/ rather than include/.
    for h in cdefs-compat types-compat; do
      install -Dm644 ${root}/src/Libraries/libSystem/libc/libm/openlibm/src/$h.h \
        "$sdk/usr/include/$h.h"
    done
    # launchd resolves <CoreFoundation/*.h> through -F, so CF needs framework
    # shape. Flattened from the subprojects rather than tracking CF's own
    # PUBLIC_HEADERS list; over-inclusive, but that list drifts.
    mkdir -p "$sdk/System/Library/Frameworks/CoreFoundation.framework/Headers"
    # One cp per file: a couple of basenames repeat across subprojects and a
    # single cp refuses to overwrite what it just created.
    for h in ${root}/src/Libraries/CoreFoundation/*.subproj/*.h; do
      cp -Lf "$h" "$sdk/System/Library/Frameworks/CoreFoundation.framework/Headers/"
    done
    # Not a sources entry: runtime/ mixes headers with .mm/.cpp. Same *.h set
    # libobjc.nix flattens into its own <objc/*.h> dir.
    mkdir -p "$sdk/usr/include/objc"
    cp -RL ${root}/src/Libraries/objc4/runtime/*.h "$sdk/usr/include/objc/"
    mkdir -p "$sdk/usr/include/puredarwin"
    ${lib.concatMapStringsSep "\n" installFile (map (entry: { path = builtins.elemAt entry 0; target = builtins.elemAt entry 1; }) files)}
    # libc's generated private headers include this availability shim directly.
    # Keep it in the SDK root so targets built with -nostdinc can still resolve it.
    install -Dm644 ${root}/src/Libraries/AvailabilityVersions/include/AvailabilityInternal.h \
      "$sdk/usr/include/AvailabilityInternalPrivate.h"
    {
      printf '%s\n' '--- !tapi-tbd' 'tbd-version: 4' \
        'targets: [ x86_64-macos, arm64-macos ]' \
        "install-name: '/usr/lib/libSystem.B.dylib'" \
        'current-version: 1292.100.5' 'compatibility-version: 1' \
        'exports:' '  - targets: [ x86_64-macos, arm64-macos ]'
      printf '    symbols: [ '
      separator=
      for source in ${lib.escapeShellArgs libSystemSymbolSources}; do
        while read -r symbol remainder; do
          case "$symbol" in
            ""|'#'*) continue ;;
          esac
          printf '%s%s' "$separator" "$symbol"
          separator=', '
        done < "$source"
      done
      printf '%s\n' ' ]' '...'
    } > "$sdk/usr/lib/libSystem.tbd"
    for arch in x86_64 arm64; do
      mkdir -p "$sdk/usr/include/$arch"
      ARCHS="$arch" SRCROOT=${root}/src/Libraries/libSystem/libc \
        DERIVED_FILES_DIR="$TMPDIR/libc-features" VARIANT_PLATFORM_NAME=macosx \
        perl ${root}/src/Libraries/libSystem/libc/scripts/generate_features.pl
      cp "$TMPDIR/libc-features/$arch/libc-features.h" "$sdk/usr/include/$arch/"
    done
    ln -s MacOSX.sdk "$out/Platforms/MacOSX.platform/Developer/SDKs/MacOSX20.4.sdk"
    # <mach/*.h> RPC interfaces, generated from xnu's .defs rather than carried
    # pre-generated. Skipped for the base SDK that builds migcom itself.
    ${lib.optionalString (migcom != null) ''
      export MIGCOM=${migcom}/bin/migcom
      # mig.sh always passes -arch, which only a Darwin-targeting clang accepts.
      cat > "$TMPDIR/migcc" <<EOF
#!/bin/sh
exec ${clang}/bin/clang -target x86_64-apple-darwin20.4 "\$@"
EOF
      chmod +x "$TMPDIR/migcc"
      export MIGCC="$TMPDIR/migcc"
      for d in clock clock_priv clock_reply exc host_priv host_security \
               mach_host mach_port mach_vm mach_voucher memory_entry processor \
               processor_set task thread_act vm_map mach_eventlink; do
        defs="${root}/src/Kernel/xnu/osfmk/mach/$d.defs"
        [ -e "$defs" ] || continue
        bash ${root}/tools/mig/mig.sh -novouchers \
          -I"$sdk/usr/include" -isysroot "$sdk" \
          -user /dev/null -server /dev/null \
          -header "$sdk/usr/include/mach/$d.h" "$defs" || exit 1
      done
    ''}
    # Generated Mach interfaces use a few newer XNU types that are not in the
    # 11.3 SDK's public mach_types.h. Apply this after MIG generation.
    compat="$sdk/usr/include/mach/pd_mach_compat.h"
    mkdir -p "$(dirname "$compat")"
    printf '%s\n' \
      '#ifndef _PD_MACH_COMPAT_H_' '#define _PD_MACH_COMPAT_H_' \
      '#ifndef __ASSEMBLER__' \
      '#include <stdint.h>' \
      '#include <mach/vm_types.h>' \
      '#ifndef PD_TASK_CORPSE_FORKING_BEHAVIOR_T_DEFINED' \
      '#define PD_TASK_CORPSE_FORKING_BEHAVIOR_T_DEFINED' \
      'typedef uint32_t task_corpse_forking_behavior_t;' '#endif' \
      '#ifndef PD_KCDATA_OBJECT_T_DEFINED' '#define PD_KCDATA_OBJECT_T_DEFINED' \
      'typedef uint32_t kcdata_object_t;' '#endif' \
      '#ifndef PD_IO_MAIN_T_DEFINED' '#define PD_IO_MAIN_T_DEFINED' \
      'typedef uint32_t io_main_t;' '#endif' \
      '#ifndef PD_MACH_SERVICE_PORT_INFO_DATA_T_DEFINED' \
      '#define PD_MACH_SERVICE_PORT_INFO_DATA_T_DEFINED' \
      '#define MACH_SERVICE_PORT_INFO_STRING_NAME_MAX_BUF_LEN 255' \
      'typedef struct mach_service_port_info {' \
      '  char mspi_string_name[MACH_SERVICE_PORT_INFO_STRING_NAME_MAX_BUF_LEN];' \
      '  uint8_t mspi_domain_type;' \
      '} mach_service_port_info_data_t;' '#endif' '#endif' '#endif' > "$compat"
    find "$sdk" \( -path '*/usr/include/mach/mach_interface.h' -o \
      -path '*/usr/include/mach/task.h' -o \
      -path '*/usr/include/mach/mach_port.h' -o \
      -path '*/usr/include/mach/mach_host.h' -o \
      -path '*/usr/include/mach/mach_eventlink.h' \) -print | while read -r h; do
      sed -i '1i #include <mach/pd_mach_compat.h>' "$h"
    done
    if [ -e "$sdk/usr/include/mach/mach_eventlink.h" ]; then
      sed -i '1i #include <mach/mach_eventlink_types.h>' "$sdk/usr/include/mach/mach_eventlink.h"
    fi
    if [ -e "$sdk/usr/include/mach/mach_interface.h" ]; then
      # The generated Mach client declarations are consumed by C++ runtimes
      # as well as C clients. Keep their ABI names unmangled.
      sed -i '2i #ifdef __cplusplus\nextern "C" {\n#endif' "$sdk/usr/include/mach/mach_interface.h"
      sed -i '$i #ifdef __cplusplus\n}\n#endif' "$sdk/usr/include/mach/mach_interface.h"
    fi
    # Some Darwin consumers include mach_host.h without mach_init.h, although
    # mach_host_self() is traditionally declared by the latter. Keep the
    # public host interface self-contained like the system SDKs do.
    cat >> "$sdk/usr/include/mach/mach_host.h" <<'EOF'

#ifdef __cplusplus
extern "C" {
#endif
extern mach_port_t mach_host_self(void);
#ifdef __cplusplus
}
#endif
EOF
    cat >> "$sdk/usr/include/mach/thread_policy.h" <<'EOF'

#ifdef __cplusplus
extern "C" {
#endif
extern kern_return_t thread_policy_set(mach_port_t thread,
    thread_policy_flavor_t flavor, thread_policy_t policy_info,
    mach_msg_type_number_t policy_infoCnt);
#ifdef __cplusplus
}
#endif
EOF
    # MIG emits only the _kernelrpc_* declarations in mach_port.h. The
    # userspace wrappers are implemented by libsystem_kernel and are also
    # consumed directly by C++ projects such as LLVM.
    cat >> "$sdk/usr/include/mach/mach_port.h" <<'EOF'

#ifdef __cplusplus
extern "C" {
#endif
extern kern_return_t mach_port_type(ipc_space_t, mach_port_name_t, mach_port_type_t *);
extern kern_return_t mach_port_allocate(ipc_space_t, mach_port_right_t, mach_port_name_t *);
extern kern_return_t mach_port_deallocate(ipc_space_t, mach_port_name_t);
extern kern_return_t mach_port_mod_refs(ipc_space_t, mach_port_name_t, mach_port_right_t, mach_port_delta_t);
extern kern_return_t mach_port_insert_right(ipc_space_t, mach_port_name_t, mach_port_t, mach_msg_type_name_t);
extern kern_return_t mach_port_get_attributes(ipc_space_read_t, mach_port_name_t, mach_port_flavor_t, mach_port_info_t, mach_msg_type_number_t *);
extern kern_return_t mach_port_insert_member(ipc_space_t, mach_port_name_t, mach_port_name_t);
extern kern_return_t mach_port_extract_member(ipc_space_t, mach_port_name_t, mach_port_name_t);
extern kern_return_t mach_port_space_info(ipc_space_read_t, ipc_info_space_t *, ipc_info_name_array_t *, mach_msg_type_number_t *, ipc_info_tree_name_array_t *, mach_msg_type_number_t *);
extern kern_return_t mach_port_construct(ipc_space_t, mach_port_options_ptr_t, mach_port_context_t, mach_port_name_t *);
extern kern_return_t mach_port_destruct(ipc_space_t, mach_port_name_t, mach_port_delta_t, mach_port_context_t);
#ifdef __cplusplus
}
#endif
EOF
    runHook postInstall
  '';

  meta = with lib; {
    description = "Redistributable PureDarwin SDK assembled from open-source headers";
    license = licenses.free;
    platforms = platforms.unix;
  };
}
