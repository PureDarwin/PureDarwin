# Source trees for the components built from this repository, plus the
# upstream tarballs that need patching before use.
#
# sourceWith filters the repo down to just the prefixes a component needs, so
# editing an unrelated file does not invalidate its build.
{ fbdoomExternalSrcEnv
, libSystemSourcePaths
, pkgs
, sourceWith
}:

let
  # Every consumer that pulls in src/Kernel/xnu/osfmk (directly, or via
  # libSystemSourcePaths) uses it purely as a header search path (-I), never
  # as compiled sources - grep confirms no CMakeLists.txt under src/Libraries
  # or src/Userspace adds osfmk/*.c to target_sources (only kernelSource and
  # kextsSource, which are left untouched, actually compile it). Stripping
  # implementation files from osfmk here means an unrelated kernel-only
  # change (e.g. osfmk/ipc/ipc_policy.c) doesn't invalidate every downstream
  # userland derivation that transitively pulls in one of these sources.
  stripOsfmkImpl = src:
    pkgs.lib.cleanSourceWith {
      inherit src;
      filter = path: type:
        let
          rel = pkgs.lib.removePrefix "${toString src}/" (toString path);
          isOsfmkImpl =
            type == "regular"
            && pkgs.lib.hasPrefix "src/Kernel/xnu/osfmk/" rel
            && pkgs.lib.any (ext: pkgs.lib.hasSuffix ext rel) [ ".c" ".cpp" ".cc" ".m" ".mm" ".s" ".S" ];
        in
          !isOsfmkImpl;
    };
  fbdoomExternalSrc =
    if fbdoomExternalSrcEnv == "" then null
    else builtins.path { path = /. + fbdoomExternalSrcEnv; name = "fbdoom-external-src"; };

  chocolateDoomPatchedSrc = pkgs.runCommand "puredarwin-chocolate-doom-patched" { } ''
    mkdir -p $out
    cp -r ${pkgs.chocolate-doom.src}/opl $out/opl
    cp ${pkgs.chocolate-doom.src}/src/midifile.c ${pkgs.chocolate-doom.src}/src/midifile.h \
       ${pkgs.chocolate-doom.src}/src/mus2mid.c ${pkgs.chocolate-doom.src}/src/mus2mid.h \
       $out/
    chmod -R u+w $out
    cd $out
    patch -p1 < ${../src/Userspace/fbdoom/patches/midifile.c.patch}
    patch -p1 < ${../src/Userspace/fbdoom/patches/midifile.h.patch}
    patch -p1 < ${../src/Userspace/fbdoom/patches/mus2mid.c.patch}
    patch -p1 < ${../src/Userspace/fbdoom/patches/opl.c.patch}
    patch -p1 < ${../src/Userspace/fbdoom/patches/opl_internal.h.patch}
  '';
  kernelSource = sourceWith "puredarwin-kernel-source" [
    "projects"
    "src/Kernel/CMakeLists.txt"
    "src/Kernel/xnu"
    "src/Kernel/libfirehose_kernel"
    "src/Libraries/CMakeLists.txt"
    "src/Libraries/AvailabilityVersions"
    "src/Libraries/libSystem/CMakeLists.txt"
    "src/Libraries/libSystem/libplatform"
    "src/Libraries/libSystem/pthread"
    "tools"
  ];
  libSystemSource = stripOsfmkImpl (sourceWith "puredarwin-libsystem-source" libSystemSourcePaths);
  kextsSource = sourceWith "puredarwin-kexts-source" [
    "projects"
    "src/Kernel/CMakeLists.txt"
    "src/Kernel/xnu/EXTERNAL_HEADERS"
    "src/Kernel/Extensions"
    "src/Kernel/libkmod"
    "src/Kernel/xnu"
    "src/Kernel/libfirehose_kernel"
    "src/Libraries"
    "tools"
  ];
  userlandSource = stripOsfmkImpl (sourceWith "puredarwin-userland-source" [
    # nohup detaches from the console session through _vprocmgr_detach_from_console;
    # vproc_priv.h is private and not in the SDK.
    "src/Libraries/XPC/libxpc/include"
    "src/Kernel/xnu/osfmk"
    # ping(8) from network_cmds needs <netinet/ip_var.h>,
    # <netinet/in_systm.h>, <netinet/ip_icmp.h> and the SO_TC_* socket
    # options in <sys/socket.h>'s PRIVATE block.
    "src/Kernel/xnu/bsd"
    "src/Libraries/IOKit"
    "src/Libraries/PDGOP"
    "src/Libraries/libSystem/libmalloc/compat-include"
    "src/Libraries/libSystem/libsystem_kernel/mach"
    # 3D user-client ABI header, shared with the IOVirtIOGPU kext, used
    # used by the pd-virgl-shim static lib the Mesa winsys links.
    "src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h"
    # plain-C virgl shim (built into a static archive for Mesa's libGL)
    "src/Libraries/PDVirglShim"
    # mdnsd is built from src/Userspace/mdnsd but its sources are
    # vendored alongside the libdns_sd client.
    "src/Libraries/mDNSResponder"
    "src/Userspace"
    "tools/mig"
  ]);
  fbdoomSource = stripOsfmkImpl (sourceWith "puredarwin-fbdoom-source" [
    "src/Kernel/xnu/EXTERNAL_HEADERS"
    "src/Kernel/xnu/osfmk"
    "src/Kernel/xnu/libkern/libkern"
    "src/Kernel/xnu/libkern/os"
    # os/log_private.h includes <firehose/tracepoint_private.h>
    "src/Kernel/xnu/libkern/firehose"
    "src/Kernel/xnu/bsd/i386"
    # machine/_types.h selects the target arch header, so an arm64 build
    # of anything in this tree needs bsd/arm as well as bsd/i386.
    "src/Kernel/xnu/bsd/arm"
    "src/Kernel/xnu/bsd/bsm"
    "src/Kernel/xnu/bsd/machine"
    "src/Kernel/xnu/bsd/net"
    "src/Kernel/xnu/bsd/netinet"
    "src/Kernel/xnu/bsd/netinet6"
    "src/Kernel/xnu/bsd/pthread"
    "src/Kernel/xnu/bsd/sys"
    "src/Kernel/xnu/bsd/sys_private"
    "src/Kernel/xnu/bsd/uuid"
    "src/Kernel/xnu/bsd/kern/makesyscalls.sh"
    "src/Kernel/xnu/bsd/kern/syscalls.master"
    "src/Libraries"
    "src/Libraries/libSystem/libmalloc/compat-include"
    "src/Userspace"
    "tools/mig"
    # libobjc needs the mach-o getsection helpers compiled into libSystem.
    "tools/cctools/libmacho/getsecbyname.c"
    # NXGetArchInfo* is part of libSystem on Darwin, same libmacho source.
    "tools/cctools/libmacho/arch.c"
    "tools/cctools/include/mach-o/arch.h"
    "tools/cctools/include/stuff/openstep_mach.h"
    "tools/cctools/include/mach/machine.h"
  ]);
  cctoolsSource = stripOsfmkImpl (sourceWith "puredarwin-cctools-source" [
    "src/Kernel/xnu/osfmk"
    "src/Libraries/IOKit"
    "src/Libraries/PDGOP"
    "src/Libraries/libSystem/libmalloc/compat-include"
    "src/Libraries/libSystem/libsystem_kernel/mach"
    "src/Libraries/libcxx/include"
    # the hand-resolved __config_site every libc++ header needs
    "src/Libraries/libcxxabi/config"
    # cxxabi.h, which ld64 includes directly
    "src/Libraries/libcxxabi/include"
    "src/Libraries/libSystem/corecrypto/include"
    "src/Libraries/CommonCrypto/include"
    "src/Libraries/CommonCrypto/libcn/pd_cc_digest_bridge.c"
    "src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c"
    "src/Libraries/libSystem/libc/string/FreeBSD/strmode.c"
    "src/Userspace"
    "tools"
  ]);
  coreFoundationSource = sourceWith "puredarwin-corefoundation-source" [
    "src/Frameworks/CoreFoundation"
    "src/Libraries/libSystem/libc/pd-compat-include"
  ];
  coreServicesSource = sourceWith "puredarwin-coreservices-source" [
    "src/Libraries/CoreServices"
  ];
  securitySource = stripOsfmkImpl (sourceWith "puredarwin-security-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/Security"
      "src/Libraries/libDER"
      "src/Libraries/CommonCrypto"
      "src/Libraries/XPC/libxpc/include"
      # debugging.h includes <asl.h>
      "src/Libraries/syslog/libsystem_asl.tproj/include"
      # SecCFWrappers.h includes <IOKit/IOReturn.h>
      "src/Kernel/xnu/iokit"
      # SecItem.c/SecTrust.c include <os/activity.h>
      "src/Libraries/libsystem_trace"
      # SecPolicyLeafCallbacks.c includes <dlfcn.h>
      "src/Libraries/dyld/upstream/include"
      # SecCFError.c includes <notify.h>
      "src/Libraries/XPC/notify"
      # CommonCrypto reaches <mach-o/dyld.h> -> <architecture/byte_order.h>
      "src/Libraries/architecture"
      # SecTask.c includes <IOKit/IOKitLib.h> and <bsm/libbsm.h>
      "src/Libraries/IOKit"
      "src/Libraries/libdarwin/pd-compat-include"
    ]));
  systemConfigurationSource = stripOsfmkImpl (sourceWith "puredarwin-systemconfiguration-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/SystemConfiguration"
      # configd links bootp's IPConfiguration plugin
      "src/Libraries/bootp"
      # report_symptoms.c includes <SymptomReporter/SymptomReporter.h>
      "src/Libraries/SymptomReporter"
      # SCPreferences.h includes <Security/Security.h>, and
      # SCNetworkConfigurationPrivate.h <IOKit/IOKitLib.h>.
      "src/Libraries/Security"
      "src/Libraries/IOKit"
      # SCNetworkInterface.c reads 802.1X config keys; ip_plugin.c reads
      # PPP link states.
      "src/Libraries/eap8021x"
      "src/Libraries/ppp"
      # configd's session.c needs IOKit/IOReturn.h; SCNetworkInterface.c
      # and InterfaceNamer need the IONetworkingFamily headers.
      "src/Kernel/xnu/iokit"
      "src/Kernel/Extensions/IONetworkingFamily/include"
      "src/Kernel/Extensions/IOStorageFamily/include"
      "src/Kernel/Extensions/IOSerialFamily/include"
      "src/Kernel/Extensions/IOUSBFamily/include"
      "src/Libraries/dyld/upstream/include"
    ]));
  iokitCFSource = stripOsfmkImpl (sourceWith "puredarwin-iokitcf-source"
    (libSystemSourcePaths ++ [
      "src/Kernel/xnu/iokit"
      # IOHIDKeys.h and friends, shared between IOHIDLib and the kext. The
      # include/ tree is symlinks into IOHIDFamily/, so both are needed.
      "src/Kernel/Extensions/IOHIDFamily/include"
      "src/Kernel/Extensions/IOHIDFamily/IOHIDFamily"
      "src/Kernel/Extensions/IOHIDFamily/IOHIDSystem/IOKit"
    ]));
  symptomReporterSource = stripOsfmkImpl (sourceWith "puredarwin-symptomreporter-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/SymptomReporter"
    ]));
  protocolBufferSource = sourceWith "puredarwin-protocolbuffer-source" [
    "src/Libraries/ProtocolBuffer"
  ];
  wirelessDiagnosticsSource = sourceWith "puredarwin-wirelessdiagnostics-source" [
    "src/Libraries/WirelessDiagnostics"
  ];
  diskArbitrationSource = stripOsfmkImpl (sourceWith "puredarwin-diskarbitration-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/DiskArbitration"
      # DAServer.defs imports <Security/Authorization.h>.
      "src/Libraries/Security"
      "src/Libraries/IOKit"
      # IOMedia/IOBSD plus the CD/DVD/BD media class-name constants.
      "src/Kernel/Extensions/IOStorageFamily/include"
      "src/Kernel/Extensions/IOCDStorageFamily/include"
      "src/Kernel/Extensions/IODVDStorageFamily/include"
      "src/Kernel/Extensions/IOBDStorageFamily/include"
      "src/Kernel/xnu/iokit"
      "src/Libraries/dyld/upstream/include"
      # diskarbitrationd
      "src/Libraries/XPC"
      "src/Libraries/CommonCrypto"
      "src/Libraries/libdarwin"
      "src/Libraries/architecture"
      "src/Libraries/libsystem_trace"
    ]));
  objcSource = sourceWith "puredarwin-objc-source" [
    "src/Libraries/objc4"
    "src/Libraries/libunwind"
  ];
  libcxxDylibSource = sourceWith "puredarwin-libcxx-dylib-source" [
    "src/Libraries/libcxxabi"
    "src/Libraries/libcxx"
    "src/Libraries/libunwind"
    "src/Libraries/llvm-libc"
  ];
  foundationSource = sourceWith "puredarwin-foundation-source" [
    "src/Frameworks/Foundation"
  ];

  xfconfSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfconf/4.20/xfconf-4.20.0.tar.bz2";
    sha256 = "sha256-i8Q8YPFxaxPPNfyJnio26pxs3DR4qPBRIg7vD1NWfv0=";
  };
  libxfce4uiSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/libxfce4ui/4.20/libxfce4ui-4.20.2.tar.bz2";
    sha256 = "sha256-XT1nsSRKEM7g6JsEV2bAX+EDX3k48EEKxqPYIitd+Qc=";
  };
  xfwm4Src = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfwm4/4.20/xfwm4-4.20.0.tar.bz2";
    sha256 = "sha256-pYtj5JOXqg2NHc8GNr6TyLtZJnea71Fl4IUokBkNzwY=";
  };
  garconSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/garcon/4.20/garcon-4.20.0.tar.bz2";
    sha256 = "sha256-f7hRfBIwnKTd+LQsNLwMMV446gd7VEK/zEUJQV/q2o8=";
  };
  exoSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/exo/4.20/exo-4.20.0.tar.bz2";
    sha256 = "sha256-Qnf3mSRfHv3gHNkX/VOLprEs+RyfinP+IDX9VFbsB40=";
  };
  xfce4SessionSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfce4-session/4.20/xfce4-session-4.20.4.tar.bz2";
    sha256 = "sha256-gFw3M3jQgHVNad0vINuVzcBmyJpPAkpBQ1yg1mVxxAI=";
  };
  xfce4PanelSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfce4-panel/4.20/xfce4-panel-4.20.8.tar.bz2";
    sha256 = "sha256-1pyx83eVOusfub287xLCRr6mZYbj8oaPO3WODozj0/4=";
  };
  xfdesktopSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfdesktop/4.20/xfdesktop-4.20.2.tar.bz2";
    sha256 = "sha256-HZvXYBX7bprKBec82ZjHxm7U/IwQtibgj8LrfDnfP3s=";
  };
  # vte 0.70.6, not the 0.84 nixpkgs pins: 0.84 hard-requires simdutf,
  # fast_float and fmt (all unported C++ libraries), while 0.70 needs only
  # glib/gtk3/pango/cairo/pcre2/fribidi - everything already here - with
  # gnutls and icu behind get_option(). xfce4-terminal asks for vte >= 0.51.3,
  # so 0.70 is comfortably new enough.
  # asmjit: a real third-party JIT library, used to prove PureDarwin supports
  # code generation the way an unmodified upstream JIT expects.
  asmjitSrc = pkgs.asmjit.src;
  vteSrc = pkgs.fetchurl {
    url = "https://download.gnome.org/sources/vte/0.70/vte-0.70.6.tar.xz";
    sha256 = "sha256-6Q4gjdWrcPlnXmtTLr1cjYMJQ75A7svZ026YCXkIAVI=";
  };
  xfce4TerminalSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/apps/xfce4-terminal/1.2/xfce4-terminal-1.2.0.tar.xz";
    sha256 = "sha256-aHTHuXXMPcO9Y21X/+xyPedFggLe/mU3dZPTp+BzS5Q=";
  };
  xfce4SettingsSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfce4-settings/4.20/xfce4-settings-4.20.5.tar.bz2";
    sha256 = "sha256-pfvg5RHM4p1gMyCt5XWtQAG9Vw5g83dgIzI3ukeK/+g=";
  };
  thunarSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/thunar/4.20/thunar-4.20.9.tar.bz2";
    sha256 = "sha256-6wmGnOk7Eu0oVniWf1XyQ8gz8rry+xDJhErHZI2ScMs=";
  };
  xfce4AppfinderSrc = pkgs.fetchurl {
    url = "https://archive.xfce.org/src/xfce/xfce4-appfinder/4.20/xfce4-appfinder-4.20.0.tar.bz2";
    sha256 = "sha256-gsqC933IPihdtFQ4wv4x30RRSKqYb/6/L6q+5K+ecwQ=";
  };
in {
  inherit
    fbdoomExternalSrc
    chocolateDoomPatchedSrc
    kernelSource
    libSystemSource
    kextsSource
    userlandSource
    fbdoomSource
    cctoolsSource
    coreFoundationSource
    coreServicesSource
    securitySource
    systemConfigurationSource
    iokitCFSource
    diskArbitrationSource
    symptomReporterSource
    protocolBufferSource
    wirelessDiagnosticsSource
    objcSource
    libcxxDylibSource
    foundationSource
    xfconfSrc
    libxfce4uiSrc
    xfwm4Src
    garconSrc
    exoSrc
    xfce4SessionSrc
    xfce4PanelSrc
    xfdesktopSrc
    asmjitSrc
    vteSrc
    xfce4TerminalSrc
    xfce4SettingsSrc
    xfce4AppfinderSrc
    thunarSrc
    ;
}
