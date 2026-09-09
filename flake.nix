{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    iig-tools.url = "github:PureDarwin/iig-tools";
    kc-tools.url = "github:PureDarwin/kc-tools/xnu-12377";
    xnu-loader.url = "github:PureDarwin/xnu-loader/xnu-12377";
  };

  outputs = { self, nixpkgs, iig-tools, kc-tools, xnu-loader }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = lib.genAttrs systems;

      mkSystem = system:
        let
          basePkgs = import nixpkgs { inherit system; };

          # Two stages so the MIG-generated <mach/*.h> can be produced at build
          # time instead of being carried pre-generated: the base SDK has the
          # hand-written headers migcom itself needs, and the final SDK adds
          # what migcom emits from xnu's .defs.
          appleSdkBase = basePkgs.callPackage ./nix/pkgs/toolchain/apple-sdk-pinned.nix { };
          sdkMigcom = basePkgs.callPackage ./nix/pkgs/toolchain/migcom.nix {
            appleSdk = appleSdkBase;
          };
          appleSdk = basePkgs.callPackage ./nix/pkgs/toolchain/apple-sdk-pinned.nix {
            migcom = sdkMigcom;
          };
          pkgs = basePkgs.extend (_: _: { inherit appleSdk; });
          isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
          # fbDOOM (GPL, opt-in - see src/Userspace/fbdoom/CMakeLists.txt) is
          # an external checkout, not a flake input: point PUREDARWIN_FBDOOM_SOURCE_ENV
          # at it (requires --impure). Mirrors the SDK tarball's requireFile
          # pattern - never hardcode a personal machine path into this file.
          fbdoomExternalSrcEnv = builtins.getEnv "PUREDARWIN_FBDOOM_SOURCE_ENV";
          # Component source trees (see nix/sources.nix).
          sources = import ./nix/sources.nix {
            inherit pkgs sourceWith sourceWithExtraFilter frameworkHeadersOnly libSystemSourcePaths fbdoomExternalSrcEnv;
          };
          inherit (sources)
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
          iig = iig-tools.packages.${system}.default or (
            (pkgs.callPackage iig-tools { }).overrideAttrs (old: {
              meta = (old.meta or { }) // {
                platforms = pkgs.lib.platforms.unix;
              };
            })
          );
          # Bootstrap toolchain: ld64.lld, used only to build the real cctools
          # ld64 below. Everything else uses darwinCrossToolchain, which has the
          # real linker - lld silently ignores -dylib_file/-image_base/-segaddr.
          bootstrapCrossToolchain =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix { };
          libtapi = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/libtapi.nix { };
          # A Darwin host already has Apple's ld64 in nixpkgs, so it needs none
          # of the bootstrap-lld-then-build-cctools dance the Linux cross build
          # goes through to obtain a TAPI-capable linker.
          nativeLd =
            if isDarwin then pkgs.ld64
            else pkgs.callPackage ./nix/pkgs/toolchain/native-ld.nix {
              darwinCrossToolchain = bootstrapCrossToolchain;
              inherit libtapi iig;
            };
          # Not actually a *cross* toolchain on Darwin - the wrappers pin the
          # target triple, the pinned SDK and the linker, which is what every
          # consumer wants on either host. toolchain.nix is built from nixpkgs'
          # llvmPackages_21, which exists on both.
          darwinCrossToolchain = pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
          };
          arm64CrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
            target = "arm64-apple-darwin20.4";
            clangTarget = "arm64-apple-macosx26.5";
          };
          # The Pi Zero's ARM1176 is ARMv6; there is no macosx deployment
          # target for 32-bit ARM, so the triple stays a plain darwin one.
          armv6CrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/toolchain.nix {
            inherit nativeLd;
            target = "armv6-apple-darwin20.4";
            clangTarget = "armv6-apple-darwin20.4";
          };
          nativeUnifdef = pkgs.callPackage ./nix/pkgs/toolchain/unifdef.nix { };
          nativeMigcom = pkgs.callPackage ./nix/pkgs/toolchain/migcom.nix { };
          libapfsrwBuild = pkgs.callPackage ./nix/pkgs/apple/libapfsrw.nix { };

          sourceWithExtraFilter = name: prefixes: extraFilter:
            lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let
                  rel = lib.removePrefix "${toString ./.}/" (toString path);
                  isParentOfPrefix = prefix:
                    lib.hasPrefix "${rel}/" prefix;
                in
                  extraFilter rel type
                  && (rel == "CMakeLists.txt"
                  || rel == "src/CMakeLists.txt"
                  || rel == "cmake"
                  || lib.hasPrefix "cmake/" rel
                  || lib.any (prefix:
                    rel == prefix
                    || lib.hasPrefix "${prefix}/" rel
                    || (type == "directory" && isParentOfPrefix prefix)
                  ) prefixes);
            };
          sourceWith = name: prefixes:
            sourceWithExtraFilter name prefixes (rel: type: true);
          # The top-level CMakeLists.txt globs *.h out of the CoreFoundation and
          # Foundation trees for a header search path, and nothing built from
          # libSystemSource compiles their sources. Dropping the implementation
          # files keeps a .m edit in either framework from invalidating
          # libSystem - and so from rebuilding LLVM, which depends on it.
          frameworkHeadersOnly = rel: type:
            !(type == "regular"
              && (lib.hasPrefix "src/Frameworks/CoreFoundation/" rel
                  || lib.hasPrefix "src/Frameworks/Foundation/" rel)
              && !(lib.hasSuffix ".h" rel));
          libSystemSourcePaths = [
            "src/Kernel/xnu/EXTERNAL_HEADERS"
            "src/Kernel/xnu/osfmk"
            "src/Kernel/xnu/libkern/libkern"
            "src/Kernel/xnu/libkern/libkern/arm"
            "src/Kernel/xnu/libkern/os"
            # os/log_private.h includes <firehose/tracepoint_private.h>
            "src/Kernel/xnu/libkern/firehose"
            "src/Kernel/xnu/bsd/arm"
            "src/Kernel/xnu/bsd/i386"
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
            "src/Frameworks/CoreFoundation"
            "src/Frameworks/Foundation"
            "src/Libraries"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "tools/mig"
            # libobjc needs the mach-o getsection helpers compiled into libSystem.
            "tools/cctools/libmacho/getsecbyname.c"
            # NXGetArchInfo* is part of libSystem on Darwin, same libmacho source.
            "tools/cctools/libmacho/arch.c"
            # mach-o/utils.h (macOS 13) arch-name helpers, over that same table.
            "tools/cctools/libmacho/utils.c"
            "tools/cctools/include/mach-o/utils.h"
            "tools/cctools/include/mach-o/arch.h"
            "tools/cctools/include/stuff/openstep_mach.h"
            "tools/cctools/include/mach/machine.h"
          ];
          mkPureDarwinBuild = args: pkgs.callPackage ./build.nix ({
            inherit appleSdk darwinCrossToolchain nativeLd nativeUnifdef nativeMigcom iig;
            compilerRt = compilerRtBuild;
            compilerRtArm64 = arm64.compilerRtArm64Build or null;
          } // args);

          userlandBuild = mkPureDarwinBuild {
            pname = "puredarwin-userland";
            src = userlandSource;
            buildTargets = [ "sw_vers" "ps" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "ping" "pcmplay" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" ]
              # shell_cmds (+ tsort/uuencode/uudecode)
              ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
              # text_cmds
              ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ]
              ++ lib.optionals (!isDarwin) [ "puredarwingop_drv" "puredarwininput_drv" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableTools = false;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
            xorgDriverIncludes = if isDarwin then null else [
              "${xorgBuild}/usr/include/xorg"
              "${xorgBuild}/usr/include"
              "${lib.getDev pkgs.xorgproto}/include"
              "${xvfbPixmanBuild}/include/pixman-1"
            ];
          };
          # Wayland-only image: the same userland without the Xorg DDX drivers,
          # which are what drag the whole Xorg tree into the build graph.
          # startx stays in the target list - it is plain libSystem and only
          # execs Xorg at runtime, so it costs no dependency, and its install
          # rule is unconditional in CMake. It is removed from the image in
          # splitBaseSystemWayland instead.
          userlandNoxBuild = mkPureDarwinBuild {
            pname = "puredarwin-userland-nox";
            src = userlandSource;
            buildTargets = [ "sw_vers" "ps" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "ping" "pcmplay" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" ]
              ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
              ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableTools = false;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
          };
          tccBuild = mkPureDarwinBuild {
            pname = "puredarwin-tcc";
            src = userlandSource;
            buildTargets = [ "tcc" ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableTools = false;
            enableTcc = true;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
          };
          cctoolsBuild = mkPureDarwinBuild {
            pname = "puredarwin-cctools";
            src = cctoolsSource;
            buildTargets = [
              "lipo_selfhost" "size_selfhost" "strings_selfhost" "checksyms_selfhost"
              "iig_selfhost" "ld64_selfhost"
              "ar_selfhost" "nm_selfhost" "libtool_selfhost" "ranlib_selfhost"
              "otool_selfhost"
              "redo_prebinding_selfhost"
              "seg_hack_selfhost" "install_name_tool_selfhost"
              "indr_selfhost" "strip_selfhost" "segedit_selfhost" "pagestuff_selfhost"
              "codesign_allocate_selfhost" "bitcode_strip_selfhost" "ctf_insert_selfhost"
              "check_dylib_selfhost" "cmpdylib_selfhost" "inout_selfhost"
              "nmedit_selfhost"
            ];
            enableProjects = false;
            enableKernel = false;
            enableLibraries = false;
            enableUserspace = false;
            enableTools = true;
            installUserland = true;
            installKernel = false;
            prebuiltLibSystem = libSystemBuild;
            extraCmakeFlags = [
              "-DPUREDARWIN_ENABLE_SELFHOST_CCTOOLS=ON"
              "-DPUREDARWIN_IIG_SOURCE=${iig-tools}"
            ];
          };
          xvfbPixmanBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-pixman.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) pixman;
            };
          xvfbLibXauBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-stub-lib.nix {
              inherit darwinCrossToolchain;
              name = "Xau";
              version = pkgs.libxau.version or "1.0.12";
              pcName = "xau";
              pcDescription = "X authorization file management library";
              includeFrom = [ pkgs.libxau pkgs.xorgproto ];
              source = ''
                void *XauGetBestAuthByAddr(unsigned int family, unsigned int address_length, const char *address, unsigned int number_length, const char *number, int types_length, char **types, const int *type_lengths) { (void)family; (void)address_length; (void)address; (void)number_length; (void)number; (void)types_length; (void)types; (void)type_lengths; return 0; }
                void *XauReadAuth(const char *auth_file_name) { (void)auth_file_name; return 0; }
                void XauDisposeAuth(void *auth) { (void)auth; }
              '';
            };
          xvfbLibXdmcpBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain;
              nativeLd = nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXdmcp";
              version = pkgs.libxdmcp.version;
              src = pkgs.libxdmcp.src;
              deps = [ pkgs.xorgproto ];
            };
          xvfbZlibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-zlib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib;
            };
          # Always shared: fontconfig is a dylib and records a dependency on
          # /lib/libfreetype.6.N.dylib, so a static freetype cannot satisfy it.
          freetype2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-freetype.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              inherit (pkgs) zlib freetype;
            };
          libfontencBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libfontenc";
              version = pkgs.libfontenc.version;
              src = pkgs.libfontenc.src;
              deps = [ pkgs.xorgproto xvfbZlibBuild ];
            };
          xvfbLibXfont2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXfont2";
              version = pkgs.libxfont_2.version;
              src = pkgs.libxfont_2.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xvfbZlibBuild
                freetype2Build
                libfontencBuild
              ];
              configureFlags = [
                "--disable-devel-docs"
              ];
            };
          xlibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libX11";
              version = pkgs.libX11.version;
              src = pkgs.libX11.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xcbBuild
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              configureFlags = [
                "--disable-specs"
                "--enable-xlocaledir"
              ];
            };
          xcbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb";
              version = pkgs.libxcb.version;
              src = pkgs.libxcb.src;
              deps = [
                pkgs.xorgproto
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              nativeDeps = [
                pkgs.python3
                pkgs.xcb-proto
              ];
              configureFlags = [
                "--disable-devel-docs"
              ];
              preConfigureExtra = ''
                export PYTHONPATH="${pkgs.xcb-proto}/${pkgs.python3.sitePackages}:$PYTHONPATH"
              '';
            };
          xcbUtilBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-util";
              version = pkgs.libxcb-util.version;
              src = pkgs.libxcb-util.src;
              deps = [ pkgs.xorgproto xcbBuild ];
            };
          xcbKeysymsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-keysyms";
              version = pkgs.libxcb-keysyms.version;
              src = pkgs.libxcb-keysyms.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbWmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-wm";
              version = pkgs.libxcb-wm.version;
              src = pkgs.libxcb-wm.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
              nativeDeps = [ pkgs.m4 ];
            };
          xcbRenderUtilBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-render-util";
              version = pkgs.libxcb-render-util.version;
              src = pkgs.libxcb-render-util.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbImageBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-image";
              version = pkgs.libxcb-image.version;
              src = pkgs.libxcb-image.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild xcbRenderUtilBuild ];
              postPatchExtra = ''
                sed -i 's/^SUBDIRS = image test/SUBDIRS = image/' Makefile.in
              '';
            };
          xcbCursorBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-cursor";
              version = pkgs.libxcb-cursor.version;
              src = pkgs.libxcb-cursor.src;
              deps = [
                pkgs.xorgproto
                xcbBuild
                xcbUtilBuild
                xcbKeysymsBuild
                xcbImageBuild
                xcbRenderUtilBuild
              ];
              nativeDeps = [ pkgs.m4 ];
            };
          xcbXrmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-xcb-util-xrm";
              version = pkgs.xcbutilxrm.version;
              src = pkgs.xcbutilxrm.src;
              deps = [ pkgs.xorgproto xlibBuild xcbBuild xcbUtilBuild ];
              nativeDeps = [ pkgs.m4 pkgs.util-macros ];
              configureFlags = [
                "--disable-devel-docs"
              ];
            };
          libevBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libev";
              version = pkgs.libev.version;
              src = pkgs.libev.src;
              preConfigureExtra = ''
                export ac_cv_func_poll=yes
                export ac_cv_func_select=yes
                export ac_cv_header_poll_h=yes
              '';
            };
          pcre2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-pcre2";
              version = pkgs.pcre2.version;
              src = pkgs.pcre2.src;
              configureFlags = [
                "--disable-pcre2-16"
                "--disable-pcre2-32"
                "--disable-jit"
                "--disable-pcre2grep-jit"
                "--disable-pcre2grep-callout"
                "--disable-pcre2grep-callout-fork"
              ];
            };
          yajlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/yajl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) yajl;
            };
          startupNotificationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-startup-notification";
              version = pkgs.libstartup_notification.version;
              src = pkgs.libstartup_notification.src;
              deps = [ pkgs.xorgproto xlibBuild xcbBuild xcbUtilBuild ];
              configureFlags = [
                "--x-includes=${lib.getDev xlibBuild}/include"
                "--x-libraries=${xlibBuild}/lib"
              ];
              preConfigureExtra = ''
                export lf_cv_sane_realloc=yes
              '';
              postPatchExtra = ''
                sed -i 's/^SUBDIRS=libsn test doc/SUBDIRS=libsn/' Makefile.in
              '';
            };
          cairoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/cairo.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) cairo xorgproto;
              pixman = xvfbPixmanBuild;
              zlib = xvfbZlibBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              freetype = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              libpng = libpngBuild;
            };
          libffiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libffi";
              version = pkgs.libffi.version;
              src = pkgs.libffi.src;
              configureFlags = [
                "--disable-docs"
                "--disable-multi-os-directory"
              ];
            };
          glibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/glib.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) glib;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
            };
          expatBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-expat";
              version = pkgs.expat.version;
              src = pkgs.expat.src;
              configureFlags = [
                "--without-docbook"
                "--without-examples"
                "--without-tests"
              ];
            };
          fontconfigBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/fontconfig.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fontconfig;
              nativeMesonTools = nativeMesonToolsDir;
              freetype = freetype2Build;
              expat = expatBuild;
            };
          fribidiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/fribidi.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fribidi;
            };
          harfbuzzBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/harfbuzz.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) harfbuzz;
              freetype = freetype2Build;
              icu = icuCoreBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              libiconv = libiconvBuild;
              cairo = cairoBuild;
              pixman = xvfbPixmanBuild;
              libpng = libpngBuild;
              zlib = xvfbZlibBuild;
              expat = expatBuild;
              fontconfig = fontconfigBuild;
            };
          pangoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/pango.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) pango;
              glib = glibBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              cairo = cairoBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              pixman = xvfbPixmanBuild;
              libxcb = xcbBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              expat = expatBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              inherit (pkgs) xorgproto;
              libpng = libpngBuild;
            };
          i3Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/i3.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) i3;
              inherit (pkgs) xorgproto;
              startup-notification = startupNotificationBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libxcb-util = xcbUtilBuild;
              libxcb-keysyms = xcbKeysymsBuild;
              libxcb-wm = xcbWmBuild;
              libxcb-render-util = xcbRenderUtilBuild;
              libxcb-image = xcbImageBuild;
              libxcb-cursor = xcbCursorBuild;
              xcb-util-xrm = xcbXrmBuild;
              xkbcommon = xkbcommonBuild;
              yajl = yajlBuild;
              pcre2 = pcre2Build;
              cairo = cairoBuild;
              pango = pangoBuild;
              glib = glibBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              libev = libevBuild;
              libiconv = libiconvBuild;
              zlib = xvfbZlibBuild;
              libffi = libffiBuild;
              pixman = xvfbPixmanBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              expat = expatBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libpng = libpngBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
            };
          i3statusShimBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/i3status-shim.nix { };
          xvfbLibICEBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libICE";
              version = pkgs.libICE.version;
              src = pkgs.libICE.src;
              deps = [ pkgs.xorgproto pkgs.xtrans ];
              preConfigureExtra = ''
                export ac_cv_func_arc4random_buf=yes
              '';
            };
          xvfbLibSMBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libSM";
              version = pkgs.libSM.version;
              src = pkgs.libSM.src;
              deps = [ pkgs.xorgproto pkgs.xtrans xvfbLibICEBuild ];
              configureFlags = [
                "--without-libuuid"
              ];
            };
          xvfbLibXtBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXt";
              version = pkgs.libXt.version;
              src = pkgs.libXt.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibICEBuild
                xvfbLibSMBuild
              ];
            };
          xvfbLibXextBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXext";
              version = pkgs.libXext.version;
              src = pkgs.libXext.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXauBuild ];
            };
          xvfbLibXmuBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXmu";
              version = pkgs.libXmu.version;
              src = pkgs.libXmu.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXtBuild
                xvfbLibSMBuild
                xvfbLibICEBuild
              ];
            };
          xvfbLibXpmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXpm";
              version = pkgs.libXpm.version;
              src = pkgs.libXpm.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXawBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXaw";
              version = pkgs.libXaw.version;
              src = pkgs.libXaw.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXmuBuild
                xvfbLibXpmBuild
                xvfbLibXtBuild
                xvfbLibSMBuild
                xvfbLibICEBuild
              ];
              preConfigureExtra = ''
                export CFLAGS="$CFLAGS -include limits.h"
              '';
              postInstallExtra = ''
                ln -sf libXaw7.a $out/lib/libXaw.a
              '';
            };
          xvfbLibXkbfileBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxkbfile";
              version = pkgs.libxkbfile.version;
              src = pkgs.libxkbfile.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xkbcompBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-xkbcomp.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) xkbcomp xorgproto;
              libX11 = xlibBuild;
              libxkbfile = xvfbLibXkbfileBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libxcb = xcbBuild;
            };
          xvfbFontsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-fonts.nix { };
          xkeyboardConfigBuild =
            if isDarwin then null else pkgs.runCommand "puredarwin-xkeyboard-config" { } ''
              mkdir -p "$out/usr/share"
              cp -a ${pkgs.xkeyboard_config}/share/X11 "$out/usr/share/X11"
              chmod -R u+w "$out/usr/share/X11"
              cp -a ${pkgs.xkeyboard_config}/share/xkeyboard-config-2 "$out/usr/share/xkeyboard-config-2"
              chmod -R u+w "$out/usr/share/xkeyboard-config-2"
              if [ -L "$out/usr/share/X11/xkb" ]; then
                rm "$out/usr/share/X11/xkb"
                cp -a "$out/usr/share/xkeyboard-config-2" "$out/usr/share/X11/xkb"
                chmod -R u+w "$out/usr/share/X11/xkb"
              fi
            '';
          xlibLocaleBuild =
            if isDarwin then null else pkgs.runCommand "puredarwin-libx11-locale" { } ''
              mkdir -p "$out/usr/share/X11"
              cp -a ${pkgs.libX11}/share/X11/locale "$out/usr/share/X11/locale"
              chmod -R u+w "$out/usr/share/X11/locale"
            '';
          libzDylibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libz-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib;
            };
          libcurlDylibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libcurl-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              systemConfiguration = systemConfigurationBuild;
              zlib = xvfbZlibBuild;
              openssl = opensslBuild;
              inherit (pkgs) curl;
            };
          dbusBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/dbus.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              expat = expatBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              inherit (pkgs) dbus meson ninja python3 xorgproto;
            };
          libxml2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libxml2.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libxml2 meson ninja python3 git;
            };
          atspi2CoreBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/at-spi2-core.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glib = glibBuild;
              libxml2 = libxml2Build;
              dbus = dbusBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              inherit (pkgs) at-spi2-core meson ninja python3;
            };
          libwapcapletBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libwapcaplet.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libwapcaplet;
            };
          libparserutilsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libparserutils.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconv = libiconvBuild;
              inherit (pkgs) libparserutils perl;
            };
          libnsutilsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libnsutils.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsutils;
            };
          libnsgifBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libnsgif.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsgif;
            };
          libnsbmpBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libnsbmp.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsbmp;
            };
          libutf8procBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libutf8proc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libutf8proc;
            };
          libhubbubBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libhubbub.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              inherit (pkgs) libhubbub perl gperf gnused;
            };
          libcssBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libcss.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              inherit (pkgs) libcss perl python3;
            };
          libdomBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/libdom.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              libhubbub = libhubbubBuild;
              expat = expatBuild;
              inherit (pkgs) libdom;
            };
          netsurfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/netsurf.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              hostOtool = hostOtoolBuild;
              glibNative = pkgs.glib.dev;
              gdkPixbufNative = pkgs.gdk-pixbuf.dev;
              inherit (pkgs) inetutils;
              gtk3 = gtk3Build;
              glib = glibBuild;
              cairo = cairoBuild;
              cairoGobject = cairoGobjectBuild;
              pango = pangoBuild;
              gdkPixbuf = gdkPixbufBuild;
              libepoxy = libepoxyBuild;
              atspi2Core = atspi2CoreBuild;
              dbus = dbusBuild;
              libcurl = libcurlDylibBuild;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
              libpng = libpngBuild;
              libiconv = libiconvBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              libhubbub = libhubbubBuild;
              libcss = libcssBuild;
              libdom = libdomBuild;
              libnsgif = libnsgifBuild;
              libnsbmp = libnsbmpBuild;
              libnsutils = libnsutilsBuild;
              libutf8proc = libutf8procBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXcursor = xvfbLibXcursorBuild;
              xorgproto = pkgs.xorgproto;
              expat = expatBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              inherit (pkgs) perl pkg-config nsgenbind;
            };
          libepoxyBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/libepoxy.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libX11 = xlibBuild;
              mesa = mesaBuild;
              inherit (pkgs) libepoxy xorgproto meson ninja python3;
            };
          pdVirglShimBuild =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-pd-virgl-shim";
              src = userlandSource;
              buildTargets = [ "pd_virgl_shim" ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              installUserland = false;
              installKernel = false;
              prebuiltLibSystem = libSystemBuild;
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p $out/usr/lib $out/include
                ar=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar
                mkdir -p repack && ( cd repack && \
                  "$ar" x ../build-nix/src/Userspace/pd-virgl-shim/libpd_virgl_shim.a )
                ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
                  -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
                  -L${libSystemBuild}/usr/lib \
                  -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystemBuild}/usr/lib/system/libdyld.dylib \
                  -Wl,-install_name,/usr/lib/libpd_virgl_shim.dylib \
                  -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
                  repack/*.o -lSystem \
                  -o $out/usr/lib/libpd_virgl_shim.dylib
                cp src/Libraries/PDVirglShim/include/pd_virgl_shim.h $out/include/
                runHook postInstall
              '';
            });
          mesaBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/mesa.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              llvm = llvmCrossBuild;
              libxshmfence = libxshmfenceSharedBuild;
              zlib = xvfbZlibBuild;
              expat = expatBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXxf86vm = xvfbLibXxf86vmBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              pdVirglShim = pdVirglShimBuild;
              virglWinsysSrc = ./nix/pkgs/mesa/virgl-puredarwin;
              virglAbiHeader = ./src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h;
              inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
            };
          gluBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/glu.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              mesa = mesaBuild;
              inherit (pkgs) meson ninja pkg-config;
            };
          gluNoxBuild =
            if isDarwin then null else gluBuild.override { mesa = mesaNoxBuild; };
          mesaDemosBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/mesa-demos.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              mesa = mesaBuild;
              openglFramework = openglFrameworkBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              inherit (pkgs) meson ninja pkg-config xorgproto xtrans;
            };
          hostOtoolBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/host-otool.nix { };
          # aarch64 Windows PE toolchain (llvm-mingw equivalent) for Wine's
          # new WoW64 on arm64. See the file for the two nixpkgs bugs it works around.
          fexWow64Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/fex-wow64.nix {
              mingwAarch64Cc = mingwAarch64.cc;
              mingwAarch64Pthreads = mingwAarch64.pthreads;
            };

          mingwAarch64 =
            if isDarwin then null else import ./nix/pkgs/toolchain/mingw-aarch64.nix {
              inherit pkgs system;
              # 21 hits the AArch64 SEH unwind backend bug on Wine's concrt140.
              llvmVersion = "22";
            };

          compilerRtBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/compiler-rt.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
            };
          llvmCrossBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/llvm-cross.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
              nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
              nativeLlvmConfig = "${pkgs.llvmPackages_21.llvm.dev}/bin/llvm-config";
            };
          # WebKitGTK's mandatory dependencies (Source/cmake/OptionsGTK.cmake).
          # Independently useful: sqlite3, libjpeg and libsoup have no other
          # provider in this tree.
          libgpgErrorBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libgpg-error";
              inherit (pkgs.libgpg-error) version src;
              # mkheader looks for syscfg/lock-obj-pub.<host_os>.h, i.e.
              # darwin20.4, while the tree ships the generic
              # x86_64-apple-darwin one. Same contents, different name.
              postPatchExtra = ''
                cp src/syscfg/lock-obj-pub.x86_64-apple-darwin.h \
                   src/syscfg/lock-obj-pub.darwin20.4.h
              '';
              # sysutils.c calls mkdir() without including <sys/stat.h>, which
              # newer clang makes a hard error rather than an implicit decl.
              preConfigureExtra = ''
                export CFLAGS="$CFLAGS -include sys/stat.h"
              '';
              configureFlags = [
                "--disable-nls"
                "--disable-doc"
                "--disable-tests"
                "--disable-languages"
              ];
            };
          libtasn1Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libtasn1";
              inherit (pkgs.libtasn1) version src;
              configureFlags = [ "--disable-doc" "--disable-gtk-doc" ];
            };
          sqliteBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-sqlite";
              inherit (pkgs.sqlite) version;
              # nixpkgs' sqlite src is a .zip the default unpacker cannot read.
              src = pkgs.runCommand "sqlite-src-${pkgs.sqlite.version}" { nativeBuildInputs = [ pkgs.unzip ]; } ''
                unzip -q ${pkgs.sqlite.src}
                mkdir -p $out
                cp -R sqlite-src-*/. $out/
                chmod -R u+w $out
              '';
              configureFlags = [ "--disable-readline" "--disable-editline" ];
              deps = [ xvfbZlibBuild ];
            };
          webkitgtkBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/webkitgtk.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              glibNative = pkgs.glib.dev;
              libxml2Native = pkgs.libxml2;
              waylandScanner = waylandScannerBuild;
              inherit (pkgs) cmake ninja pkg-config python3 perl ruby gperf unifdef;
              webkitgtk = pkgs.webkitgtk_6_0;
              icu = icuCoreBuild;
              mesa = mesaBuild;
              deps = [
                glibBuild pcre2Build libffiBuild xvfbZlibBuild libiconvBuild
                cairoBuild cairoGobjectBuild xvfbPixmanBuild
                pangoBuild fribidiBuild harfbuzzBuild freetype2Build
                fontconfigBuild expatBuild
                gdkPixbufBuild libepoxyBuild atspi2CoreBuild dbusBuild libpngBuild
                gtk3Build icuCoreBuild libxml2Build
                libsoupBuild sqliteBuild libpslBuild nghttp2Build
                libgcryptBuild libgpgErrorBuild libtasn1Build libjpegBuild libwebpBuild
                waylandBuild waylandProtocolsBuild xkbcommonBuild mesaBuild
                xlibBuild xcbBuild xvfbLibXauBuild xvfbLibXdmcpBuild
                xvfbLibXextBuild xvfbLibXrenderBuild xvfbLibXfixesBuild
                xvfbLibXcompositeBuild xvfbLibXdamageBuild
                pkgs.xorgproto
              ];
            };
          libsoupBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libsoup.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              glibNative = pkgs.glib.dev;
              inherit (pkgs) meson ninja pkg-config python3;
              libsoup = pkgs.libsoup_3;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              sqlite = sqliteBuild;
              libpsl = libpslBuild;
              nghttp2 = nghttp2Build;
              libxml2 = libxml2Build;
            };
          nghttp2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-nghttp2";
              inherit (pkgs.nghttp2) version src;
              # Only libnghttp2 is wanted; the apps are C++ and pull in
              # libev/openssl/jansson that nothing here needs.
              configureFlags = [ "--enable-lib-only" "--disable-python-bindings" ];
            };
          libpslBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libpsl";
              inherit (pkgs.libpsl) version;
              # Upstream ships a .tar.lz, which the default unpacker cannot read.
              src = pkgs.runCommand "libpsl-src-${pkgs.libpsl.version}" { nativeBuildInputs = [ pkgs.lzip ]; } ''
                lzip -dc ${pkgs.libpsl.src} | tar -x
                mkdir -p $out
                cp -R libpsl-*/. $out/
                chmod -R u+w $out
              '';
              # No libidn2/libunistring here, so IDNA is the builtin variant.
              # python is a build-time tool: it generates the suffix tables.
              nativeDeps = [ pkgs.python3 ];
              configureFlags = [ "--disable-runtime" "--disable-builtin" "--disable-man" ];
            };
          libwebpBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libwebp.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libwebp cmake ninja;
            };
          libjpegBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libjpeg-turbo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libjpeg_turbo cmake ninja;
            };
          libgcryptBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libgcrypt";
              inherit (pkgs.libgcrypt) version src;
              deps = [ libgpgErrorBuild ];
              configureFlags = [
                "--disable-doc"
                "--disable-tests"
                "--disable-asm"
                "--with-libgpg-error-prefix=${libgpgErrorBuild}"
              ];
            };
          kcToolsGuestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/kc-tools-guest.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              kcToolsSrc = kc-tools;
              inherit (pkgs) cmake ninja;
            };
          clangCrossBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/clang-cross.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              llvm = llvmCrossBuild;
              llvmSrc = pkgs.llvmPackages_21.libllvm.monorepoSrc;
              llvmVersion = pkgs.llvmPackages_21.llvm.version;
              nativeTblgen = "${pkgs.llvmPackages_21.llvm}/bin/llvm-tblgen";
            };
          nativeMesonToolsDir =
            if isDarwin then null else pkgs.runCommand "puredarwin-native-meson-tools" { } ''
              mkdir -p $out/bin
              ln -s ${hostOtoolBuild}/bin/otool $out/bin/otool
              ln -s ${hostOtoolBuild}/bin/install_name_tool $out/bin/install_name_tool
            '';
          libpngBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/libpng.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              zlib = xvfbZlibBuild;
              inherit (pkgs) libpng;
            };
          libcrocoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              guestPrefix = true;
              shared = true;
              pname = "puredarwin-libcroco";
              version = "0.6.13";
              src = pkgs.fetchurl {
                url = "https://download.gnome.org/sources/libcroco/0.6/libcroco-0.6.13.tar.xz";
                hash = "sha256-dn7CNK56poRpWzpzVUgiSIgTLgY/kttYV1m0IlcGIdQ=";
              };
              deps = [ glibBuild libxml2Build pcre2Build libffiBuild libiconvBuild xvfbZlibBuild ];
              configureFlags = [ "--disable-Werror" "--disable-Bsymbolic" ];
            };

          # Only gettext-runtime is built: it is what produces libintl, and the
          # gettext-tools half is a build-host toolchain (msgfmt/xgettext) that
          # nothing on the guest needs.
          gettextBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              guestPrefix = true;
              shared = true;
              pname = "puredarwin-gettext";
              version = pkgs.gettext.version;
              src = pkgs.gettext.src;
              deps = [ libiconvBuild ];
              preConfigureExtra = ''
                cd gettext-runtime
              '';
              configureFlags = [
                "--with-included-libintl"
                "--with-libiconv-prefix=${libiconvBuild}"
                "--disable-java"
                "--disable-csharp"
                "--disable-libasprintf"
                "--disable-rpath"
                "--disable-dependency-tracking"
              ];
            };

          librsvgBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              guestPrefix = true;
              shared = true;
              pname = "puredarwin-librsvg";
              version = "2.40.21";
              src = pkgs.fetchurl {
                url = "https://download.gnome.org/sources/librsvg/2.40/librsvg-2.40.21.tar.xz";
                hash = "sha256-92KJBfHK2oTofisUiD7VfYCU3KMoHVvLJOzkJ56akro=";
              };
              deps = [
                glibBuild gdkPixbufBuild cairoBuild cairoGobjectBuild pangoBuild
                libxml2Build libcrocoBuild libpngBuild freetype2Build fontconfigBuild
                fribidiBuild harfbuzzBuild expatBuild pcre2Build libffiBuild
                libiconvBuild xvfbZlibBuild xvfbPixmanBuild
              ];
              preConfigureExtra = ''
                # libxml2 installs under include/libxml2/libxml, and the deps
                # mapping only contributes the include/ level.
                export CFLAGS="$CFLAGS -I${libxml2Build}/include/libxml2"
                export CFLAGS="$CFLAGS -include libxml/parser.h"
                export CFLAGS="$CFLAGS -Wno-incompatible-function-pointer-types"
                export ac_cv_path_GDK_PIXBUF_QUERYLOADERS="$(command -v true)"
              '';
              configureFlags = [
                "--disable-introspection"
                "--disable-tools"
                "--enable-pixbuf-loader"
                "--disable-Bsymbolic"
              ];
              postInstallExtra = ''
                nested=$(find "$out/nix" -type d -name loaders 2>/dev/null | head -1)
                if [ -n "$nested" ]; then
                  mkdir -p "$out/lib/gdk-pixbuf-2.0/2.10.0/loaders"
                  cp -a "$nested"/. "$out/lib/gdk-pixbuf-2.0/2.10.0/loaders/"
                  rm -rf "$out/nix"
                fi
                for so in "$out"/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.so; do
                  [ -e "$so" ] || continue
                  ${nativeMesonToolsDir}/bin/install_name_tool \
                    -change //lib/librsvg-2.2.dylib /lib/librsvg-2.2.dylib "$so"
                done
              '';
            };

          cairoGobjectBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/cairo-gobject.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              cairo = cairoBuild;
              cairoReal = pkgs.cairo;
              glib = glibBuild;
            };
          gdkPixbufBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/gdk-pixbuf.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              libpng = libpngBuild;
              inherit (pkgs) gdk-pixbuf meson ninja python3;
            };
          gtk3Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/gtk3.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              cairo = cairoBuild;
              cairoGobject = cairoGobjectBuild;
              pixman = xvfbPixmanBuild;
              pango = pangoBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              gdkPixbuf = gdkPixbufBuild;
              libepoxy = libepoxyBuild;
              atspi2Core = atspi2CoreBuild;
              dbus = dbusBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXcursor = xvfbLibXcursorBuild;
              libpng = libpngBuild;
              glibNative = pkgs.glib.dev;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonBuild;
              mesa = mesaBuild;
              inherit (pkgs) gtk3 xorgproto;
            };
          onyx2dBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/onyx2d.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              libpng = libpngBuild;
              libjpeg = libjpegBuild;
              zlib = xvfbZlibBuild;
              src = ./src/Frameworks/Onyx2D;
            };
          coregraphicsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/coregraphics.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              onyx2d = onyx2dBuild;
              freetype2 = freetype2Build;
              libpng = libpngBuild;
              libjpeg = libjpegBuild;
              zlib = xvfbZlibBuild;
              src = ./src/Frameworks/CoreGraphics;
            };
          coretextBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/coretext.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              onyx2d = onyx2dBuild;
              coregraphics = coregraphicsBuild;
              src = ./src/Frameworks/CoreText;
            };
          coredataBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/coredata.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              src = ./src/Frameworks/CoreData;
            };
          cgScreenDemoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/cg-screen-demo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              pdsurface = pdsurfaceBuild;
              coregraphics = coregraphicsBuild;
              src = ./src/Userspace/cg-screen-demo;
            };
          gershwinSystemBuild = pkgs.stdenvNoCC.mkDerivation {
            pname = "gershwin-system";
            version = "2026-09-06";
            src = pkgs.fetchFromGitHub {
              owner = "gershwin-desktop";
              repo = "gershwin-system";
              rev = "a29ac743fea4388d580467e1998d7bf4c06b1de2";
              hash = "sha256-T/1iGevtrkIfoUVDu645UTujd+lhUxOuGiYfCYlaCYw=";
            };
            installPhase = ''
              runHook preInstall
              mkdir -p "$out/System/Library"
              cp -R Library/. "$out/System/Library/"
              runHook postInstall
            '';
            meta = {
              description = "Gershwin system-domain configuration and scripts";
              homepage = "https://github.com/gershwin-desktop/gershwin-system";
              license = lib.licenses.bsd2;
              platforms = lib.platforms.unix;
            };
          };
          gershwinAssetsBuild = pkgs.stdenvNoCC.mkDerivation {
            pname = "gershwin-assets";
            version = "2026-09-06";
            src = pkgs.fetchFromGitHub {
              owner = "gershwin-desktop";
              repo = "gershwin-assets";
              rev = "9266b6edd28ce7fb6d9e6dfcf9cca4cc4b1c3038";
              hash = "sha256-Scc33jaG/lJqEjunU9MGKGIM47YgRACoipsWsnPe2U8=";
            };
            installPhase = ''
              runHook preInstall
              mkdir -p "$out/System/Library"
              cp -R Library/. "$out/System/Library/"
              runHook postInstall
            '';
            meta = {
              description = "Gershwin system-domain artwork and resources";
              homepage = "https://github.com/gershwin-desktop/gershwin-assets";
              license = lib.licenses.bsd2;
              platforms = lib.platforms.unix;
            };
          };
          # Wayland-only image: cairo/dbus/mesa each bake an libX11 path into
          # their output unless their X11 backends are configured out.
          cairoNoxBuild =
            if isDarwin then null else cairoBuild.override {
              withX11 = false;
              xorgproto = null; libX11 = null; libXext = null;
              libXrender = null; libxcb = null; libXau = null; libXdmcp = null;
            };
          dbusNoxBuild =
            if isDarwin then null else dbusBuild.override {
              withX11 = false;
              libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
              xorgproto = null;
            };
          mesaNoxBuild =
            if isDarwin then null else mesaBuild.override {
              withX11 = false;
              libX11 = null; libXext = null; libxcb = null; libXau = null;
              libXdmcp = null; libXxf86vm = null; xorgproto = null; xtrans = null;
            };
          openglFrameworkNoxBuild =
            if isDarwin then null else openglFrameworkBuild.override {
              withX11 = false;
              mesa = mesaNoxBuild;
              glu = gluNoxBuild;
              libX11 = null; xorgproto = null; libXext = null;
              libxcb = null; libXau = null; libXdmcp = null;
            };
          mesaDemosNoxBuild =
            if isDarwin then null else mesaDemosBuild.override {
              withX11 = false;
              mesa = mesaNoxBuild;
              wayland = waylandBuild;
              xkbcommon = xkbcommonNoxBuild;
              waylandScanner = waylandScannerBuild;
              waylandProtocols = waylandProtocolsBuild;
              libX11 = null; libXext = null; libxcb = null; libXau = null;
              libXdmcp = null; xorgproto = null; xtrans = null;
            };
          # librsvg takes an explicit deps list rather than a cairo argument, so
          # the nox cairo has to be substituted into it by hand.
          librsvgNoxBuild =
            if isDarwin then null else librsvgBuild.override {
              deps = [
                glibBuild gdkPixbufBuild cairoNoxBuild cairoGobjectNoxBuild pangoNoxBuild
                libxml2Build libcrocoBuild libpngBuild freetype2Build fontconfigBuild
                fribidiBuild harfbuzzNoxBuild expatBuild pcre2Build libffiBuild
                libiconvBuild xvfbZlibBuild xvfbPixmanBuild
              ];
            };
          netsurfNoxBuild =
            if isDarwin then null else netsurfBuild.override {
              withX11 = false;
              gtk3 = gtk3NoxBuild;
              cairo = cairoNoxBuild;
              cairoGobject = cairoGobjectNoxBuild;
              pango = pangoNoxBuild;
              harfbuzz = harfbuzzNoxBuild;
              libepoxy = libepoxyNoxBuild;
              dbus = dbusNoxBuild;
              atspi2Core = atspi2CoreNoxBuild;
              libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
              libXext = null; libXi = null; libXrender = null; libXrandr = null;
              libXfixes = null; libXcursor = null; xorgproto = null;
            };
          libepoxyNoxBuild =
            if isDarwin then null else libepoxyBuild.override {
              withX11 = false;
              mesa = mesaNoxBuild;
              libX11 = null; xorgproto = null;
            };
          fastfetchNoxBuild =
            if isDarwin then null else fastfetchBuild.override {
              withX11 = false;
              mesa = mesaNoxBuild;
              glu = gluNoxBuild;
              openglFramework = openglFrameworkNoxBuild;
              libX11 = null; libXext = null; libxcb = null;
              libXau = null; libXdmcp = null;
            };
          harfbuzzNoxBuild =
            if isDarwin then null else harfbuzzBuild.override { cairo = cairoNoxBuild; };
          atspi2CoreNoxBuild =
            if isDarwin then null else atspi2CoreBuild.override { dbus = dbusNoxBuild; };
          cairoGobjectNoxBuild =
            if isDarwin then null else cairoGobjectBuild.override { cairo = cairoNoxBuild; };
          xkbcommonNoxBuild =
            if isDarwin then null else xkbcommonBuild.override {
              withX11 = false;
              libxcb = null; libXau = null; libXdmcp = null;
            };
          pangoNoxBuild =
            if isDarwin then null else pangoBuild.override {
              withX11 = false;
              cairo = cairoNoxBuild;
              harfbuzz = harfbuzzNoxBuild;
              libX11 = null; libxcb = null; libXext = null; libXrender = null;
              xorgproto = null;
            };
          gtk3NoxBuild =
            if isDarwin then null else gtk3Build.override {
              withX11 = false;
              cairo = cairoNoxBuild;
              cairoGobject = cairoGobjectNoxBuild;
              pango = pangoNoxBuild;
              harfbuzz = harfbuzzNoxBuild;
              libepoxy = libepoxyNoxBuild;
              mesa = mesaNoxBuild;
              dbus = dbusNoxBuild;
              atspi2Core = atspi2CoreNoxBuild;
              xkbcommon = xkbcommonNoxBuild;
              libX11 = null;
              libxcb = null;
              libXau = null;
              libXdmcp = null;
              libXext = null;
              libXi = null;
              libXrender = null;
              libXrandr = null;
              libXfixes = null;
              libXcursor = null;
              xorgproto = null;
            };
          gtkLayerShellNoxBuild =
            if isDarwin then null else gtkLayerShellBuild.override {
              gtk3 = gtk3NoxBuild;
              withX11 = false;
              atspi2Core = atspi2CoreNoxBuild;
              xkbcommon = xkbcommonNoxBuild;
              cairo = cairoNoxBuild;
              cairoGobject = cairoGobjectNoxBuild;
              pango = pangoNoxBuild;
              harfbuzz = harfbuzzNoxBuild;
              libepoxy = libepoxyNoxBuild;
              dbus = dbusNoxBuild;
              mesa = mesaNoxBuild;
              libX11 = null; libxcb = null; libXau = null; libXdmcp = null;
              libXext = null; libXi = null; libXrender = null; libXrandr = null;
              libXfixes = null; libXcursor = null; xorgproto = null;
            };
          libwnckBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xfce/libwnck.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              cairo = cairoBuild;
              cairoGobject = cairoGobjectBuild;
              pixman = xvfbPixmanBuild;
              pango = pangoBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              gdkPixbuf = gdkPixbufBuild;
              libepoxy = libepoxyBuild;
              atspi2Core = atspi2CoreBuild;
              dbus = dbusBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXcursor = xvfbLibXcursorBuild;
              libXres = xvfbLibXresBuild;
              libpng = libpngBuild;
              glibNative = pkgs.glib.dev;
              gtk3 = gtk3Build;
              startupNotification = startupNotificationBuild;
              inherit (pkgs) libwnck xorgproto;
            };
          gtkLayerShellBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/gtk-layer-shell.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              cairo = cairoBuild;
              cairoGobject = cairoGobjectBuild;
              pixman = xvfbPixmanBuild;
              pango = pangoBuild;
              fribidi = fribidiBuild;
              harfbuzz = harfbuzzBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              gdkPixbuf = gdkPixbufBuild;
              libepoxy = libepoxyBuild;
              atspi2Core = atspi2CoreBuild;
              dbus = dbusBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXcursor = xvfbLibXcursorBuild;
              libpng = libpngBuild;
              glibNative = pkgs.glib.dev;
              gtk3 = gtk3Build;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonBuild;
              mesa = mesaBuild;
              inherit (pkgs) gtk-layer-shell xorgproto;
            };
          xvfbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanBuild;
              libXau = xvfbLibXauBuild;
              libXfont2 = xvfbLibXfont2Build;
              zlib = xvfbZlibBuild;
              freetype2 = freetype2Build;
              libfontenc = libfontencBuild;
              xvfbZlib = xvfbZlibBuild;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileBuild;
              libXdmcp = pkgs.libxdmcp;
            };
          xvfbLibxcvtBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xvfb-libxcvt.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libxcvt;
            };
          xorgBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanBuild;
              libXau = xvfbLibXauBuild;
              libXfont2 = xvfbLibXfont2Build;
              zlib = xvfbZlibBuild;
              freetype2 = freetype2Build;
              libfontenc = libfontencBuild;
              xvfbZlib = xvfbZlibBuild;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileBuild;
              libXdmcp = pkgs.libxdmcp;
              libxcvt = xvfbLibxcvtBuild;
              mesa = mesaBuild;
              mesaGlHeaders = pkgs.mesa-gl-headers;
              glHeaders = pkgs.libglvnd.dev;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXext = xvfbLibXextBuild;
              libXfixes = xvfbLibXfixesBuild;
            };
          pdsurfaceBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/pdsurface.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
            };
          libgbmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/libgbm.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pdsurface = pdsurfaceBuild;
            };
          libdrmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/libdrm.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              puredarwinSource = ./src/Libraries/libdrm;
              src = pkgs.libdrm.src;
              inherit (pkgs) meson ninja pkg-config python3;
            };
          jsoncBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/json-c.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              src = pkgs.json_c.src;
              inherit (pkgs) cmake ninja pkg-config;
            };
          xwaylandBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xwayland.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xwayland = pkgs.xwayland;
              pixman = xvfbPixmanBuild;
              xorgproto = pkgs.xorgproto;
              xtrans = pkgs.xtrans;
              xlib = xlibBuild;
              xcb = xcbBuild;
              libXfont2 = xvfbLibXfont2Build;
              libxkbfile = xvfbLibXkbfileBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXres = xvfbLibXresBuild;
              libXcomposite = xvfbLibXcompositeBuild;
              libXdamage = xvfbLibXdamageBuild;
              libxshmfence = libxshmfenceSharedBuild;
              zlib = xvfbZlibBuild;
              freetype2 = freetype2Build;
              libfontenc = libfontencBuild;
              xvfbZlib = xvfbZlibBuild;
              libxcvt = xvfbLibxcvtBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonBuild;
              xkbcomp = xkbcompBuild;
              xkeyboardConfig = xkeyboardConfigBuild;
              openssl = opensslBuild;
              mesaGlHeaders = pkgs.mesa-gl-headers;
              mesa = mesaBuild;
              libepoxy = libepoxyBuild;
              libdrm = libdrmBuild;
            };
          xvfbLibXrenderBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXrender";
              version = pkgs.libXrender.version;
              src = pkgs.libXrender.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          # libGL's direct-rendering path calls into XF86VidMode for refresh
          # rate reporting, so glx-direct=true needs this.
          xvfbLibXxf86vmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXxf86vm";
              version = pkgs.libXxf86vm.version;
              src = pkgs.libXxf86vm.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXextBuild ];
            };
          xvfbLibXfixesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXfixes";
              version = pkgs.libXfixes.version;
              src = pkgs.libXfixes.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXcursorBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXcursor";
              version = pkgs.libXcursor.version;
              src = pkgs.libXcursor.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXfixesBuild xvfbLibXrenderBuild ];
              postInstallExtra = ''
                mkdir -p .libXcursor-dylib
                (
                  cd .libXcursor-dylib
                  ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar x "$out/lib/libXcursor.a"
                  ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
                    -isysroot "$DARWIN_SDK_ROOT" \
                    -mmacosx-version-min=26.5 \
                    -fuse-ld=${nativeLd}/bin/ld \
                    -nostdlib \
                    -dynamiclib \
                    -Wl,-install_name,/lib/libXcursor.1.dylib \
                    -Wl,-compatibility_version,1.0.0 \
                    -Wl,-current_version,1.0.2 \
                    -Wl,-undefined,dynamic_lookup \
                    -L${libSystemBuild}/usr/lib \
                    -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystemBuild}/usr/lib/system/libdyld.dylib \
                    -o "$out/lib/libXcursor.1.dylib" \
                    ./*.o \
                    -lSystem
                )
                ln -sf libXcursor.1.dylib "$out/lib/libXcursor.dylib"
              '';
            };
          xvfbLibXrandrBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXrandr";
              version = pkgs.libXrandr.version;
              src = pkgs.libXrandr.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXrenderBuild xvfbLibXextBuild ];
            };
          # XFCE desktop (see nix/xfce.nix).
          xfceDesktop = import ./nix/xfce.nix {
            inherit lib;
            inherit atspi2CoreBuild;
            inherit cairoBuild;
            inherit cairoGobjectBuild;
            inherit darwinCrossToolchain;
            inherit dbusBuild;
            inherit exoSrc;
            inherit expatBuild;
            inherit fontconfigBuild;
            inherit freetype2Build;
            inherit fribidiBuild;
            inherit garconSrc;
            inherit gdkPixbufBuild;
            inherit glibBuild;
            inherit gtk3Build;
            inherit gtkLayerShellBuild;
            inherit mesaBuild;
            inherit waylandBuild;
            inherit waylandProtocolsBuild;
            inherit waylandScannerBuild;
            inherit xkbcommonBuild;
            inherit harfbuzzBuild;
            inherit isDarwin;
            inherit libSystemBuild;
            inherit libdisplayInfoBuild;
            inherit libepoxyBuild;
            inherit libffiBuild;
            inherit libiconvBuild;
            inherit libpngBuild;
            inherit libwnckBuild;
            inherit libxfce4uiSrc;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            inherit pangoBuild;
            inherit pcre2Build;
            inherit pkgs;
            inherit startupNotificationBuild;
            inherit thunarSrc;
            inherit vteBuild;
            inherit xcbBuild;
            inherit xfce4AppfinderSrc;
            inherit xfce4PanelSrc;
            inherit xfce4SessionSrc;
            inherit xfce4SettingsSrc;
            inherit xfce4TerminalSrc;
            inherit xfconfSrc;
            inherit xfdesktopSrc;
            inherit xfwm4Src;
            inherit xlibBuild;
            inherit xvfbLibICEBuild;
            inherit xvfbLibSMBuild;
            inherit xvfbLibXauBuild;
            inherit xvfbLibXcompositeBuild;
            inherit xvfbLibXcursorBuild;
            inherit xvfbLibXdamageBuild;
            inherit xvfbLibXdmcpBuild;
            inherit xvfbLibXextBuild;
            inherit xvfbLibXfixesBuild;
            inherit xvfbLibXiBuild;
            inherit xvfbLibXineramaBuild;
            inherit xvfbLibXpresentBuild;
            inherit xvfbLibXrandrBuild;
            inherit xvfbLibXrenderBuild;
            inherit xvfbLibXresBuild;
            inherit xvfbPixmanBuild;
            inherit xvfbZlibBuild;
          };
          xfceDesktopArm64 = import ./nix/xfce.nix {
            inherit lib;
            targetTriple = "arm64-apple-darwin20.4";
            atspi2CoreBuild = atspi2CoreArm64Build;
            cairoBuild = cairoArm64Build;
            cairoGobjectBuild = cairoGobjectArm64Build;
            darwinCrossToolchain = arm64CrossToolchain;
            dbusBuild = dbusArm64Build;
            inherit exoSrc;
            expatBuild = expatArm64Build;
            fontconfigBuild = fontconfigArm64Build;
            freetype2Build = freetype2Arm64Build;
            fribidiBuild = fribidiArm64Build;
            inherit garconSrc;
            gdkPixbufBuild = gdkPixbufArm64Build;
            glibBuild = glibArm64Build;
            gtk3Build = gtk3Arm64Build;
            gtkLayerShellBuild = gtkLayerShellArm64Build;
            mesaBuild = mesaArm64Build;
            waylandBuild = waylandArm64Build;
            inherit waylandProtocolsBuild;
            inherit waylandScannerBuild;
            xkbcommonBuild = xkbcommonArm64Build;
            harfbuzzBuild = harfbuzzArm64Build;
            inherit isDarwin;
            libSystemBuild = libSystemArm64Build;
            libdisplayInfoBuild = libdisplayInfoArm64Build;
            libepoxyBuild = libepoxyArm64Build;
            libffiBuild = libffiArm64Build;
            libiconvBuild = libiconvArm64Build;
            libpngBuild = libpngArm64Build;
            libwnckBuild = libwnckArm64Build;
            inherit libxfce4uiSrc;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            pangoBuild = pangoArm64Build;
            pcre2Build = pcre2Arm64Build;
            inherit pkgs;
            startupNotificationBuild = startupNotificationArm64Build;
            inherit thunarSrc;
            vteBuild = vteArm64Build;
            xcbBuild = xcbArm64Build;
            inherit xfce4AppfinderSrc;
            inherit xfce4PanelSrc;
            inherit xfce4SessionSrc;
            inherit xfce4SettingsSrc;
            inherit xfce4TerminalSrc;
            inherit xfconfSrc;
            inherit xfdesktopSrc;
            inherit xfwm4Src;
            xlibBuild = xlibArm64Build;
            xvfbLibICEBuild = xvfbLibICEArm64Build;
            xvfbLibSMBuild = xvfbLibSMArm64Build;
            xvfbLibXauBuild = xvfbLibXauArm64Build;
            xvfbLibXcompositeBuild = xvfbLibXcompositeArm64Build;
            xvfbLibXcursorBuild = xvfbLibXcursorArm64Build;
            xvfbLibXdamageBuild = xvfbLibXdamageArm64Build;
            xvfbLibXdmcpBuild = xvfbLibXdmcpArm64Build;
            xvfbLibXextBuild = xvfbLibXextArm64Build;
            xvfbLibXfixesBuild = xvfbLibXfixesArm64Build;
            xvfbLibXiBuild = xvfbLibXiArm64Build;
            xvfbLibXineramaBuild = xvfbLibXineramaArm64Build;
            xvfbLibXpresentBuild = xvfbLibXpresentArm64Build;
            xvfbLibXrandrBuild = xvfbLibXrandrArm64Build;
            xvfbLibXrenderBuild = xvfbLibXrenderArm64Build;
            xvfbLibXresBuild = xvfbLibXresArm64Build;
            xvfbPixmanBuild = xvfbPixmanArm64Build;
            xvfbZlibBuild = xvfbZlibArm64Build;
          };          xfconfArm64 = xfceDesktopArm64.xfconfBuild;
          libxfce4utilArm64 = xfceDesktopArm64.libxfce4utilBuild;
          libxfce4uiArm64 = xfceDesktopArm64.libxfce4uiBuild;
          xfwm4Arm64 = xfceDesktopArm64.xfwm4Build;
          libxfce4windowingArm64 = xfceDesktopArm64.libxfce4windowingBuild;
          garconArm64 = xfceDesktopArm64.garconBuild;
          exoArm64 = xfceDesktopArm64.exoBuild;
          xfce4SessionArm64 = xfceDesktopArm64.xfce4SessionBuild;
          xfce4PanelArm64 = xfceDesktopArm64.xfce4PanelBuild;
          xfdesktopArm64 = xfceDesktopArm64.xfdesktopBuild;
          xfce4TerminalArm64 = xfceDesktopArm64.xfce4TerminalBuild;
          xfce4SettingsArm64 = xfceDesktopArm64.xfce4SettingsBuild;
          xfce4AppfinderArm64 = xfceDesktopArm64.xfce4AppfinderBuild;
          thunarArm64 = xfceDesktopArm64.thunarBuild;
          inherit (xfceDesktop)
            xfconfBuild
            libxfce4utilBuild
            libxfce4uiBuild
            xfwm4Build
            libxfce4windowingBuild
            garconBuild
            exoBuild
            xfce4SessionBuild
            xfce4PanelBuild
            xfdesktopBuild
            xfce4TerminalBuild
            xfce4SettingsBuild
            xfce4AppfinderBuild
            thunarBuild
            ;
          cursorThemeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/cursor-theme.nix {
              inherit (pkgs) vanilla-dmz;
            };
          iconThemesBuild =
            if isDarwin then null else pkgs.runCommand "puredarwin-icon-themes" { } ''
              mkdir -p "$out/share/icons"
              cp -a ${pkgs.hicolor-icon-theme}/share/icons/hicolor "$out/share/icons/"
              cp -a ${pkgs.adwaita-icon-theme}/share/icons/Adwaita "$out/share/icons/"
              chmod -R u+w "$out/share/icons"
            '';
          libdisplayInfoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/libdisplay-info.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libdisplay-info hwdata;
            };
          glibNetworkingBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/glib-networking.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              glibNetworking = pkgs.glib-networking;
              glib = glibBuild;
              gnutls = gnutlsSharedBuild;
            };

          vteBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk/vte.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              version = "0.70.6";
              src = vteSrc;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              zlib = xvfbZlibBuild;
              libiconv = libiconvBuild;
              cairo = cairoBuild;
              cairoGobject = cairoGobjectBuild;
              pixman = xvfbPixmanBuild;
              pango = pangoBuild;
              fribidi = fribidiBuild;
              gnutls = gnutlsSharedBuild;
              harfbuzz = harfbuzzBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              gdkPixbuf = gdkPixbufBuild;
              libepoxy = libepoxyBuild;
              atspi2Core = atspi2CoreBuild;
              dbus = dbusBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXrandr = xvfbLibXrandrBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXcursor = xvfbLibXcursorBuild;
              libpng = libpngBuild;
              glibNative = pkgs.glib.dev;
              gtk3 = gtk3Build;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              inherit (pkgs) xorgproto;
            };
          xrandrBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-xrandr";
              version = "1.5.4";
              src = pkgs.fetchurl {
                url = "https://www.x.org/releases/individual/app/xrandr-1.5.4.tar.xz";
                hash = "sha256-LK/MsqrySRpAaGdhF6DU+QqzB3JLlv/8VM0dqVN3lAA=";
              };
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXrandrBuild
                xvfbLibXrenderBuild
                xvfbLibXextBuild
                xcbBuild
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              preConfigureExtra = ''
                export LIBS="-lXrandr -lXrender -lXext -lX11 -lxcb -lXau -lXdmcp $LIBS"
              '';
            };
          xrdbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-xrdb";
              version = "1.2.3";
              # release tarball: nixpkgs' src is a git checkout with no configure
              src = pkgs.fetchurl {
                url = "https://www.x.org/releases/individual/app/xrdb-1.2.3.tar.xz";
                sha256 = "sha256-yI9WAkMnjIls5PySrlpForUFoxb/pCf+VbAuXVkUxOQ=";
              };
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXmuBuild
                xvfbLibXtBuild
                xvfbLibXextBuild
                xvfbLibSMBuild
                xvfbLibICEBuild
                xcbBuild
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              preConfigureExtra = ''
                export LIBS="-lXmu -lXt -lXext -lX11 -lxcb -lXau -lXdmcp -lSM -lICE $LIBS"
              '';
              configureFlags = [
                "--with-cpp=/usr/bin/cpp,/bin/cpp"
              ];
            };
          xinitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              guestPrefix = true;
              pname = "puredarwin-xinit";
              version = "1.4.4";
              src = pkgs.fetchurl {
                url = "https://www.x.org/releases/individual/app/xinit-1.4.4.tar.xz";
                sha256 = "sha256-QKR8ehZMf5gc43h7Szf35BH7QyMdzeVD1wCUB12s/vk=";
              };
              deps = [
                pkgs.xorgproto
                xlibBuild
                xcbBuild
                xvfbLibXauBuild
                xvfbLibXdmcpBuild
              ];
              preConfigureExtra = ''
                export LIBS="-lX11 -lxcb -lXau -lXdmcp $LIBS"
              '';
              configureFlags = [
                "--with-xserver=/usr/bin/Xorg"
                # launchd support here is the macOS org.x.startx plist machinery,
                # which is unrelated to how PureDarwin starts X.
                "--without-launchd"
              ];
              postInstallExtra = ''
                rm -f "$out/bin/startx"
              '';
            };
          iceauthBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-iceauth";
              version = "1.0.11";
              src = pkgs.fetchurl {
                url = "https://www.x.org/releases/individual/app/iceauth-1.0.11.tar.xz";
                sha256 = "sha256-nWM88NTR2Y4+8C0YZgNylYtgpnAW6Kcs0ECTqNj41Ok=";
              };
              deps = [ pkgs.xorgproto xlibBuild xvfbLibICEBuild ];
            };
          xvfbLibXineramaBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXinerama";
              version = pkgs.libXinerama.version;
              src = pkgs.libXinerama.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXextBuild ];
            };
          xvfbLibXresBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXres";
              version = pkgs.libXres.version;
              src = pkgs.libXres.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXextBuild ];
            };
          xvfbLibXcompositeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXcomposite";
              version = pkgs.libXcomposite.version;
              src = pkgs.libXcomposite.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXfixesBuild ];
            };
          xvfbLibXdamageBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXdamage";
              version = pkgs.libXdamage.version;
              src = pkgs.libXdamage.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXfixesBuild ];
            };
          xvfbLibXpresentBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXpresent";
              version = pkgs.libXpresent.version;
              src = pkgs.libXpresent.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXfixesBuild
                xvfbLibXrandrBuild
                xvfbLibXrenderBuild
              ];
            };
          libXftBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXft";
              version = pkgs.libXft.version;
              src = pkgs.libXft.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXrenderBuild
                freetype2Build
                fontconfigBuild
                expatBuild
              ];
              nativeDeps = [ pkgs.util-macros ];
            };
          dmenuBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/dmenu.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) dmenu;
              inherit (pkgs) xorgproto;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXft = libXftBuild;
              libXrender = xvfbLibXrenderBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
            };
          xvfbLibXiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXi";
              version = pkgs.libXi.version;
              src = pkgs.libXi.src;
              deps = [
                pkgs.xorgproto
                xlibBuild
                xvfbLibXextBuild
                xvfbLibXfixesBuild
              ];
              configureFlags = [
                "--disable-malloc0returnsnull"
              ];
            };
          xeyesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xeyes.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xeyes = pkgs.xeyes;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXi = xvfbLibXiBuild;
              libXrender = xvfbLibXrenderBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXmu = xvfbLibXmuBuild;
              libXt = xvfbLibXtBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              inherit (pkgs) xorgproto;
            };
          xclockBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xclock.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xclock = pkgs.xclock;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              libXmu = xvfbLibXmuBuild;
              libXt = xvfbLibXtBuild;
              libXaw = xvfbLibXawBuild;
              libXft = libXftBuild;
              libxkbfile = xvfbLibXkbfileBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              inherit (pkgs) xorgproto;
            };
          xcalcBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xcalc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xcalc = pkgs.xcalc;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXmu = xvfbLibXmuBuild;
              libXt = xvfbLibXtBuild;
              libXaw = xvfbLibXawBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              inherit (pkgs) xorgproto;
            };
          xmessageBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xmessage.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xmessage = pkgs.xmessage;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXmu = xvfbLibXmuBuild;
              libXt = xvfbLibXtBuild;
              libXaw = xvfbLibXawBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              inherit (pkgs) xorgproto;
            };
          fltkBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/fltk.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fltk_1_3 util-macros;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXft = libXftBuild;
              libxcbcursor = xcbCursorBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              fontconfig = fontconfigBuild;
              freetype2 = freetype2Build;
              expat = expatBuild;
              inherit (pkgs) xorgproto;
            };
          mkSharedXorgLib = args: pkgs.callPackage ./nix/pkgs/x11/xorg-cross-lib.nix ({
            inherit darwinCrossToolchain nativeLd;
            libSystem = libSystemBuild;
            nativeMesonTools = nativeMesonToolsDir;
            guestPrefix = true;
            shared = true;
          } // args);

          libXauSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXau";
            inherit (pkgs.libXau) version src;
            deps = [ pkgs.xorgproto ];
          };
          libXdmcpSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXdmcp";
            inherit (pkgs.libXdmcp) version src;
            deps = [ pkgs.xorgproto ];
          };
          libxcbSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libxcb";
            inherit (pkgs.libxcb) version src;
            deps = [ pkgs.xorgproto libXauSharedBuild libXdmcpSharedBuild ];
            nativeDeps = [ pkgs.python3 pkgs.xcb-proto ];
            configureFlags = [ "--disable-devel-docs" ];
            preConfigureExtra = ''
              export PYTHONPATH="${pkgs.xcb-proto}/${pkgs.python3.sitePackages}:$PYTHONPATH"
            '';
          };
          libX11SharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libX11";
            inherit (pkgs.libX11) version src;
            deps = [ pkgs.xorgproto pkgs.xtrans libxcbSharedBuild libXauSharedBuild libXdmcpSharedBuild ];
            configureFlags = [ "--disable-specs" "--enable-xlocaledir" ];
          };
          libXextSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXext";
            inherit (pkgs.libXext) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild libXauSharedBuild ];
          };
          libXrenderSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXrender";
            inherit (pkgs.libXrender) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild ];
          };
          libXfixesSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXfixes";
            inherit (pkgs.libXfixes) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild libXextSharedBuild ];
          };
          libXiSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXi";
            inherit (pkgs.libXi) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild libXextSharedBuild libXfixesSharedBuild ];
          };
          libXrandrSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXrandr";
            inherit (pkgs.libXrandr) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild libXextSharedBuild libXrenderSharedBuild ];
          };
          libXcursorSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libXcursor";
            inherit (pkgs.libXcursor) version src;
            deps = [ pkgs.xorgproto libX11SharedBuild libXrenderSharedBuild libXfixesSharedBuild ];
          };

          # Wine's schannel/bcrypt TLS backend is GnuTLS-only
          vulkanToolsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/vulkan-tools.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              nativeMesonTools = nativeMesonToolsDir;
              vulkanTools = pkgs.vulkan-tools;
              vulkanHeaders = pkgs.vulkan-headers;
              vulkanLoader = vulkanLoaderBuild;
              libX11 = libX11SharedBuild;
              libXext = libXextSharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              libXrandr = libXrandrSharedBuild;
              inherit (pkgs) xorgproto;
            };
          vulkanLoaderBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa/vulkan-loader.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nativeMesonTools = nativeMesonToolsDir;
              corefoundation = coreFoundationBuild;
              vulkanLoader = pkgs.vulkan-loader;
              vulkanHeaders = pkgs.vulkan-headers;
              libX11 = libX11SharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              libXrandr = libXrandrSharedBuild;
              libXrender = libXrenderSharedBuild;
              inherit (pkgs) xorgproto;
            };
          libxshmfenceSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-libxshmfence";
            inherit (pkgs.libxshmfence) version src;
            deps = [ pkgs.xorgproto ];
            configureFlags = [ "--with-shared-memory-dir=/tmp" ];
          };
          nettleSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-nettle";
            inherit (pkgs.nettle) version src;
            configureFlags = [
              "--enable-mini-gmp"
              "--disable-documentation"
              "--disable-assembler"
              "--disable-openssl"
            ];
          };
          gnutlsSharedBuild = if isDarwin then null else mkSharedXorgLib {
            pname = "puredarwin-gnutls";
            inherit (pkgs.gnutls) version src;
            deps = [ nettleSharedBuild ];
            postPatchExtra = ''
              substituteInPlace configure lib/Makefile.in \
                --replace ' -framework Security -framework CoreFoundation' ""
              # The keychain trust store goes with those frameworks. The header
              # is included whether or not that code path is compiled, and here
              # it is not: DEFAULT_TRUST_STORE_FILE wins the #if chain.
              substituteInPlace lib/system/certs.c \
                --replace-fail '#ifdef __APPLE__' '#if 0'
            '';
            configureFlags = [
              "--with-default-trust-store-file=/etc/ssl/cert.pem"
              "--with-nettle-mini"
              "--with-included-libtasn1"
              "--with-included-unistring"
              "--without-p11-kit"
              "--without-idn"
              "--without-tpm"
              "--without-tpm2"
              "--without-brotli"
              "--without-zstd"
              "--without-zlib"
              "--disable-doc"
              "--disable-tools"
              "--disable-tests"
              "--disable-cxx"
              "--disable-nls"
              "--disable-libdane"
              "--disable-guile"
              "--disable-hardware-acceleration"
            ];
          };

          wineToolsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/wine-tools.nix {
              # Only version and src are taken from it. nixpkgs' top-level `wine`
              # is winePackages.full, which pulls in pkgsi686Linux and so cannot
              # be evaluated on a non-x86 host; wine64 has the same version and
              # the same src derivation and evaluates everywhere.
              wine = pkgs.wine64;
              inherit (pkgs) flex bison freetype;
            };

          wineBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/wine.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              coreservices = coreServicesBuild;
              security = securityBuild;
              diskArbitration = diskArbitrationBuild;
              systemConfiguration = systemConfigurationBuild;
              iokit = iokitBuild;
              corefoundation = coreFoundationBuild;
              inherit (pkgs) perl;
              wineTools = wineToolsBuild;
              mingwGcc = pkgs.pkgsCross.mingwW64.buildPackages.gcc;
              mingwBintools = pkgs.pkgsCross.mingwW64.buildPackages.bintools;
              mingwGcc32 = pkgs.pkgsCross.mingw32.buildPackages.gcc;
              mingwBintools32 = pkgs.pkgsCross.mingw32.buildPackages.bintools;
              inherit (pkgs) python3;
              # See the note on wineToolsBuild above: wine64 for version/src so
              # this evaluates on non-x86 hosts.
              wine = pkgs.wine64;
              inherit (pkgs) xorgproto flex bison;
              libX11 = libX11SharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              libXext = libXextSharedBuild;
              libXrender = libXrenderSharedBuild;
              libXfixes = libXfixesSharedBuild;
              libXi = libXiSharedBuild;
              libXcursor = libXcursorSharedBuild;
              libXrandr = libXrandrSharedBuild;
              freetype = freetype2Build;
              fontconfig = fontconfigBuild;
              expat = expatBuild;
              gnutls = gnutlsSharedBuild;
              mesa = mesaBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonBuild;
              libxml2 = libxml2Build;
            };

          dilloBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/dillo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) dillo util-macros;
              fltk = fltkBuild;
              openssl = opensslBuild;
              libiconv = libiconvBuild;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libXext = xvfbLibXextBuild;
              libXrender = xvfbLibXrenderBuild;
              libXfixes = xvfbLibXfixesBuild;
              libXft = libXftBuild;
              libxcbcursor = xcbCursorBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              fontconfig = fontconfigBuild;
              freetype2 = freetype2Build;
              expat = expatBuild;
              inherit (pkgs) xorgproto;
            };
          ncursesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/ncurses.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              ncurses = pkgs.ncurses;
            };
          libiconvBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/libiconv.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconvReal = pkgs.libiconvReal;
            };
          toyboxBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/toybox.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              toybox = pkgs.toybox;
              zlib = xvfbZlibBuild;
            };
          nanoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/nano.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nano = pkgs.nano;
              ncurses = ncursesBuild;
            };
          xxdBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/xxd.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              tinyxxd = pkgs.tinyxxd;
            };
          xzBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/xz.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xz = pkgs.xz;
            };
          bmakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/bmake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              bmake = pkgs.bmake;
            };
          gnumakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/gnumake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              gnumake = pkgs.gnumake;
            };
          pkgconfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/pkgconf.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pkgconf = pkgs.pkgconf-unwrapped;
            };
          mesonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/meson.nix {
              python = pythonBuild;
              inherit (pkgs) meson;
            };
          cmakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/cmake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              libcurlDylib = libcurlDylibBuild;
              corefoundation = coreFoundationBuild;
              inherit (pkgs) cmake ninja;
            };
          ninjaBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/ninja.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              inherit (pkgs) ninja;
            };
          gnum4Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/gnum4.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              gnum4 = pkgs.gnum4;
            };
          autoconfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/autoconf.nix {
              autoconf = pkgs.autoconf;
            };
          automakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/automake.nix {
              automake = pkgs.automake;
              # Host autoconf, not autoconfBuild: this only drives
              # automake's own build/test-generation on the Linux builder
              autoconf = pkgs.autoconf;
            };
          bisonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/bison.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              bison = pkgs.bison;
            };
          flexBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/flex.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              flex = pkgs.flex;
            };
          pythonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/python.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              python3 = pkgs.python3;
              zlib = xvfbZlibBuild;
              openssl = opensslBuild;
              libffi = libffiBuild;
            };
          perlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/perl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              perl = pkgs.perl;
              zlib = xvfbZlibBuild;
            };
          zshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/zsh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconv = libiconvBuild;
              zsh = pkgs.zsh;
              ncurses = ncursesBuild;
            };
          fileBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/file.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              file = pkgs.file;
              zlib = xvfbZlibBuild;
            };
          opensslBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/openssl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssl = pkgs.openssl;
            };
          curlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/curl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              systemConfiguration = systemConfigurationBuild;
              curl = pkgs.curl;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          opensshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/openssh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssh = pkgs.openssh;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          gitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/base/git.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              git = pkgs.git;
              zlib = xvfbZlibBuild;
              curl = curlBuild;
              openssl = opensslBuild;
            };
          migcomDarwinBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain/migcom-darwin.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
            };
          ioregBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/ioreg.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
            };
          xkbcommonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xkbcommon.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libxml2 = libxml2Build;
              xkeyboard-config = xkeyboardConfigBuild;
            };
          waylandScannerBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/wayland-scanner.nix {
              src = ./src/ThirdParty/wayland;
              inherit (pkgs) expat libxml2;
            };
          waylandProtocolsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/wayland-protocols.nix {
              src = ./src/ThirdParty/wayland-protocols;
            };
          neuwldBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/neuwld.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              pixman = xvfbPixmanBuild;
              src = ./src/ThirdParty/neuwld;
            };
          neuswcBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/neuswc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              iokit = iokitBuild;
              iokitHeaders = iokitCFStaticBuild;
              neuwld = neuwldBuild;
              pixman = xvfbPixmanBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonNoxBuild;
              src = ./src/ThirdParty/neuswc;
            };
          wlrootsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/wlroots.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              libSystem = libSystemBuild;
              iokit = iokitBuild;
              iokitHeaders = iokitCFStaticBuild;
              pdgopSource = ./src/Libraries/PDGOP;
              pdVirglShim = pdVirglShimBuild;
              libdrm = libdrmBuild;
              pixman = xvfbPixmanBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonBuild;
              xcb = xcbBuild;
              xcbWm = xcbWmBuild;
              xwayland = xwaylandBuild;
              pdsurface = pdsurfaceBuild;
              src = ./src/ThirdParty/wlroots;
            };
          swayBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/sway.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              libSystem = libSystemBuild;
              cairo = cairoBuild;
              fribidi = fribidiBuild;
              freetype = freetype2Build;
              glib = glibBuild;
              harfbuzz = harfbuzzBuild;
              jsonc = jsoncBuild;
              libdrm = libdrmBuild;
              pango = pangoBuild;
              pcre2 = pcre2Build;
              pixman = xvfbPixmanBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              wlroots = wlrootsBuild;
              xkbcommon = xkbcommonBuild;
              xcb = xcbBuild;
              xcbWm = xcbWmBuild;
              src = ./src/ThirdParty/sway;
            };
          # Wayland-only image: wlroots/sway rebuilt without the Xwayland
          # backend so nothing on the image links libxcb.
          wlrootsNoxBuild =
            if isDarwin then null else wlrootsBuild.override {
              withXwayland = false;
              xcb = null;
              xcbWm = null;
              xwayland = null;
              xkbcommon = xkbcommonNoxBuild;
            };
          swayNoxBuild =
            if isDarwin then null else swayBuild.override {
              withXwayland = false;
              wlroots = wlrootsNoxBuild;
              cairo = cairoNoxBuild;
              xkbcommon = xkbcommonNoxBuild;
              pango = pangoNoxBuild;
              harfbuzz = harfbuzzNoxBuild;
              xcb = null;
              xcbWm = null;
            };
          tllistBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/tllist.nix { };
          fcftBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/fcft.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              libSystem = libSystemBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              pixman = xvfbPixmanBuild;
              harfbuzz = harfbuzzNoxBuild;
              libutf8proc = libutf8procBuild;
              tllist = tllistBuild;
              expat = expatBuild;
              zlib = xvfbZlibBuild;
              libpng = libpngBuild;
              libiconv = libiconvBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
            };
          footBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/foot.nix {
              inherit darwinCrossToolchain nativeLd;
              nativeMesonTools = nativeMesonToolsDir;
              libSystem = libSystemBuild;
              wayland = waylandBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = waylandScannerBuild;
              xkbcommon = xkbcommonNoxBuild;
              fontconfig = fontconfigBuild;
              freetype = freetype2Build;
              pixman = xvfbPixmanBuild;
              harfbuzz = harfbuzzNoxBuild;
              libutf8proc = libutf8procBuild;
              tllist = tllistBuild;
              fcft = fcftBuild;
              epollShim = pdEpollShimBuild;
              expat = expatBuild;
              zlib = xvfbZlibBuild;
              libpng = libpngBuild;
              libiconv = libiconvBuild;
              glib = glibBuild;
              pcre2 = pcre2Build;
              libffi = libffiBuild;
              ncurses = ncursesBuild;
            };
          pdEpollShimBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/pd-epoll-shim.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              src = ./src/Libraries/pd-epoll-shim;
            };
          waylandBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/wayland/wayland.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libffi = libffiBuild;
              waylandScanner = waylandScannerBuild;
              src = ./src/ThirdParty/wayland;
            };
          fastfetchBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apps/fastfetch.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              fastfetch = pkgs.fastfetch;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              libobjc = libobjcBuild;
              iokit = iokitBuild;
              openglFramework = openglFrameworkBuild;
              glu = gluBuild;
              libX11 = libX11SharedBuild;
              libXext = libXextSharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              mesa = mesaBuild;
            };
          xtermBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/x11/xterm.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              ncurses = ncursesBuild;
              xterm = pkgs.xterm;
              libX11 = xlibBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              libICE = xvfbLibICEBuild;
              libSM = xvfbLibSMBuild;
              libXt = xvfbLibXtBuild;
              libXext = xvfbLibXextBuild;
              libXmu = xvfbLibXmuBuild;
              libXpm = xvfbLibXpmBuild;
              libXaw = xvfbLibXawBuild;
              inherit (pkgs) xorgproto;
            };
          icuCoreBuild =
            pkgs.callPackage ./nix/pkgs/apple/icucore.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              icuSrc = pkgs.icu.src;
            };
          coreFoundationBuild =
            pkgs.callPackage ./nix/pkgs/apple/corefoundation.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              icu = icuCoreBuild;
              src = "${coreFoundationSource}/src/Frameworks/CoreFoundation";
              pdCompatInclude = "${coreFoundationSource}/src/Libraries/libSystem/libc/pd-compat-include";
              libobjc = libobjcBuild;
              foundationSrc = "${foundationSource}/src/Frameworks/Foundation";
            };
          libcxxabiDylibBuild =
            pkgs.callPackage ./nix/pkgs/apple/libcxxabi-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              src = libcxxDylibSource;
            };

          libcxxDylibBuild =
            pkgs.callPackage ./nix/pkgs/apple/libcxx-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              src = libcxxDylibSource;
            };
          libcxxTestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/libcxx-test.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              libcxxDylib = libcxxDylibBuild;
            };
          libobjcBuild =
            pkgs.callPackage ./nix/pkgs/apple/libobjc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              src = objcSource;
            };
          asmjitTestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/asmjit-test.nix {
              inherit darwinCrossToolchain nativeLd asmjitSrc;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
            };
          objcTestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/objc-test.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
            };
          foundationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/foundation.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              libffi = libffiBuild;
              # libSystem exports the DNSService* API but installs no header.
              dnssdInclude = ./src/Libraries/mDNSResponder/mDNSShared;
              # Likewise notify_post/notify_register_dispatch.
              notifyInclude = ./src/Libraries/XPC/notify;
              src = "${foundationSource}/src/Frameworks/Foundation";
            };
          protocolBufferBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/protocolbuffer.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              src = "${protocolBufferSource}/src/Libraries/ProtocolBuffer";
            };
          wirelessDiagnosticsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/wirelessdiagnostics.nix {
              src = "${wirelessDiagnosticsSource}/src/Libraries/WirelessDiagnostics";
            };
          iokitBuild =
            pkgs.callPackage ./nix/pkgs/apple/iokit.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokitCFStatic = iokitCFStaticBuild;
              libobjc = libobjcBuild;
              foundation = foundationBuild;
            };
          coreServicesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/coreservices.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              src = coreServicesSource;
            };
          openglFrameworkBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/opengl-framework.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              mesa = mesaBuild;
              glu = gluBuild;
              libX11 = libX11SharedBuild;
              inherit (pkgs) xorgproto;
              libXext = libXextSharedBuild;
              libxcb = libxcbSharedBuild;
              libXau = libXauSharedBuild;
              libXdmcp = libXdmcpSharedBuild;
              src = ./src/Frameworks/OpenGL;
            };
          appkitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/appkit.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              onyx2d = onyx2dBuild;
              coregraphics = coregraphicsBuild;
              coretext = coretextBuild;
              quartzcore = quartzcoreBuild;
              applicationservices = applicationservicesBuild;
              coreservices = coreServicesBuild;
              openglFramework = openglFrameworkBuild;
              windowserver = windowserverBuild;
              freetype2 = freetype2Build;
              fontconfig = fontconfigBuild;
              mesa = mesaBuild;
              glu = gluBuild;
              src = ./src/Frameworks/AppKit;
            };
          windowserverBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/windowserver.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              coregraphics = coregraphicsBuild;
              corefoundation = coreFoundationBuild;
              wayland = waylandBuild;
              xkbcommon = xkbcommonBuild;
              waylandProtocols = waylandProtocolsBuild;
              waylandScanner = pkgs.wayland-scanner;
              src = ./src/Frameworks/WindowServer;
            };
          applicationservicesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/applicationservices.nix {
              src = ./src/Frameworks/ApplicationServices;
            };
          corevideoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/corevideo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              openglFramework = openglFrameworkBuild;
              mesa = mesaBuild;
              glu = gluBuild;
              src = ./src/Frameworks/CoreVideo;
            };
          quartzcoreBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/quartzcore.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              foundation = foundationBuild;
              onyx2d = onyx2dBuild;
              coregraphics = coregraphicsBuild;
              coretext = coretextBuild;
              openglFramework = openglFrameworkBuild;
              corevideo = corevideoBuild;
              applicationservices = applicationservicesBuild;
              mesa = mesaBuild;
              glu = gluBuild;
              src = ./src/Frameworks/QuartzCore;
            };
          # arm64 twins of the remaining image packages and their dependency
          # closure, generated from the x86 wiring: toolchain/triple/libSystem
          # come from mkArm64Build, and each package's own PureDarwin deps are
          # pointed at their arm64 builds.

          # arm64 cross-compiled package set. Split out to keep this file
          # focused on the x86 build and the shared plumbing both arches use.
          arm64 = import ./nix/arm64.nix {
            inherit lib pkgs isDarwin;
            inherit arm64CrossToolchain;
            inherit armv6CrossToolchain;
            inherit coreFoundationBuild;
            inherit darwinCrossToolchain;
            inherit fbdoomSource;
            inherit foundationSource;
            inherit hostOtoolBuild;
            inherit i3statusShimBuild;
            inherit icuCoreBuild;
            inherit iokitBuild;
            inherit iokitCFStaticBuild;
            inherit kernelSource;
            inherit kextsSource;
            inherit launchctlBuild;
            inherit launchdBuild;
            inherit libSystemBuild;
            inherit libcxxDylibBuild;
            inherit libcxxabiDylibBuild;
            inherit libiconvBuild;
            inherit libobjcBuild;
            inherit mkPureDarwinBuild;
            inherit mkSystemConfigurationBuild;
            inherit symptomReporterBuild;
            inherit nativeLd;
            inherit nativeMesonToolsDir;
            inherit ncursesBuild;
            inherit coreServicesSource;
            inherit vteSrc;
            inherit asmjitSrc;
            inherit wineToolsBuild;
            inherit fexWow64Build;
            mingwAarch64Cc = if isDarwin then null else mingwAarch64.cc;
            mingwArm64ecCc = if isDarwin then null else mingwAarch64.arm64ecCc;
            inherit waylandScannerBuild;
            inherit waylandProtocolsBuild;
            inherit diskArbitrationSource;
            inherit securitySource;
            inherit userlandBuild;
            inherit userlandSource;
            inherit xkeyboardConfigBuild;
            inherit xlibLocaleBuild;
            inherit xvfbFontsBuild;
            inherit zshBuild;
            inherit xfconfArm64 libxfce4utilArm64 libxfce4uiArm64
              libxfce4windowingArm64 libwnckArm64Build garconArm64 exoArm64
              xfwm4Arm64 xfce4SessionArm64 xfce4PanelArm64 xfdesktopArm64
              xfce4AppfinderArm64 thunarArm64 xfce4SettingsArm64
              xfce4TerminalArm64;
          };
          inherit (arm64)
            atspi2CoreArm64Build
            autoconfArm64Build
            automakeArm64Build
            cairoArm64Build
            cairoGobjectArm64Build
            curlArm64Build
            dbusArm64Build
            dilloArm64Build
            dmenuArm64Build
            fastfetchArm64Build
            fastfetchNoGLArm64Build
            vmprobeArm64Build
            fltkArm64Build
            foundationArm64Build
            fribidiArm64Build
            gdkPixbufArm64Build
            gitArm64Build
            gtk3Arm64Build
            harfbuzzArm64Build
            i3Arm64Build
            ioregArm64Build
            libXftArm64Build
            libcssArm64Build
            libcurlDylibArm64Build
            libcxxTestArm64Build
            libdomArm64Build
            libepoxyArm64Build
            libfontencArm64Build
            libhubbubArm64Build
            libnsbmpArm64Build
            libnsgifArm64Build
            libnsutilsArm64Build
            libparserutilsArm64Build
            libutf8procArm64Build
            libwapcapletArm64Build
            libzDylibArm64Build
            mesaArm64Build
            mesaDemosArm64Build
            migcomDarwinArm64Build
            netsurfArm64Build
            openglFrameworkArm64Build
            opensshArm64Build
            pangoArm64Build
            pythonArm64Build
            securityArm64Build
            systemConfigurationArm64Build
            startupNotificationArm64Build
            xcalcArm64Build
            xcbArm64Build
            xcbCursorArm64Build
            xcbImageArm64Build
            xcbKeysymsArm64Build
            xcbRenderUtilArm64Build
            xcbUtilArm64Build
            xcbWmArm64Build
            xcbXrmArm64Build
            xclockArm64Build
            xeyesArm64Build
            xkbcommonArm64Build
            xkbcompArm64Build
            xlibArm64Build
            xmessageArm64Build
            xorgArm64Build
            xtermArm64Build
            xvfbArm64Build
            xvfbLibICEArm64Build
            xvfbLibSMArm64Build
            xvfbLibXauArm64Build
            xvfbLibXawArm64Build
            xvfbLibXcursorArm64Build
            xvfbLibXdmcpArm64Build
            xvfbLibXextArm64Build
            xvfbLibXfixesArm64Build
            xvfbLibXfont2Arm64Build
            xvfbLibXiArm64Build
            xvfbLibXkbfileArm64Build
            xvfbLibXmuArm64Build
            xvfbLibXpmArm64Build
            xvfbLibXrandrArm64Build
            xvfbLibXrenderArm64Build
            xvfbLibXtArm64Build
            xvfbLibxcvtArm64Build
            xvfbPixmanArm64Build
            pdVirglShimArm64Build
            hostOtoolArm64Build
            xkeyboardConfigArm64Build
            xlibLocaleArm64Build
            xvfbFontsArm64Build
            i3statusShimArm64Build
            libSystemArm64Build
            icuCoreArm64Build
            libcxxabiDylibArm64Build
            libcxxDylibArm64Build
            libobjcArm64Build
            coreFoundationArm64Build
            iokitArm64Build
            launchdArm64Build
            launchctlArm64Build
            mkArm64Build
            xvfbZlibArm64Build
            toyboxArm64Build
            xzArm64Build
            fileArm64Build
            opensslArm64Build
            sqliteArm64Build
            libjpegArm64Build
            libwebpArm64Build
            libgpgErrorArm64Build
            libgcryptArm64Build
            libtasn1Arm64Build
            nghttp2Arm64Build
            libpslArm64Build
            gettextArm64Build
            libxshmfenceSharedArm64Build
            nettleSharedArm64Build
            xvfbLibXineramaArm64Build
            xvfbLibXresArm64Build
            xvfbLibXcompositeArm64Build
            xvfbLibXdamageArm64Build
            xvfbLibXpresentArm64Build
            iceauthArm64Build
            xrandrArm64Build
            xrdbArm64Build
            xinitArm64Build
            coreServicesArm64Build
            iomediacheckArm64Build
            pdsurfaceArm64Build
            diskArbitrationArm64Build
            libcrocoArm64Build
            librsvgArm64Build
            gnutlsSharedArm64Build
            libsoupArm64Build
            vteArm64Build
            libwnckArm64Build
            llvmCrossArm64Build
            compilerRtArm64Build
            wineArm64Build
            asmjitTestArm64Build
            gtkLayerShellArm64Build
            libgbmArm64Build
            jsoncArm64Build
            libX11SharedArm64Build
            libxcbSharedArm64Build
            libXauSharedArm64Build
            libXdmcpSharedArm64Build
            libXextSharedArm64Build
            libXrenderSharedArm64Build
            libXfixesSharedArm64Build
            libXiSharedArm64Build
            libXcursorSharedArm64Build
            libXrandrSharedArm64Build
            mesonArm64Build
            cmakeArm64Build
            ninjaArm64Build
            clangCrossArm64Build
            wlrootsArm64Build
            swayArm64Build
            libdrmArm64Build
            xwaylandArm64Build
            libdisplayInfoArm64Build
            xvfbLibXxf86vmArm64Build
            waylandArm64Build
            bmakeArm64Build
            gnumakeArm64Build
            gnum4Arm64Build
            pkgconfArm64Build
            bisonArm64Build
            flexArm64Build
            xxdArm64Build
            nanoArm64Build
            libffiArm64Build
            expatArm64Build
            pcre2Arm64Build
            libevArm64Build
            libpngArm64Build
            freetype2Arm64Build
            fontconfigArm64Build
            libxml2Arm64Build
            yajlArm64Build
            glibArm64Build
            libiconvArm64Build
            ncursesArm64Build
            zshArm64Build
            userlandArm64Build
            kernelArm64Build
            kernelArm64VirtBuild
            kernelArm64VirtDebugBuild
            kernelArm64T8010Build
            kernelArm64T8010DebugBuild
            kernelArm64Bcm2837Build
            kernelArm64Bcm2837DebugBuild
            kernelArm32Bcm2835Build
            kernelArm32Bcm2835DebugBuild
            kernelArm32Bcm2835DevBuild
            kextsArm32Bcm2835Build
            compilerRtArmv6Build
            libSystemArmv6Build
            userlandArm32Bcm2835Build
            kextsArm64Build
            splitBaseSystemArm64VirtMinimal
            splitBaseSystemArm64VirtMinimalRelease
            splitBaseSystemArm64VirtWayland
            imageExtraPackageSetArm64
            imageExtraPackagesArm64Nox
            ;
          # The XFCE desktop is instantiated in this file (it needs both the
          # arm64 builds and the shared sources), so it is appended here rather
          # than inside arm64.nix where the rest of the list lives.
          imageExtraPackagesArm64 = arm64.imageExtraPackagesArm64 ++ [
            xfconfArm64 libxfce4utilArm64 libxfce4uiArm64 libxfce4windowingArm64
            garconArm64 exoArm64 xfwm4Arm64 xfdesktopArm64 thunarArm64
          ];
          securityBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/security.nix {
              inherit mkPureDarwinBuild;
              corefoundation = coreFoundationBuild;
              libobjc = libobjcBuild;
              foundation = foundationBuild;
              sqlite = sqliteBuild;
              src = securitySource;
            };
          mkSystemConfigurationBuild = { corefoundation, libobjc, security, iokit, symptomReporter }:
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-systemconfiguration";
              src = systemConfigurationSource;
              buildTargets = [ "SystemConfiguration" "configd" "bootplib_static" "ipconfiguration_static" "ipconfig" ];
              enableProjects = false;
              enableKernel = false;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_SYSTEMCONFIGURATION=ON"
                "-DPUREDARWIN_COREFOUNDATION_PREFIX=${corefoundation}"
                "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjc}"
                "-DPUREDARWIN_SECURITY_PREFIX=${security}"
                # InterfaceNamer drives the interface-naming user client through
                # IOKitUser's CF-based IOKitLib.
                "-DPUREDARWIN_IOKIT_PREFIX=${iokit}"
                "-DPUREDARWIN_SYMPTOMREPORTER_PREFIX=${symptomReporter}"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                fw="$out/System/Library/Frameworks/SystemConfiguration.framework"
                mkdir -p "$fw/Versions/A/Headers"
                cp build-nix/src/Libraries/SystemConfiguration/libSystemConfiguration.dylib \
                  "$fw/Versions/A/SystemConfiguration"
                cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration/. \
                  "$fw/Versions/A/Headers/"
                # Resources the framework looks up at runtime: the localized
                # interface-name strings and NetworkConfiguration.plist, which
                # InterfaceNamer consults while naming. Without them every
                # lookup logs "failed to get resource url" and falls back to an
                # empty strings table.
                mkdir -p "$fw/Versions/A/Resources"
                cp src/Libraries/SystemConfiguration/SystemConfiguration.fproj/NetworkConfiguration.plist \
                  "$fw/Versions/A/Resources/"
                cp -a src/Libraries/SystemConfiguration/SystemConfiguration.fproj/en.lproj \
                  "$fw/Versions/A/Resources/"
                ln -s A "$fw/Versions/Current"
                ln -s Versions/Current/SystemConfiguration "$fw/SystemConfiguration"
                ln -s Versions/Current/Headers "$fw/Headers"
                ln -s Versions/Current/Resources "$fw/Resources"
                # Flat dylib alias, as Security.framework and OpenGL have.
                mkdir -p "$out/usr/lib"
                ln -s "../../System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration" \
                  "$out/usr/lib/libSystemConfiguration.dylib"
                mkdir -p "$out/include"
                cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration "$out/include/"
                # configd, the SCDynamicStore server, plus its launchd job.
                mkdir -p "$out/usr/libexec" "$out/System/Library/LaunchDaemons"
                cp build-nix/src/Libraries/SystemConfiguration/configd.tproj/configd \
                  "$out/usr/libexec/configd"
                cp src/Libraries/SystemConfiguration/configd.tproj/com.apple.configd.plist \
                  "$out/System/Library/LaunchDaemons/"
                # Builtin plugins live inside configd, but plugin_support.c still
                # discovers them by walking /System/Library/SystemConfiguration
                # for bundles, so each one needs its Info.plist installed.
                # IPConfiguration is builtin too, but its sources live in the
                # bootp project rather than under Plugins/, and its plist is
                # generated by src/Libraries/bootp/CMakeLists.txt.
                for p in PreferencesMonitor LinkConfiguration KernelEventMonitor IPMonitor InterfaceNamer; do
                  d="$out/System/Library/SystemConfiguration/$p.bundle/Contents"
                  mkdir -p "$d"
                  # The build-dir copy, not the source one: these plists set
                  # CFBundleIdentifier to Xcode's $(PRODUCT_BUNDLE_IDENTIFIER),
                  # and configd skips any bundle whose identifier does not
                  # resolve. The build substitutes it; the source cannot.
                  cp "build-nix/src/Libraries/SystemConfiguration/configd.tproj/plugin-plists/$p/Info.plist" "$d/"
                done
                d="$out/System/Library/SystemConfiguration/IPConfiguration.bundle/Contents"
                mkdir -p "$d"
                cp "build-nix/src/Libraries/bootp/plugin-plists/IPConfiguration/Info.plist" "$d/"
                # ipconfig(8): SystemStarter runs `ipconfig waitall` to block
                # until every interface has finished configuring.
                mkdir -p "$out/usr/sbin"
                cp build-nix/src/Libraries/bootp/ipconfig "$out/usr/sbin/ipconfig"
                runHook postInstall
              '';
            });
          # IOKitUser's CoreFoundation-based IOKitLib. Built apart from libSystem
          # because it links CoreFoundation, which itself links libSystem.
          iokitCFStaticBuild =
            (mkPureDarwinBuild {
              pname = "puredarwin-iokitcf-static";
              src = iokitCFSource;
              buildTargets = [ "IOKitCF" ];
              enableProjects = false;
              enableKernel = false;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_IOKITCF=ON"
                "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationBuild}"
                "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjcBuild}"
                "-DPUREDARWIN_FOUNDATION_PREFIX=${foundationBuild}"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p "$out/usr/lib/system"
                cp build-nix/src/Libraries/IOKit/libIOKitCF.a "$out/usr/lib/system/"
                mkdir -p "$out/include"
                cp -a src/Libraries/IOKit/iokituser/include/IOKit "$out/include/"
                runHook postInstall
              '';
            });
          # Diagnostic: replays diskarbitrationd's DADiskCreateFromIOMedia checks
          # against one IOMedia and names the failing one.
          iomediacheckBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/iomediacheck.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              iokitHeaders = iokitCFStaticBuild;
            };
          # Apple's SymptomReporter is a private framework with no source and
          # no SDK header; this is PureDarwin's own, recording symptoms through
          # os_log. bootp's IPConfiguration plugin is the consumer.
          symptomReporterBuild =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-symptomreporter";
              src = symptomReporterSource;
              buildTargets = [ "SymptomReporter" ];
              enableProjects = false;
              enableKernel = false;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_SYMPTOMREPORTER=ON"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                fw="$out/System/Library/PrivateFrameworks/SymptomReporter.framework"
                mkdir -p "$fw/Versions/A/Headers"
                cp build-nix/src/Libraries/SymptomReporter/libSymptomReporter.dylib \
                  "$fw/Versions/A/SymptomReporter"
                cp -a src/Libraries/SymptomReporter/include/SymptomReporter/. \
                  "$fw/Versions/A/Headers/"
                ln -s A "$fw/Versions/Current"
                ln -s Versions/Current/SymptomReporter "$fw/SymptomReporter"
                ln -s Versions/Current/Headers "$fw/Headers"
                mkdir -p "$out/include"
                cp -a src/Libraries/SymptomReporter/include/SymptomReporter "$out/include/"
                runHook postInstall
              '';
            });
          diskArbitrationBuild =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-diskarbitration";
              src = diskArbitrationSource;
              buildTargets = [ "DiskArbitration" "diskarbitrationd" ];
              enableProjects = false;
              enableKernel = false;
              enableUserspace = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_DISKARBITRATION=ON"
                "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationBuild}"
                "-DPUREDARWIN_IOKIT_PREFIX=${iokitBuild}"
                "-DPUREDARWIN_SECURITY_PREFIX=${securityBuild}"
                "-DPUREDARWIN_SYSTEMCONFIGURATION_PREFIX=${systemConfigurationBuild}"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                fw="$out/System/Library/Frameworks/DiskArbitration.framework"
                mkdir -p "$fw/Versions/A/Headers"
                cp build-nix/src/Libraries/DiskArbitration/libDiskArbitration.dylib \
                  "$fw/Versions/A/DiskArbitration"
                cp -a src/Libraries/DiskArbitration/include/DiskArbitration/. \
                  "$fw/Versions/A/Headers/"
                ln -s A "$fw/Versions/Current"
                ln -s Versions/Current/DiskArbitration "$fw/DiskArbitration"
                ln -s Versions/Current/Headers "$fw/Headers"
                mkdir -p "$out/usr/lib"
                ln -s "../../System/Library/Frameworks/DiskArbitration.framework/Versions/A/DiskArbitration" \
                  "$out/usr/lib/libDiskArbitration.dylib"
                mkdir -p "$out/include"
                cp -a src/Libraries/DiskArbitration/include/DiskArbitration "$out/include/"
                mkdir -p "$out/usr/libexec" "$out/System/Library/LaunchDaemons"
                cp build-nix/src/Libraries/DiskArbitration/diskarbitrationd \
                  "$out/usr/libexec/diskarbitrationd"
                cp src/Libraries/DiskArbitration/diskarbitrationd/com.apple.diskarbitrationd.plist \
                  "$out/System/Library/LaunchDaemons/"
                runHook postInstall
              '';
            });
          systemConfigurationBuild =
            mkSystemConfigurationBuild {
              corefoundation = coreFoundationBuild;
              libobjc = libobjcBuild;
              security = securityBuild;
              iokit = iokitBuild;
              symptomReporter = symptomReporterBuild;
            };
          systemStarterBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/apple/systemstarter.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          launchctlBuild =
            pkgs.callPackage ./nix/pkgs/apple/launchctl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          launchdBuild =
            pkgs.callPackage ./nix/pkgs/apple/launchd.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          libSystemBuild = mkPureDarwinBuild {
            pname = "puredarwin-libsystem";
            src = libSystemSource;
            buildTargets = [ "libSystem_B_stub" "libDER_static" "dyld" "libsystem_kernel_static" "libdispatch_static" "XPC_libnv_static" "XPC_libinfo_static" "XPC_libxpc_static" "XPC_launchd_static" "XPC_launchd_mig_static" "XPC_notify_client_static" "notifyd" "logd" ];
            enableUserspace = false;
            enableKernel = false;
            installUserland = false;
            installKernel = false;
            installLibSystem = true;
          };
          fbdoomBuild = (mkPureDarwinBuild {
            pname = "puredarwin-fbdoom";
            src = fbdoomSource;
            buildTargets = [ "fbdoom" ];
            enableProjects = false;
            enableKernel = false;
            installUserland = false;
            installKernel = false;
            extraCmakeFlags = [
              "-DPUREDARWIN_ENABLE_FBDOOM=ON"
              "-DPUREDARWIN_FBDOOM_SOURCE=${fbdoomExternalSrc}"
              "-DPUREDARWIN_CHOCOLATE_DOOM_SOURCE=${chocolateDoomPatchedSrc}"
            ];
          }).overrideAttrs (old: {
            installPhase = ''
              runHook preInstall
              mkdir -p $out/usr/bin
              cp build-nix/src/Userspace/fbdoom/fbdoom $out/usr/bin/fbdoom
              runHook postInstall
            '';
          });
          kernelBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "RELEASE";
          };
          kernelDebugBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel-debug";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "DEBUG";
          };
          xnuHeadersBuild = mkPureDarwinBuild {
            pname = "puredarwin-xnu-headers";
            src = kernelSource;
            buildTargets = [ "xnu_headers.extproj" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installXnuHeaders = true;
            xnuKernelConfig = "RELEASE";
          };
          kextsBuild = mkPureDarwinBuild {
            pname = "puredarwin-kexts";
            src = kextsSource;
            buildTargets = [ "kexts" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            enableIOGraphicsFamily = true;
          };
          iographicsBuild = mkPureDarwinBuild {
            pname = "puredarwin-iographics";
            src = kextsSource;
            buildTargets = [ "IOGraphicsFamily.kext" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [ "IOGraphicsFamily.kext" ];
            enableIOGraphicsFamily = true;
          };
          # Image contents (see nix/image-contents.nix).
          imageContents = import ./nix/image-contents.nix {
            inherit
              atspi2CoreBuild autoconfBuild automakeBuild bisonBuild bmakeBuild cairoBuild
              cairoGobjectBuild cctoolsBuild coreFoundationBuild curlBuild darwinCrossToolchain
              coreServicesBuild dbusBuild dilloBuild diskArbitrationBuild wineBuild dmenuBuild exoBuild expatBuild fastfetchBuild
              libX11SharedBuild libxcbSharedBuild libXauSharedBuild libXdmcpSharedBuild
              libXextSharedBuild libXrenderSharedBuild libXfixesSharedBuild libXiSharedBuild
              libXcursorSharedBuild libXrandrSharedBuild nettleSharedBuild gnutlsSharedBuild glibNetworkingBuild llvmCrossBuild vulkanLoaderBuild libxshmfenceSharedBuild vulkanToolsBuild
              fbdoomBuild fbdoomExternalSrc fileBuild flexBuild fontconfigBuild foundationBuild
              freetype2Build fribidiBuild garconBuild gdkPixbufBuild gitBuild glibBuild gnum4Build
              gnumakeBuild gtk3Build gtkLayerShellBuild gtk3NoxBuild gtkLayerShellNoxBuild onyx2dBuild coregraphicsBuild coretextBuild coredataBuild cgScreenDemoBuild gershwinSystemBuild gershwinAssetsBuild cairoNoxBuild dbusNoxBuild pdEpollShimBuild tllistBuild fcftBuild footBuild userlandNoxBuild pangoNoxBuild netsurfNoxBuild libepoxyNoxBuild fastfetchNoxBuild harfbuzzNoxBuild atspi2CoreNoxBuild cairoGobjectNoxBuild xkbcommonNoxBuild mesaNoxBuild openglFrameworkNoxBuild mesaDemosNoxBuild librsvgNoxBuild harfbuzzBuild i3Build i3statusShimBuild iceauthBuild
              cursorThemeBuild iconThemesBuild icuCoreBuild imageExtraPackagesArm64 imageExtraPackagesArm64Nox iographicsBuild iokitBuild asmjitTestArm64Build
              iomediacheckBuild ioregBuild isDarwin jsoncBuild kc-tools kernelArm64Build kernelArm64VirtBuild
              kernelArm64VirtDebugBuild kernelArm64T8010Build kernelArm64T8010DebugBuild kernelArm64Bcm2837Build kernelArm64Bcm2837DebugBuild kernelArm32Bcm2835Build kernelArm32Bcm2835DebugBuild kernelArm32Bcm2835DevBuild
              kextsArm32Bcm2835Build compilerRtArmv6Build
              kernelBuild kernelDebugBuild kernelSource kextsArm64Build kextsBuild
              launchctlBuild launchdBuild lib libSystemBuild libdrmBuild libXftBuild libapfsrwBuild libcssBuild waylandBuild waylandProtocolsBuild neuwldBuild neuswcBuild wlrootsBuild swayBuild wlrootsNoxBuild swayNoxBuild
              pdsurfaceBuild libgbmBuild libcurlDylibBuild libcxxDylibBuild libcxxTestBuild libcxxabiDylibBuild libdisplayInfoBuild
              libdomBuild libepoxyBuild libevBuild libffiBuild libhubbubBuild libiconvArm64Build
              libiconvBuild libnsbmpBuild libnsgifBuild libnsutilsBuild libobjcBuild libparserutilsBuild
              libpngBuild libutf8procBuild libwapcapletBuild libwnckBuild libxfce4uiBuild
              libxfce4utilBuild libxfce4windowingBuild libxml2Build libzDylibBuild mesaBuild gluBuild gluNoxBuild
              mesaDemosBuild migcomDarwinBuild mkPureDarwinBuild clangCrossBuild cmakeBuild kcToolsGuestBuild mesonBuild nanoBuild nativeLd ncursesBuild ninjaBuild
              netsurfBuild objcTestBuild openglFrameworkBuild opensshBuild opensslBuild
              pangoBuild pcre2Build pdVirglShimBuild pkgconfBuild pkgs pythonBuild
              securityBuild symptomReporterBuild splitBaseSystemArm64VirtMinimal splitBaseSystemArm64VirtMinimalRelease splitBaseSystemArm64VirtWayland
              startupNotificationBuild system systemConfigurationBuild systemStarterBuild tccBuild
              fastfetchNoGLArm64Build foundationArm64Build vmprobeArm64Build
              toyboxArm64Build toyboxBuild userlandBuild vteBuild xcalcBuild xcbBuild xcbCursorBuild
              xcbImageBuild xcbKeysymsBuild xcbRenderUtilBuild xcbUtilBuild xcbWmBuild xcbXrmBuild
              xclockBuild xeyesBuild thunarBuild xfce4AppfinderBuild xfce4PanelBuild xfce4SessionBuild
              xfce4SettingsBuild xfce4TerminalBuild xfconfBuild xfdesktopBuild xfwm4Build xinitBuild
              xkbcommonBuild xkbcompBuild xkeyboardConfigBuild xlibBuild xlibLocaleBuild xmessageBuild
              xnu-loader xnuHeadersBuild xorgBuild xwaylandBuild xrdbBuild xrandrBuild xtermBuild xvfbBuild xvfbFontsBuild xvfbPixmanBuild
              xvfbLibICEBuild xvfbLibSMBuild xvfbLibXauBuild xvfbLibXcompositeBuild xvfbLibXcursorBuild
              xvfbLibXdamageBuild xvfbLibXdmcpBuild xvfbLibXextBuild xvfbLibXfixesBuild
              xvfbLibXineramaBuild xvfbLibXkbfileBuild xvfbLibXpresentBuild xvfbLibXrandrBuild
              xvfbLibXrenderBuild xvfbLibXresBuild xvfbLibxcvtBuild xvfbZlibBuild xxdBuild xzBuild
              yajlBuild zshArm64Build zshBuild libcrocoBuild librsvgBuild gettextBuild
              webkitgtkBuild libsoupBuild sqliteBuild libpslBuild nghttp2Build
              libgcryptBuild libgpgErrorBuild libtasn1Build libjpegBuild libwebpBuild
              ;
          };
          inherit (imageContents)
            fullBuild
            splitBaseSystem
            splitBaseSystemStripped
            splitBaseSystemMinimal
            imageExtraPackageSet
            commonPackages
            linuxPackages
            linuxApps
            ;
          # Investigative builds, not image contents.
          probePackages = lib.optionalAttrs (!isDarwin) {
            # Cross toolchain, exposed so out-of-tree flakes (e.g. checkm8-tools'
            # PongoOS build) can link Mach-O with the real cctools ld64.
            arm64-cross-toolchain = arm64CrossToolchain;
            native-ld = nativeLd;
            coreservices = coreServicesBuild;
            wine-tools = wineToolsBuild;
            libX11-shared = libX11SharedBuild;
            libxcb-shared = libxcbSharedBuild;
            libXau-shared = libXauSharedBuild;
            libXdmcp-shared = libXdmcpSharedBuild;
            libXext-shared = libXextSharedBuild;
            libXrender-shared = libXrenderSharedBuild;
            libXfixes-shared = libXfixesSharedBuild;
            libXi-shared = libXiSharedBuild;
            libXcursor-shared = libXcursorSharedBuild;
            libXrandr-shared = libXrandrSharedBuild;
            nettle-shared = nettleSharedBuild;
            gnutls-shared = gnutlsSharedBuild;
            libxshmfence-shared = libxshmfenceSharedBuild;
            xrandr = xrandrBuild;
            vulkan-loader = vulkanLoaderBuild;
            vulkan-tools = vulkanToolsBuild;
            llvm-cross = llvmCrossBuild;
            compiler-rt = compilerRtBuild;
            mingw-aarch64-cc = if isDarwin then null else mingwAarch64.cc;
            mingw-arm64ec-cc = if isDarwin then null else mingwAarch64.arm64ecCc;
            fex-wow64 = fexWow64Build;
            mingw-aarch64-crt = if isDarwin then null else mingwAarch64.mingw;
            asmjit-test = asmjitTestBuild;
            clang = clangCrossBuild;
            kc-tools-guest = kcToolsGuestBuild;
            libgpg-error = libgpgErrorBuild;
            libgcrypt = libgcryptBuild;
            libjpeg = libjpegBuild;
            libwebp = libwebpBuild;
            nghttp2 = nghttp2Build;
            libpsl = libpslBuild;
            libsoup = libsoupBuild;
            webkitgtk = webkitgtkBuild;
            gettext = gettextBuild;
            libtasn1 = libtasn1Build;
            sqlite = sqliteBuild;
            glib-networking = glibNetworkingBuild;
            freetype-shared = freetype2Build;
            neuwld = neuwldBuild;
            neuswc = neuswcBuild;
            quartzcore = quartzcoreBuild;
            corevideo = corevideoBuild;
            applicationservices = applicationservicesBuild;
            windowserver = windowserverBuild;
            appkit = appkitBuild;
          };
          arm64Packages = lib.optionalAttrs (!isDarwin) {
            libSystem-armv6 = arm64.libSystemArmv6Build;
            userland-arm32-bcm2835 = arm64.userlandArm32Bcm2835Build;
            libsystem-armv6 = arm64.libSystemArmv6Build;
            libobjc-armv6 = arm64.libobjcArmv6Build;
            libcxxabi-armv6 = arm64.libcxxabiDylibArmv6Build;
            icu-armv6 = arm64.icuCoreArmv6Build;
            corefoundation-armv6 = arm64.coreFoundationArmv6Build;
            iokit-armv6 = arm64.iokitArmv6Build;
            launchd-armv6 = arm64.launchdArmv6Build;
            compiler-rt-armv6 = arm64.compilerRtArmv6Build;
            kernel-arm32-bcm2835 = arm64.kernelArm32Bcm2835Build;
            kernel-arm32-bcm2835-debug = arm64.kernelArm32Bcm2835DebugBuild;
            kernel-arm32-bcm2835-dev = arm64.kernelArm32Bcm2835DevBuild;
            kexts-arm32-bcm2835 = arm64.kextsArm32Bcm2835Build;
            zlib-arm64 = xvfbZlibArm64Build;
            toybox-arm64 = toyboxArm64Build;
            xz-arm64 = xzArm64Build;
            file-arm64 = fileArm64Build;
            openssl-arm64 = opensslArm64Build;
            sqlite-arm64 = sqliteArm64Build;
            libjpeg-arm64 = libjpegArm64Build;
            libwebp-arm64 = libwebpArm64Build;
            libgpg-error-arm64 = libgpgErrorArm64Build;
            libgcrypt-arm64 = libgcryptArm64Build;
            libtasn1-arm64 = libtasn1Arm64Build;
            nghttp2-arm64 = nghttp2Arm64Build;
            libpsl-arm64 = libpslArm64Build;
            gettext-arm64 = gettextArm64Build;
            libxshmfence-arm64 = libxshmfenceSharedArm64Build;
            nettle-arm64 = nettleSharedArm64Build;
            libXinerama-arm64 = xvfbLibXineramaArm64Build;
            libXres-arm64 = xvfbLibXresArm64Build;
            libXcomposite-arm64 = xvfbLibXcompositeArm64Build;
            libXdamage-arm64 = xvfbLibXdamageArm64Build;
            libXpresent-arm64 = xvfbLibXpresentArm64Build;
            iceauth-arm64 = iceauthArm64Build;
            xrandr-arm64 = xrandrArm64Build;
            xrdb-arm64 = xrdbArm64Build;
            xinit-arm64 = xinitArm64Build;
            coreServices-arm64 = coreServicesArm64Build;
            iomediacheck-arm64 = iomediacheckArm64Build;
            pdsurface-arm64 = pdsurfaceArm64Build;
            diskArbitration-arm64 = diskArbitrationArm64Build;
            libcroco-arm64 = libcrocoArm64Build;
            librsvg-arm64 = librsvgArm64Build;
            gnutls-arm64 = gnutlsSharedArm64Build;
            libsoup-arm64 = libsoupArm64Build;
            vte-arm64 = vteArm64Build;
            libwnck-arm64 = libwnckArm64Build;
            llvm-arm64 = llvmCrossArm64Build;
            compiler-rt-arm64 = compilerRtArm64Build;
            wine-arm64 = wineArm64Build;
            asmjit-test-arm64 = asmjitTestArm64Build;
            gtkLayerShell-arm64 = gtkLayerShellArm64Build;
            libgbm-arm64 = libgbmArm64Build;
            jsonc-arm64 = jsoncArm64Build;
            meson-arm64 = mesonArm64Build;
            cmake-arm64 = cmakeArm64Build;
            ninja-arm64 = ninjaArm64Build;
            clang-arm64 = clangCrossArm64Build;
            wlroots-arm64 = wlrootsArm64Build;
            sway-arm64 = swayArm64Build;
            libdrm-arm64 = libdrmArm64Build;
            xwayland-arm64 = xwaylandArm64Build;
            libdisplayInfo-arm64 = libdisplayInfoArm64Build;
            libXxf86vm-arm64 = xvfbLibXxf86vmArm64Build;
            wayland-arm64 = waylandArm64Build;
            xfconf-arm64 = xfconfArm64;
            libxfce4util-arm64 = libxfce4utilArm64;
            libxfce4ui-arm64 = libxfce4uiArm64;
            xfwm4-arm64 = xfwm4Arm64;
            libxfce4windowing-arm64 = libxfce4windowingArm64;
            garcon-arm64 = garconArm64;
            exo-arm64 = exoArm64;
            xfce4Session-arm64 = xfce4SessionArm64;
            xfce4Panel-arm64 = xfce4PanelArm64;
            xfdesktop-arm64 = xfdesktopArm64;
            xfce4Terminal-arm64 = xfce4TerminalArm64;
            xfce4Settings-arm64 = xfce4SettingsArm64;
            xfce4Appfinder-arm64 = xfce4AppfinderArm64;
            thunar-arm64 = thunarArm64;
            ncurses-arm64 = ncursesArm64Build;
            libiconv-arm64 = libiconvArm64Build;
            zsh-arm64 = zshArm64Build;
            bmake-arm64 = bmakeArm64Build;
            gnumake-arm64 = gnumakeArm64Build;
            gnum4-arm64 = gnum4Arm64Build;
            pkgconf-arm64 = pkgconfArm64Build;
            bison-arm64 = bisonArm64Build;
            flex-arm64 = flexArm64Build;
            xxd-arm64 = xxdArm64Build;
            nano-arm64 = nanoArm64Build;
            libffi-arm64 = libffiArm64Build;
            expat-arm64 = expatArm64Build;
            pcre2-arm64 = pcre2Arm64Build;
            libev-arm64 = libevArm64Build;
            libpng-arm64 = libpngArm64Build;
            freetype2-arm64 = freetype2Arm64Build;
            fontconfig-arm64 = fontconfigArm64Build;
            libxml2-arm64 = libxml2Arm64Build;
            yajl-arm64 = yajlArm64Build;
            glib-arm64 = glibArm64Build;
            userland-arm64 = userlandArm64Build;
            coreFoundation-arm64 = coreFoundationArm64Build;
            i3statusShim-arm64 = i3statusShimArm64Build;
            icuCore-arm64 = icuCoreArm64Build;
            iokit-arm64 = iokitArm64Build;
            libcxxDylib-arm64 = libcxxDylibArm64Build;
            libcxxabiDylib-arm64 = libcxxabiDylibArm64Build;
            libobjc-arm64 = libobjcArm64Build;
            pdVirglShim-arm64 = pdVirglShimArm64Build;
            xkeyboardConfig-arm64 = xkeyboardConfigArm64Build;
            xlibLocale-arm64 = xlibLocaleArm64Build;
            xvfbFonts-arm64 = xvfbFontsArm64Build;
            atspi2Core-arm64 = atspi2CoreArm64Build;
            autoconf-arm64 = autoconfArm64Build;
            automake-arm64 = automakeArm64Build;
            cairo-arm64 = cairoArm64Build;
            cairoGobject-arm64 = cairoGobjectArm64Build;
            curl-arm64 = curlArm64Build;
            dbus-arm64 = dbusArm64Build;
            dillo-arm64 = dilloArm64Build;
            dmenu-arm64 = dmenuArm64Build;
            fastfetch-arm64 = fastfetchArm64Build;
            fastfetch-arm64-nogl = fastfetchNoGLArm64Build;
            vmprobe-arm64 = vmprobeArm64Build;
            fltk-arm64 = fltkArm64Build;
            foundation-arm64 = foundationArm64Build;
            fribidi-arm64 = fribidiArm64Build;
            gdkPixbuf-arm64 = gdkPixbufArm64Build;
            git-arm64 = gitArm64Build;
            gtk3-arm64 = gtk3Arm64Build;
            harfbuzz-arm64 = harfbuzzArm64Build;
            i3-arm64 = i3Arm64Build;
            ioreg-arm64 = ioregArm64Build;
            libXft-arm64 = libXftArm64Build;
            libcss-arm64 = libcssArm64Build;
            libcurlDylib-arm64 = libcurlDylibArm64Build;
            libcxxTest-arm64 = libcxxTestArm64Build;
            libdom-arm64 = libdomArm64Build;
            libepoxy-arm64 = libepoxyArm64Build;
            libfontenc-arm64 = libfontencArm64Build;
            libhubbub-arm64 = libhubbubArm64Build;
            libnsbmp-arm64 = libnsbmpArm64Build;
            libnsgif-arm64 = libnsgifArm64Build;
            libnsutils-arm64 = libnsutilsArm64Build;
            libparserutils-arm64 = libparserutilsArm64Build;
            libutf8proc-arm64 = libutf8procArm64Build;
            libwapcaplet-arm64 = libwapcapletArm64Build;
            libzDylib-arm64 = libzDylibArm64Build;
            mesa-arm64 = mesaArm64Build;
            mesaDemos-arm64 = mesaDemosArm64Build;
            migcomDarwin-arm64 = migcomDarwinArm64Build;
            netsurf-arm64 = netsurfArm64Build;
            openglFramework-arm64 = openglFrameworkArm64Build;
            openssh-arm64 = opensshArm64Build;
            pango-arm64 = pangoArm64Build;
            python-arm64 = pythonArm64Build;
            security-arm64 = securityArm64Build;
            systemConfiguration-arm64 = systemConfigurationArm64Build;
            diskArbitration = diskArbitrationBuild;
            symptomReporter = symptomReporterBuild;
            protocolBuffer = protocolBufferBuild;
            wirelessDiagnostics = wirelessDiagnosticsBuild;
            iokitcf-static = iokitCFStaticBuild;
            iomediacheck = iomediacheckBuild;
            startupNotification-arm64 = startupNotificationArm64Build;
            xcalc-arm64 = xcalcArm64Build;
            xcb-arm64 = xcbArm64Build;
            xcbCursor-arm64 = xcbCursorArm64Build;
            xcbImage-arm64 = xcbImageArm64Build;
            xcbKeysyms-arm64 = xcbKeysymsArm64Build;
            xcbRenderUtil-arm64 = xcbRenderUtilArm64Build;
            xcbUtil-arm64 = xcbUtilArm64Build;
            xcbWm-arm64 = xcbWmArm64Build;
            xcbXrm-arm64 = xcbXrmArm64Build;
            xclock-arm64 = xclockArm64Build;
            xeyes-arm64 = xeyesArm64Build;
            xkbcommon-arm64 = xkbcommonArm64Build;
            xkbcomp-arm64 = xkbcompArm64Build;
            xlib-arm64 = xlibArm64Build;
            xmessage-arm64 = xmessageArm64Build;
            xorg-arm64 = xorgArm64Build;
            xterm-arm64 = xtermArm64Build;
            xvfb-arm64 = xvfbArm64Build;
            xvfbLibICE-arm64 = xvfbLibICEArm64Build;
            xvfbLibSM-arm64 = xvfbLibSMArm64Build;
            xvfbLibXau-arm64 = xvfbLibXauArm64Build;
            xvfbLibXaw-arm64 = xvfbLibXawArm64Build;
            xvfbLibXcursor-arm64 = xvfbLibXcursorArm64Build;
            xvfbLibXdmcp-arm64 = xvfbLibXdmcpArm64Build;
            xvfbLibXext-arm64 = xvfbLibXextArm64Build;
            xvfbLibXfixes-arm64 = xvfbLibXfixesArm64Build;
            xvfbLibXfont2-arm64 = xvfbLibXfont2Arm64Build;
            xvfbLibXi-arm64 = xvfbLibXiArm64Build;
            xvfbLibXkbfile-arm64 = xvfbLibXkbfileArm64Build;
            xvfbLibXmu-arm64 = xvfbLibXmuArm64Build;
            xvfbLibXpm-arm64 = xvfbLibXpmArm64Build;
            xvfbLibXrandr-arm64 = xvfbLibXrandrArm64Build;
            xvfbLibXrender-arm64 = xvfbLibXrenderArm64Build;
            xvfbLibXt-arm64 = xvfbLibXtArm64Build;
            xvfbLibxcvt-arm64 = xvfbLibxcvtArm64Build;
            xvfbPixman-arm64 = xvfbPixmanArm64Build;
          };

          devShell = pkgs.mkShell ({
            packages = [
              iig
              pkgs.cmake
              pkgs.ninja
              pkgs.bison
              pkgs.flex
              pkgs.perl
              pkgs.bash
              pkgs.ed
              pkgs.unifdef
              pkgs.tcsh
              pkgs.pax
              pkgs.coreutils
              pkgs.findutils
              pkgs.gawk
              pkgs.gnused
              pkgs.clang
              pkgs.ruby
            ] ++ lib.optionals (!isDarwin) [
              darwinCrossToolchain
              nativeMigcom
              nativeUnifdef
              pkgs.libuuid
            ];
          } // lib.optionalAttrs (!isDarwin) {
            NIX_DARWIN_TOOLCHAIN_DIR = "${darwinCrossToolchain}/bin";
            NIX_NATIVE_LD_PATH = "${nativeLd}/bin/ld";
            NIX_HOST_CC_PATH = "${pkgs.clang}/bin/clang";
            NIX_MIGCOM_PATH = "${nativeMigcom}/bin/migcom";
            NIX_UNIFDEF_PATH = "${nativeUnifdef}/bin/unifdef";
            PUREDARWIN_TCC_SOURCE = "${pkgs.tinycc.src}";
            shellHook = ''
              export CMAKE_TOOLCHAIN_FILE="$PWD/cmake/nix-toolchain.cmake"
              echo "PureDarwin Nix kernel shell: cmake/nix-toolchain.cmake and cached native ld/migcom/unifdef are active."
            '';
          } // lib.optionalAttrs isDarwin {
            shellHook = ''
              echo "PureDarwin Darwin shell: using the native Apple host toolchain path."
            '';
          });
        in {
          packages = {
            apple-sdk = appleSdk;
            cg-screen-demo = cgScreenDemoBuild;
          } // commonPackages // arm64Packages // probePackages // linuxPackages;
          apps = lib.optionalAttrs (!isDarwin) linuxApps;
          devShells = {
            kernel = devShell;
            default = devShell;
          };
        };
    in {
      packages = forAllSystems (system: (mkSystem system).packages);
      apps = forAllSystems (system: (mkSystem system).apps);
      devShells = forAllSystems (system: (mkSystem system).devShells);
    };
}
