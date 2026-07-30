{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    iig-tools.url = "github:PureDarwin/iig-tools";
    kc-tools.url = "github:PureDarwin/kc-tools";
    xnu-loader.url = "github:PureDarwin/xnu-loader";
  };

  outputs = { self, nixpkgs, iig-tools, kc-tools, xnu-loader }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = lib.genAttrs systems;

      mkSystem = system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config.allowUnfreePredicate = pkg: lib.getName pkg == "MacOSX11.3.sdk.tar.xz";
          };

          isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
          # fbDOOM (GPL, opt-in - see src/Userspace/fbdoom/CMakeLists.txt) is
          # an external checkout, not a flake input: point PUREDARWIN_FBDOOM_SOURCE_ENV
          # at it (requires --impure). Mirrors the SDK tarball's requireFile
          # pattern - never hardcode a personal machine path into this file.
          fbdoomExternalSrcEnv = builtins.getEnv "PUREDARWIN_FBDOOM_SOURCE_ENV";
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
            patch -p1 < ${./src/Userspace/fbdoom/patches/midifile.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/midifile.h.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/mus2mid.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/opl.c.patch}
            patch -p1 < ${./src/Userspace/fbdoom/patches/opl_internal.h.patch}
          '';
          iig = iig-tools.packages.${system}.default or (
            (pkgs.callPackage iig-tools { }).overrideAttrs (old: {
              meta = (old.meta or { }) // {
                platforms = pkgs.lib.platforms.unix;
              };
            })
          );
          darwinCrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain.nix { };
          arm64CrossToolchain = if isDarwin then null else pkgs.callPackage ./nix/pkgs/toolchain.nix {
            target = "arm64-apple-darwin20.4";
            clangTarget = "arm64-apple-macosx11.0";
          };
          libtapi = if isDarwin then null else pkgs.callPackage ./nix/pkgs/libtapi.nix { };
          nativeLd =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/native-ld.nix {
              inherit darwinCrossToolchain libtapi iig;
            };
          nativeUnifdef = if isDarwin then null else pkgs.callPackage ./nix/pkgs/unifdef.nix { };
          nativeMigcom = if isDarwin then null else pkgs.callPackage ./nix/pkgs/migcom.nix { };
          libapfsrwBuild = pkgs.callPackage ./nix/pkgs/libapfsrw.nix { };

          sourceWith = name: prefixes:
            lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let
                  rel = lib.removePrefix "${toString ./.}/" (toString path);
                  isParentOfPrefix = prefix:
                    lib.hasPrefix "${rel}/" prefix;
                in
                  rel == "CMakeLists.txt"
                  || rel == "src/CMakeLists.txt"
                  || rel == "cmake"
                  || lib.hasPrefix "cmake/" rel
                  || lib.any (prefix:
                    rel == prefix
                    || lib.hasPrefix "${prefix}/" rel
                    || (type == "directory" && isParentOfPrefix prefix)
                  ) prefixes;
            };
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
            "src/Libraries"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "tools/mig"
            # libobjc needs the mach-o getsection helpers compiled into libSystem.
            "tools/cctools/libmacho/getsecbyname.c"
          ];
          libSystemSource = sourceWith "puredarwin-libsystem-source" libSystemSourcePaths;
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
          userlandSource = sourceWith "puredarwin-userland-source" [
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
          ];
          fbdoomSource = sourceWith "puredarwin-fbdoom-source" [
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
          ];
          cctoolsSource = sourceWith "puredarwin-cctools-source" [
            "src/Kernel/xnu/osfmk"
            "src/Libraries/IOKit"
            "src/Libraries/PDGOP"
            "src/Libraries/libSystem/libmalloc/compat-include"
            "src/Libraries/libSystem/libsystem_kernel/mach"
            "src/Libraries/libcxx/include"
            "src/Libraries/libSystem/corecrypto/include"
            "src/Libraries/CommonCrypto/include"
            "src/Libraries/CommonCrypto/libcn/pd_cc_digest_bridge.c"
            "src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c"
            "src/Libraries/libSystem/libc/string/FreeBSD/strmode.c"
            "src/Userspace"
            "tools"
          ];
          coreFoundationSource = sourceWith "puredarwin-corefoundation-source" [
            "src/Libraries/CoreFoundation"
            "src/Libraries/libSystem/libc/pd-compat-include"
          ];
          securitySource = sourceWith "puredarwin-security-source" [
            "src/Libraries/Security"
          ];
          # SystemConfiguration builds inside the same CMake project as libSystem
          # (it links -lSystem), so it needs libSystem's whole source set plus
          # its own directory.
          systemConfigurationSource = sourceWith "puredarwin-systemconfiguration-source"
            (libSystemSourcePaths ++ [
              "src/Libraries/SystemConfiguration"
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
            ]);
          iokitCFSource = sourceWith "puredarwin-iokitcf-source"
            (libSystemSourcePaths ++ [ "src/Kernel/xnu/iokit" ]);
          diskArbitrationSource = sourceWith "puredarwin-diskarbitration-source"
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
            ]);
          objcSource = sourceWith "puredarwin-objc-source" [
            "src/Libraries/objc4"
          ];
          libcxxDylibSource = sourceWith "puredarwin-libcxx-dylib-source" [
            "src/Libraries/libcxxabi"
            "src/Libraries/libcxx"
            "src/Libraries/libunwind"
          ];
          foundationSource = sourceWith "puredarwin-foundation-source" [
            "src/Libraries/Foundation"
          ];

          mkPureDarwinBuild = args: pkgs.callPackage ./build.nix ({
            inherit darwinCrossToolchain nativeLd nativeUnifdef nativeMigcom iig;
          } // args);

          userlandBuild = mkPureDarwinBuild {
            pname = "puredarwin-userland";
            src = userlandSource;
            buildTargets = [ "sw_vers" "ps" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "pd-networkd" "ping" "pcmplay" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" ]
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-pixman.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) pixman;
            };
          xvfbLibXauBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-stub-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-stub-lib.nix {
              inherit darwinCrossToolchain;
              name = "Xdmcp";
              version = pkgs.libxdmcp.version or "1.1.5";
              pcName = "xdmcp";
              pcDescription = "X Display Manager Control Protocol library";
              includeFrom = [ pkgs.libxdmcp pkgs.xorgproto ];
              source = ''
                int XdmcpWrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
                int XdmcpUnwrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
              '';
            };
          xvfbZlibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-zlib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib;
            };
          freetype2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-freetype.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib freetype;
            };
          libfontencBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libfontenc";
              version = pkgs.libfontenc.version;
              src = pkgs.libfontenc.src;
              deps = [ pkgs.xorgproto xvfbZlibBuild ];
            };
          xvfbLibXfont2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-util";
              version = pkgs.libxcb-util.version;
              src = pkgs.libxcb-util.src;
              deps = [ pkgs.xorgproto xcbBuild ];
            };
          xcbKeysymsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-keysyms";
              version = pkgs.libxcb-keysyms.version;
              src = pkgs.libxcb-keysyms.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbWmBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-wm";
              version = pkgs.libxcb-wm.version;
              src = pkgs.libxcb-wm.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
              nativeDeps = [ pkgs.m4 ];
            };
          xcbRenderUtilBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxcb-render-util";
              version = pkgs.libxcb-render-util.version;
              src = pkgs.libxcb-render-util.src;
              deps = [ pkgs.xorgproto xcbBuild xcbUtilBuild ];
            };
          xcbImageBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/yajl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) yajl;
            };
          startupNotificationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/cairo.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/glib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fontconfig.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fontconfig;
              freetype = freetype2Build;
              expat = expatBuild;
            };
          fribidiBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fribidi.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) fribidi;
            };
          harfbuzzBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/harfbuzz.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) harfbuzz;
              freetype = freetype2Build;
            };
          pangoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/pango.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/i3.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/i3status-shim.nix { };
          xvfbLibICEBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXext";
              version = pkgs.libXext.version;
              src = pkgs.libXext.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXauBuild ];
            };
          xvfbLibXmuBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXpm";
              version = pkgs.libXpm.version;
              src = pkgs.libXpm.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXawBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libxkbfile";
              version = pkgs.libxkbfile.version;
              src = pkgs.libxkbfile.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xkbcompBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-xkbcomp.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-fonts.nix { };
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libz-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) zlib;
            };
          libcurlDylibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libcurl-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              systemConfiguration = systemConfigurationBuild;
              zlib = xvfbZlibBuild;
              openssl = opensslBuild;
              inherit (pkgs) curl;
            };
          dbusBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/dbus.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              expat = expatBuild;
              libX11 = xlibBuild;
              inherit (pkgs) dbus meson ninja python3;
            };
          libxml2Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libxml2.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libxml2 meson ninja python3 git;
            };
          atspi2CoreBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/at-spi2-core.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libwapcaplet.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libwapcaplet;
            };
          libparserutilsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libparserutils.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconv = libiconvBuild;
              inherit (pkgs) libparserutils perl;
            };
          libnsutilsBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libnsutils.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsutils;
            };
          libnsgifBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libnsgif.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsgif;
            };
          libnsbmpBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libnsbmp.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libnsbmp;
            };
          libutf8procBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libutf8proc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libutf8proc;
            };
          libhubbubBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libhubbub.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              inherit (pkgs) libhubbub perl gperf gnused;
            };
          libcssBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libcss.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              inherit (pkgs) libcss perl python3;
            };
          libdomBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libdom.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libwapcaplet = libwapcapletBuild;
              libparserutils = libparserutilsBuild;
              libhubbub = libhubbubBuild;
              expat = expatBuild;
              inherit (pkgs) libdom;
            };
          netsurfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/netsurf.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libepoxy.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libX11 = xlibBuild;
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
                  -Wl,-install_name,/usr/lib/libpd_virgl_shim.dylib \
                  -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
                  repack/*.o -lSystem \
                  -o $out/usr/lib/libpd_virgl_shim.dylib
                cp src/Libraries/PDVirglShim/include/pd_virgl_shim.h $out/include/
                runHook postInstall
              '';
            });
          mesaBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              zlib = xvfbZlibBuild;
              expat = expatBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              pdVirglShim = pdVirglShimBuild;
              virglWinsysSrc = ./nix/pkgs/mesa/virgl-puredarwin;
              virglAbiHeader = ./src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h;
              inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
            };
          mesaDemosBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/mesa-demos.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              mesa = mesaBuild;
              libX11 = xlibBuild;
              libXext = xvfbLibXextBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              inherit (pkgs) meson ninja pkg-config xorgproto xtrans;
            };
          osmesaTriBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/osmesa-tri.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxDylib = libcxxDylibBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              mesa = mesaBuild;
            };
          hostOtoolBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/host-otool.nix { };
          nativeMesonToolsDir =
            if isDarwin then null else pkgs.runCommand "puredarwin-native-meson-tools" { } ''
              mkdir -p $out/bin
              ln -s ${hostOtoolBuild}/bin/otool $out/bin/otool
              ln -s ${hostOtoolBuild}/bin/install_name_tool $out/bin/install_name_tool
            '';
          libpngBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libpng.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              zlib = xvfbZlibBuild;
              inherit (pkgs) libpng;
            };
          cairoGobjectBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/cairo-gobject.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              cairo = cairoBuild;
              cairoReal = pkgs.cairo;
              glib = glibBuild;
            };
          gdkPixbufBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gdk-pixbuf.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gtk3.nix {
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
              inherit (pkgs) gtk3 xorgproto;
            };
          xvfbBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xvfb-libxcvt.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) libxcvt;
            };
          xorgBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg.nix {
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
            };
          xvfbLibXrenderBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXrender";
              version = pkgs.libXrender.version;
              src = pkgs.libXrender.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXfixesBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXfixes";
              version = pkgs.libXfixes.version;
              src = pkgs.libXfixes.src;
              deps = [ pkgs.xorgproto xlibBuild ];
            };
          xvfbLibXcursorBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
                    -mmacosx-version-min=11.0 \
                    -fuse-ld=${nativeLd}/bin/ld \
                    -nostdlib \
                    -dynamiclib \
                    -Wl,-install_name,/lib/libXcursor.1.dylib \
                    -Wl,-compatibility_version,1.0.0 \
                    -Wl,-current_version,1.0.2 \
                    -Wl,-undefined,dynamic_lookup \
                    -L${libSystemBuild}/usr/lib \
                    -o "$out/lib/libXcursor.1.dylib" \
                    ./*.o \
                    -lSystem
                )
                ln -sf libXcursor.1.dylib "$out/lib/libXcursor.dylib"
              '';
            };
          xvfbLibXrandrBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pname = "puredarwin-libXrandr";
              version = pkgs.libXrandr.version;
              src = pkgs.libXrandr.src;
              deps = [ pkgs.xorgproto xlibBuild xvfbLibXrenderBuild xvfbLibXextBuild ];
            };
          libXftBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/dmenu.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xorg-cross-lib.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xeyes.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xclock.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xcalc.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xmessage.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fltk.nix {
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
          dilloBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/dillo.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) dillo util-macros;
              fltk = fltkBuild;
              openssl = opensslBuild;
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/ncurses.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              ncurses = pkgs.ncurses;
            };
          libiconvBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libiconv.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libiconvReal = pkgs.libiconvReal;
            };
          toyboxBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/toybox.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              toybox = pkgs.toybox;
              zlib = xvfbZlibBuild;
            };
          nanoBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/nano.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              nano = pkgs.nano;
              ncurses = ncursesBuild;
            };
          xxdBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xxd.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              tinyxxd = pkgs.tinyxxd;
            };
          xzBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xz.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              xz = pkgs.xz;
            };
          bmakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/bmake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              bmake = pkgs.bmake;
            };
          gnumakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gnumake.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              gnumake = pkgs.gnumake;
            };
          pkgconfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/pkgconf.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              pkgconf = pkgs.pkgconf-unwrapped;
            };
          gnum4Build =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/gnum4.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              gnum4 = pkgs.gnum4;
            };
          autoconfBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/autoconf.nix {
              autoconf = pkgs.autoconf;
            };
          automakeBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/automake.nix {
              automake = pkgs.automake;
              # Host autoconf, not autoconfBuild: this only drives
              # automake's own build/test-generation on the Linux builder
              autoconf = pkgs.autoconf;
            };
          bisonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/bison.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              bison = pkgs.bison;
            };
          flexBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/flex.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              flex = pkgs.flex;
            };
          pythonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/python.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              python3 = pkgs.python3;
              zlib = xvfbZlibBuild;
              openssl = opensslBuild;
              libffi = libffiBuild;
            };
          perlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/perl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              perl = pkgs.perl;
              zlib = xvfbZlibBuild;
            };
          zshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/zsh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              zsh = pkgs.zsh;
              ncurses = ncursesBuild;
            };
          fileBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/file.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              file = pkgs.file;
              zlib = xvfbZlibBuild;
            };
          opensslBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/openssl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssl = pkgs.openssl;
            };
          curlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/curl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              systemConfiguration = systemConfigurationBuild;
              curl = pkgs.curl;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          opensshBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/openssh.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              openssh = pkgs.openssh;
              openssl = opensslBuild;
              zlib = xvfbZlibBuild;
            };
          gitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/git.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              git = pkgs.git;
              zlib = xvfbZlibBuild;
              curl = curlBuild;
              openssl = opensslBuild;
            };
          migcomDarwinBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/migcom-darwin.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
            };
          ioregBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/ioreg.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
            };
          xkbcommonBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xkbcommon.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libxcb = xcbBuild;
              libXau = xvfbLibXauBuild;
              libXdmcp = xvfbLibXdmcpBuild;
              xkeyboard-config = xkeyboardConfigBuild;
            };
          fastfetchBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/fastfetch.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              fastfetch = pkgs.fastfetch;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              openglFramework = openglFrameworkBuild;
              mesa = mesaBuild;
            };
          xtermBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/xterm.nix {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/icucore.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              icuSrc = pkgs.icu.src;
            };
          coreFoundationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/corefoundation.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              inherit (pkgs) icu;
              src = "${coreFoundationSource}/src/Libraries/CoreFoundation";
              pdCompatInclude = "${coreFoundationSource}/src/Libraries/libSystem/libc/pd-compat-include";
              libobjc = libobjcBuild;
              foundationSrc = "${foundationSource}/src/Libraries/Foundation";
            };
          libcxxabiDylibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libcxxabi-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              src = libcxxDylibSource;
            };
          libcxxDylibBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libcxx-dylib.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              src = libcxxDylibSource;
            };
          libcxxTestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libcxx-test.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              libcxxDylib = libcxxDylibBuild;
            };
          libobjcBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/libobjc.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libcxxabiDylib = libcxxabiDylibBuild;
              src = objcSource;
            };
          objcTestBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/objc-test.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
            };
          foundationBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/foundation.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              libobjc = libobjcBuild;
              corefoundation = coreFoundationBuild;
              src = "${foundationSource}/src/Libraries/Foundation";
            };
          iokitBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/iokit.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokitCFStatic = iokitCFStaticBuild;
            };
          openglFrameworkBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/opengl-framework.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              mesa = mesaBuild;
              src = ./src/Libraries/OpenGL;
            };
          # arm64 twins of the remaining image packages and their dependency
          # closure, generated from the x86 wiring: toolchain/triple/libSystem
          # come from mkArm64Build, and each package's own PureDarwin deps are
          # pointed at their arm64 builds.

          atspi2CoreArm64Build = mkArm64Build ./nix/pkgs/at-spi2-core.nix {
              nativeMesonTools = nativeMesonToolsDir;
              glib = glibArm64Build;
              libxml2 = libxml2Arm64Build;
              dbus = dbusArm64Build;
              pcre2 = pcre2Arm64Build;
              libffi = libffiArm64Build;
              zlib = xvfbZlibArm64Build;
              libiconv = libiconvArm64Build;
              inherit (pkgs) at-spi2-core meson ninja python3;
          };
          autoconfArm64Build = mkArm64Build ./nix/pkgs/autoconf.nix {
              autoconf = pkgs.autoconf;
          };
          automakeArm64Build = mkArm64Build ./nix/pkgs/automake.nix {
              automake = pkgs.automake;
              # Host autoconf, not autoconfArm64Build: this only drives
              # automake's own build/test-generation on the Linux builder
              autoconf = pkgs.autoconf;
          };
          cairoArm64Build = mkArm64Build ./nix/pkgs/cairo.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit (pkgs) cairo xorgproto;
              pixman = xvfbPixmanArm64Build;
              zlib = xvfbZlibArm64Build;
              libX11 = xlibArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              freetype = freetype2Arm64Build;
              fontconfig = fontconfigArm64Build;
              expat = expatArm64Build;
              libpng = libpngArm64Build;
          };
          cairoGobjectArm64Build = mkArm64Build ./nix/pkgs/cairo-gobject.nix {
              cairo = cairoArm64Build;
              cairoReal = pkgs.cairo;
              glib = glibArm64Build;
          };
          curlArm64Build = mkArm64Build ./nix/pkgs/curl.nix {
              curl = pkgs.curl;
              openssl = opensslArm64Build;
              zlib = xvfbZlibArm64Build;
              corefoundation = coreFoundationArm64Build;
              systemConfiguration = systemConfigurationArm64Build;
          };
          dbusArm64Build = mkArm64Build ./nix/pkgs/dbus.nix {
              expat = expatArm64Build;
              libX11 = xlibArm64Build;
              inherit (pkgs) dbus meson ninja python3;
          };
          dilloArm64Build = mkArm64Build ./nix/pkgs/dillo.nix {
              inherit (pkgs) dillo util-macros;
              fltk = fltkArm64Build;
              openssl = opensslArm64Build;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXfixes = xvfbLibXfixesArm64Build;
              libXft = libXftArm64Build;
              libxcbcursor = xcbCursorArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              fontconfig = fontconfigArm64Build;
              freetype2 = freetype2Arm64Build;
              expat = expatArm64Build;
              inherit (pkgs) xorgproto;
          };
          dmenuArm64Build = mkArm64Build ./nix/pkgs/dmenu.nix {
              inherit (pkgs) dmenu;
              inherit (pkgs) xorgproto;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXft = libXftArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              freetype2 = freetype2Arm64Build;
              fontconfig = fontconfigArm64Build;
              expat = expatArm64Build;
          };
          fastfetchArm64Build = mkArm64Build ./nix/pkgs/fastfetch.nix {
              fastfetch = pkgs.fastfetch;
              corefoundation = coreFoundationArm64Build;
              iokit = iokitArm64Build;
              openglFramework = openglFrameworkArm64Build;
              mesa = mesaArm64Build;
          };
          fltkArm64Build = mkArm64Build ./nix/pkgs/fltk.nix {
              inherit (pkgs) fltk_1_3 util-macros;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXfixes = xvfbLibXfixesArm64Build;
              libXft = libXftArm64Build;
              libxcbcursor = xcbCursorArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              fontconfig = fontconfigArm64Build;
              freetype2 = freetype2Arm64Build;
              expat = expatArm64Build;
              inherit (pkgs) xorgproto;
          };
          foundationArm64Build = mkArm64Build ./nix/pkgs/foundation.nix {
              libobjc = libobjcArm64Build;
              corefoundation = coreFoundationArm64Build;
              src = "${foundationSource}/src/Libraries/Foundation";
          };
          fribidiArm64Build = mkArm64Build ./nix/pkgs/fribidi.nix {
              inherit (pkgs) fribidi;
          };
          gdkPixbufArm64Build = mkArm64Build ./nix/pkgs/gdk-pixbuf.nix {
              nativeMesonTools = nativeMesonToolsDir;
              glib = glibArm64Build;
              pcre2 = pcre2Arm64Build;
              libffi = libffiArm64Build;
              zlib = xvfbZlibArm64Build;
              libiconv = libiconvArm64Build;
              libpng = libpngArm64Build;
              inherit (pkgs) gdk-pixbuf meson ninja python3;
          };
          gitArm64Build = mkArm64Build ./nix/pkgs/git.nix {
              git = pkgs.git;
              zlib = xvfbZlibArm64Build;
              curl = curlArm64Build;
              openssl = opensslArm64Build;
          };
          gtk3Arm64Build = mkArm64Build ./nix/pkgs/gtk3.nix {
              nativeMesonTools = nativeMesonToolsDir;
              glib = glibArm64Build;
              pcre2 = pcre2Arm64Build;
              libffi = libffiArm64Build;
              zlib = xvfbZlibArm64Build;
              libiconv = libiconvArm64Build;
              cairo = cairoArm64Build;
              cairoGobject = cairoGobjectArm64Build;
              pixman = xvfbPixmanArm64Build;
              pango = pangoArm64Build;
              fribidi = fribidiArm64Build;
              harfbuzz = harfbuzzArm64Build;
              freetype2 = freetype2Arm64Build;
              fontconfig = fontconfigArm64Build;
              expat = expatArm64Build;
              gdkPixbuf = gdkPixbufArm64Build;
              libepoxy = libepoxyArm64Build;
              atspi2Core = atspi2CoreArm64Build;
              dbus = dbusArm64Build;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXi = xvfbLibXiArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXrandr = xvfbLibXrandrArm64Build;
              libXfixes = xvfbLibXfixesArm64Build;
              libXcursor = xvfbLibXcursorArm64Build;
              libpng = libpngArm64Build;
              glibNative = pkgs.glib.dev;
              inherit (pkgs) gtk3 xorgproto;
          };
          harfbuzzArm64Build = mkArm64Build ./nix/pkgs/harfbuzz.nix {
              inherit (pkgs) harfbuzz;
              freetype = freetype2Arm64Build;
          };
          i3Arm64Build = mkArm64Build ./nix/pkgs/i3.nix {
              inherit (pkgs) i3;
              inherit (pkgs) xorgproto;
              startup-notification = startupNotificationArm64Build;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libxcb-util = xcbUtilArm64Build;
              libxcb-keysyms = xcbKeysymsArm64Build;
              libxcb-wm = xcbWmArm64Build;
              libxcb-render-util = xcbRenderUtilArm64Build;
              libxcb-image = xcbImageArm64Build;
              libxcb-cursor = xcbCursorArm64Build;
              xcb-util-xrm = xcbXrmArm64Build;
              xkbcommon = xkbcommonArm64Build;
              yajl = yajlArm64Build;
              pcre2 = pcre2Arm64Build;
              cairo = cairoArm64Build;
              pango = pangoArm64Build;
              glib = glibArm64Build;
              fribidi = fribidiArm64Build;
              harfbuzz = harfbuzzArm64Build;
              libev = libevArm64Build;
              libiconv = libiconvArm64Build;
              zlib = xvfbZlibArm64Build;
              libffi = libffiArm64Build;
              pixman = xvfbPixmanArm64Build;
              fontconfig = fontconfigArm64Build;
              freetype = freetype2Arm64Build;
              expat = expatArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libpng = libpngArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
          };
          ioregArm64Build = mkArm64Build ./nix/pkgs/ioreg.nix {
              corefoundation = coreFoundationArm64Build;
              iokit = iokitArm64Build;
          };
          libXftArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXft";
              version = pkgs.libXft.version;
              src = pkgs.libXft.src;
              deps = [
                pkgs.xorgproto
                xlibArm64Build
                xvfbLibXrenderArm64Build
                freetype2Arm64Build
                fontconfigArm64Build
                expatArm64Build
              ];
              nativeDeps = [ pkgs.util-macros ];
          };
          libcssArm64Build = mkArm64Build ./nix/pkgs/libcss.nix {
              libwapcaplet = libwapcapletArm64Build;
              libparserutils = libparserutilsArm64Build;
              inherit (pkgs) libcss perl python3;
          };
          libcurlDylibArm64Build = mkArm64Build ./nix/pkgs/libcurl-dylib.nix {
              zlib = xvfbZlibArm64Build;
              openssl = opensslArm64Build;
              corefoundation = coreFoundationArm64Build;
              systemConfiguration = systemConfigurationArm64Build;
              inherit (pkgs) curl;
          };
          libcxxTestArm64Build = mkArm64Build ./nix/pkgs/libcxx-test.nix {
              libcxxabiDylib = libcxxabiDylibArm64Build;
              libcxxDylib = libcxxDylibArm64Build;
          };
          libdomArm64Build = mkArm64Build ./nix/pkgs/libdom.nix {
              libwapcaplet = libwapcapletArm64Build;
              libparserutils = libparserutilsArm64Build;
              libhubbub = libhubbubArm64Build;
              expat = expatArm64Build;
              inherit (pkgs) libdom;
          };
          libepoxyArm64Build = mkArm64Build ./nix/pkgs/libepoxy.nix {
              nativeMesonTools = nativeMesonToolsDir;
              libX11 = xlibArm64Build;
              inherit (pkgs) libepoxy xorgproto meson ninja python3;
          };
          libfontencArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libfontenc";
              version = pkgs.libfontenc.version;
              src = pkgs.libfontenc.src;
              deps = [ pkgs.xorgproto xvfbZlibArm64Build ];
          };
          libhubbubArm64Build = mkArm64Build ./nix/pkgs/libhubbub.nix {
              libwapcaplet = libwapcapletArm64Build;
              libparserutils = libparserutilsArm64Build;
              inherit (pkgs) libhubbub perl gperf gnused;
          };
          libnsbmpArm64Build = mkArm64Build ./nix/pkgs/libnsbmp.nix {
              inherit (pkgs) libnsbmp;
          };
          libnsgifArm64Build = mkArm64Build ./nix/pkgs/libnsgif.nix {
              inherit (pkgs) libnsgif;
          };
          libnsutilsArm64Build = mkArm64Build ./nix/pkgs/libnsutils.nix {
              inherit (pkgs) libnsutils;
          };
          libparserutilsArm64Build = mkArm64Build ./nix/pkgs/libparserutils.nix {
              libiconv = libiconvArm64Build;
              inherit (pkgs) libparserutils perl;
          };
          libutf8procArm64Build = mkArm64Build ./nix/pkgs/libutf8proc.nix {
              inherit (pkgs) libutf8proc;
          };
          libwapcapletArm64Build = mkArm64Build ./nix/pkgs/libwapcaplet.nix {
              inherit (pkgs) libwapcaplet;
          };
          libzDylibArm64Build = mkArm64Build ./nix/pkgs/libz-dylib.nix {
              inherit (pkgs) zlib;
          };
          mesaArm64Build = mkArm64Build ./nix/pkgs/mesa.nix {
              nativeMesonTools = nativeMesonToolsDir;
              libcxxDylib = libcxxDylibArm64Build;
              libcxxabiDylib = libcxxabiDylibArm64Build;
              zlib = xvfbZlibArm64Build;
              expat = expatArm64Build;
              libX11 = xlibArm64Build;
              libXext = xvfbLibXextArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              pdVirglShim = pdVirglShimArm64Build;
              virglWinsysSrc = ./nix/pkgs/mesa/virgl-puredarwin;
              virglAbiHeader = ./src/Kernel/Extensions/IOVirtIOGPU/IOVirtIOGPU3DShared.h;
              inherit (pkgs) meson ninja pkg-config python3 bison flex xorgproto xtrans;
          };
          mesaDemosArm64Build = mkArm64Build ./nix/pkgs/mesa-demos.nix {
              nativeMesonTools = nativeMesonToolsDir;
              mesa = mesaArm64Build;
              libX11 = xlibArm64Build;
              libXext = xvfbLibXextArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              inherit (pkgs) meson ninja pkg-config xorgproto xtrans;
          };
          migcomDarwinArm64Build = mkArm64Build ./nix/pkgs/migcom-darwin.nix {
          };
          netsurfArm64Build = mkArm64Build ./nix/pkgs/netsurf.nix {
              hostOtool = hostOtoolArm64Build;
              glibNative = pkgs.glib.dev;
              gdkPixbufNative = pkgs.gdk-pixbuf.dev;
              inherit (pkgs) inetutils;
              gtk3 = gtk3Arm64Build;
              glib = glibArm64Build;
              cairo = cairoArm64Build;
              cairoGobject = cairoGobjectArm64Build;
              pango = pangoArm64Build;
              gdkPixbuf = gdkPixbufArm64Build;
              libepoxy = libepoxyArm64Build;
              atspi2Core = atspi2CoreArm64Build;
              dbus = dbusArm64Build;
              libcurl = libcurlDylibArm64Build;
              openssl = opensslArm64Build;
              zlib = xvfbZlibArm64Build;
              libpng = libpngArm64Build;
              libiconv = libiconvArm64Build;
              libwapcaplet = libwapcapletArm64Build;
              libparserutils = libparserutilsArm64Build;
              libhubbub = libhubbubArm64Build;
              libcss = libcssArm64Build;
              libdom = libdomArm64Build;
              libnsgif = libnsgifArm64Build;
              libnsbmp = libnsbmpArm64Build;
              libnsutils = libnsutilsArm64Build;
              libutf8proc = libutf8procArm64Build;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXi = xvfbLibXiArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXrandr = xvfbLibXrandrArm64Build;
              libXfixes = xvfbLibXfixesArm64Build;
              libXcursor = xvfbLibXcursorArm64Build;
              xorgproto = pkgs.xorgproto;
              expat = expatArm64Build;
              pcre2 = pcre2Arm64Build;
              libffi = libffiArm64Build;
              fribidi = fribidiArm64Build;
              harfbuzz = harfbuzzArm64Build;
              freetype2 = freetype2Arm64Build;
              fontconfig = fontconfigArm64Build;
              inherit (pkgs) perl pkg-config nsgenbind;
          };
          openglFrameworkArm64Build = mkArm64Build ./nix/pkgs/opengl-framework.nix {
              mesa = mesaArm64Build;
              src = ./src/Libraries/OpenGL;
          };
          opensshArm64Build = mkArm64Build ./nix/pkgs/openssh.nix {
              openssh = pkgs.openssh;
              openssl = opensslArm64Build;
              zlib = xvfbZlibArm64Build;
          };
          osmesaTriArm64Build = mkArm64Build ./nix/pkgs/osmesa-tri.nix {
              libcxxDylib = libcxxDylibArm64Build;
              libcxxabiDylib = libcxxabiDylibArm64Build;
              mesa = mesaArm64Build;
          };
          pangoArm64Build = mkArm64Build ./nix/pkgs/pango.nix {
              nativeMesonTools = nativeMesonToolsDir;
              inherit (pkgs) pango;
              glib = glibArm64Build;
              fribidi = fribidiArm64Build;
              harfbuzz = harfbuzzArm64Build;
              cairo = cairoArm64Build;
              pcre2 = pcre2Arm64Build;
              libffi = libffiArm64Build;
              zlib = xvfbZlibArm64Build;
              libiconv = libiconvArm64Build;
              pixman = xvfbPixmanArm64Build;
              libxcb = xcbArm64Build;
              fontconfig = fontconfigArm64Build;
              freetype = freetype2Arm64Build;
              expat = expatArm64Build;
              libX11 = xlibArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              inherit (pkgs) xorgproto;
              libpng = libpngArm64Build;
          };
          pythonArm64Build = mkArm64Build ./nix/pkgs/python.nix {
              python3 = pkgs.python3;
              zlib = xvfbZlibArm64Build;
              openssl = opensslArm64Build;
              libffi = libffiArm64Build;
          };
          securityArm64Build = mkArm64Build ./nix/pkgs/security.nix {
              corefoundation = coreFoundationArm64Build;
              src = "${securitySource}/src/Libraries/Security";
          };
          systemConfigurationArm64Build =
            let base = mkSystemConfigurationBuild {
              corefoundation = coreFoundationArm64Build;
              libobjc = libobjcArm64Build;
              security = securityArm64Build;
            };
            in if base == null then null else base.override {
              puredarwinArch = "arm64";
              inherit arm64CrossToolchain;
            };
          startupNotificationArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-startup-notification";
              version = pkgs.libstartup_notification.version;
              src = pkgs.libstartup_notification.src;
              deps = [ pkgs.xorgproto xlibArm64Build xcbArm64Build xcbUtilArm64Build ];
              configureFlags = [
                "--x-includes=${lib.getDev xlibArm64Build}/include"
                "--x-libraries=${xlibArm64Build}/lib"
              ];
              preConfigureExtra = ''
                export lf_cv_sane_realloc=yes
              '';
              postPatchExtra = ''
                sed -i 's/^SUBDIRS=libsn test doc/SUBDIRS=libsn/' Makefile.in
              '';
          };
          xcalcArm64Build = mkArm64Build ./nix/pkgs/xcalc.nix {
              xcalc = pkgs.xcalc;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXmu = xvfbLibXmuArm64Build;
              libXt = xvfbLibXtArm64Build;
              libXaw = xvfbLibXawArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              inherit (pkgs) xorgproto;
          };
          xcbArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb";
              version = pkgs.libxcb.version;
              src = pkgs.libxcb.src;
              deps = [
                pkgs.xorgproto
                xvfbLibXauArm64Build
                xvfbLibXdmcpArm64Build
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
          xcbCursorArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-cursor";
              version = pkgs.libxcb-cursor.version;
              src = pkgs.libxcb-cursor.src;
              deps = [
                pkgs.xorgproto
                xcbArm64Build
                xcbUtilArm64Build
                xcbKeysymsArm64Build
                xcbImageArm64Build
                xcbRenderUtilArm64Build
              ];
              nativeDeps = [ pkgs.m4 ];
          };
          xcbImageArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-image";
              version = pkgs.libxcb-image.version;
              src = pkgs.libxcb-image.src;
              deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build xcbRenderUtilArm64Build ];
              postPatchExtra = ''
                sed -i 's/^SUBDIRS = image test/SUBDIRS = image/' Makefile.in
              '';
          };
          xcbKeysymsArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-keysyms";
              version = pkgs.libxcb-keysyms.version;
              src = pkgs.libxcb-keysyms.src;
              deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
          };
          xcbRenderUtilArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-render-util";
              version = pkgs.libxcb-render-util.version;
              src = pkgs.libxcb-render-util.src;
              deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
          };
          xcbUtilArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-util";
              version = pkgs.libxcb-util.version;
              src = pkgs.libxcb-util.src;
              deps = [ pkgs.xorgproto xcbArm64Build ];
          };
          xcbWmArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxcb-wm";
              version = pkgs.libxcb-wm.version;
              src = pkgs.libxcb-wm.src;
              deps = [ pkgs.xorgproto xcbArm64Build xcbUtilArm64Build ];
              nativeDeps = [ pkgs.m4 ];
          };
          xcbXrmArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-xcb-util-xrm";
              version = pkgs.xcbutilxrm.version;
              src = pkgs.xcbutilxrm.src;
              deps = [ pkgs.xorgproto xlibArm64Build xcbArm64Build xcbUtilArm64Build ];
              nativeDeps = [ pkgs.m4 pkgs.util-macros ];
              configureFlags = [
                "--disable-devel-docs"
              ];
          };
          xclockArm64Build = mkArm64Build ./nix/pkgs/xclock.nix {
              xclock = pkgs.xclock;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXmu = xvfbLibXmuArm64Build;
              libXt = xvfbLibXtArm64Build;
              libXaw = xvfbLibXawArm64Build;
              libXft = libXftArm64Build;
              libxkbfile = xvfbLibXkbfileArm64Build;
              freetype2 = freetype2Arm64Build;
              fontconfig = fontconfigArm64Build;
              expat = expatArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              inherit (pkgs) xorgproto;
          };
          xeyesArm64Build = mkArm64Build ./nix/pkgs/xeyes.nix {
              xeyes = pkgs.xeyes;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXi = xvfbLibXiArm64Build;
              libXrender = xvfbLibXrenderArm64Build;
              libXfixes = xvfbLibXfixesArm64Build;
              libXmu = xvfbLibXmuArm64Build;
              libXt = xvfbLibXtArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              inherit (pkgs) xorgproto;
          };
          xkbcommonArm64Build = mkArm64Build ./nix/pkgs/xkbcommon.nix {
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              xkeyboard-config = xkeyboardConfigArm64Build;
          };
          xkbcompArm64Build = mkArm64Build ./nix/pkgs/xvfb-xkbcomp.nix {
              inherit (pkgs) xkbcomp xorgproto;
              libX11 = xlibArm64Build;
              libxkbfile = xvfbLibXkbfileArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libxcb = xcbArm64Build;
          };
          xlibArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libX11";
              version = pkgs.libX11.version;
              src = pkgs.libX11.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xcbArm64Build
                xvfbLibXauArm64Build
                xvfbLibXdmcpArm64Build
              ];
              configureFlags = [
                "--disable-specs"
                "--enable-xlocaledir"
              ];
          };
          xmessageArm64Build = mkArm64Build ./nix/pkgs/xmessage.nix {
              xmessage = pkgs.xmessage;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXmu = xvfbLibXmuArm64Build;
              libXt = xvfbLibXtArm64Build;
              libXaw = xvfbLibXawArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              inherit (pkgs) xorgproto;
          };
          xorgArm64Build = mkArm64Build ./nix/pkgs/xorg.nix {
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXfont2 = xvfbLibXfont2Arm64Build;
              zlib = xvfbZlibArm64Build;
              freetype2 = freetype2Arm64Build;
              libfontenc = libfontencArm64Build;
              xvfbZlib = xvfbZlibArm64Build;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileArm64Build;
              libXdmcp = pkgs.libxdmcp;
              libxcvt = xvfbLibxcvtArm64Build;
          };
          xtermArm64Build = mkArm64Build ./nix/pkgs/xterm.nix {
              xterm = pkgs.xterm;
              libX11 = xlibArm64Build;
              libxcb = xcbArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXdmcp = xvfbLibXdmcpArm64Build;
              libICE = xvfbLibICEArm64Build;
              libSM = xvfbLibSMArm64Build;
              libXt = xvfbLibXtArm64Build;
              libXext = xvfbLibXextArm64Build;
              libXmu = xvfbLibXmuArm64Build;
              libXpm = xvfbLibXpmArm64Build;
              libXaw = xvfbLibXawArm64Build;
              inherit (pkgs) xorgproto;
            ncurses = ncursesArm64Build;
          };
          xvfbArm64Build = mkArm64Build ./nix/pkgs/xvfb.nix {
              xorg-server = pkgs.xorg-server;
              pixman = xvfbPixmanArm64Build;
              libXau = xvfbLibXauArm64Build;
              libXfont2 = xvfbLibXfont2Arm64Build;
              zlib = xvfbZlibArm64Build;
              freetype2 = freetype2Arm64Build;
              libfontenc = libfontencArm64Build;
              xvfbZlib = xvfbZlibArm64Build;
              inherit (pkgs) xorgproto xtrans;
              libxkbfile = xvfbLibXkbfileArm64Build;
              libXdmcp = pkgs.libxdmcp;
          };
          xvfbLibICEArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libICE";
              version = pkgs.libICE.version;
              src = pkgs.libICE.src;
              deps = [ pkgs.xorgproto pkgs.xtrans ];
              preConfigureExtra = ''
                export ac_cv_func_arc4random_buf=yes
              '';
          };
          xvfbLibSMArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libSM";
              version = pkgs.libSM.version;
              src = pkgs.libSM.src;
              deps = [ pkgs.xorgproto pkgs.xtrans xvfbLibICEArm64Build ];
              configureFlags = [
                "--without-libuuid"
              ];
          };
          xvfbLibXauArm64Build = mkArm64Build ./nix/pkgs/xvfb-stub-lib.nix {
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
          xvfbLibXawArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXaw";
              version = pkgs.libXaw.version;
              src = pkgs.libXaw.src;
              deps = [
                pkgs.xorgproto
                xlibArm64Build
                xvfbLibXextArm64Build
                xvfbLibXmuArm64Build
                xvfbLibXpmArm64Build
                xvfbLibXtArm64Build
                xvfbLibSMArm64Build
                xvfbLibICEArm64Build
              ];
              preConfigureExtra = ''
                export CFLAGS="$CFLAGS -include limits.h"
              '';
              postInstallExtra = ''
                ln -sf libXaw7.a $out/lib/libXaw.a
              '';
          };
          xvfbLibXcursorArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXcursor";
              version = pkgs.libXcursor.version;
              src = pkgs.libXcursor.src;
              deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXfixesArm64Build xvfbLibXrenderArm64Build ];
              postInstallExtra = ''
                mkdir -p .libXcursor-dylib
                (
                  cd .libXcursor-dylib
                  ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar x "$out/lib/libXcursor.a"
                  ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
                    -isysroot "$DARWIN_SDK_ROOT" \
                    -mmacosx-version-min=11.0 \
                    -fuse-ld=${nativeLd}/bin/ld \
                    -nostdlib \
                    -dynamiclib \
                    -Wl,-install_name,/lib/libXcursor.1.dylib \
                    -Wl,-compatibility_version,1.0.0 \
                    -Wl,-current_version,1.0.2 \
                    -Wl,-undefined,dynamic_lookup \
                    -L${libSystemArm64Build}/usr/lib \
                    -o "$out/lib/libXcursor.1.dylib" \
                    ./*.o \
                    -lSystem
                )
                ln -sf libXcursor.1.dylib "$out/lib/libXcursor.dylib"
              '';
          };
          xvfbLibXdmcpArm64Build = mkArm64Build ./nix/pkgs/xvfb-stub-lib.nix {
              name = "Xdmcp";
              version = pkgs.libxdmcp.version or "1.1.5";
              pcName = "xdmcp";
              pcDescription = "X Display Manager Control Protocol library";
              includeFrom = [ pkgs.libxdmcp pkgs.xorgproto ];
              source = ''
                int XdmcpWrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
                int XdmcpUnwrap(const unsigned char *input, unsigned char *wrapper, const unsigned char *key) { (void)input; (void)wrapper; (void)key; return 0; }
              '';
          };
          xvfbLibXextArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXext";
              version = pkgs.libXext.version;
              src = pkgs.libXext.src;
              deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXauArm64Build ];
          };
          xvfbLibXfixesArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXfixes";
              version = pkgs.libXfixes.version;
              src = pkgs.libXfixes.src;
              deps = [ pkgs.xorgproto xlibArm64Build ];
          };
          xvfbLibXfont2Arm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXfont2";
              version = pkgs.libxfont_2.version;
              src = pkgs.libxfont_2.src;
              deps = [
                pkgs.xorgproto
                pkgs.xtrans
                xvfbZlibArm64Build
                freetype2Arm64Build
                libfontencArm64Build
              ];
              configureFlags = [
                "--disable-devel-docs"
              ];
          };
          xvfbLibXiArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXi";
              version = pkgs.libXi.version;
              src = pkgs.libXi.src;
              deps = [
                pkgs.xorgproto
                xlibArm64Build
                xvfbLibXextArm64Build
                xvfbLibXfixesArm64Build
              ];
              configureFlags = [
                "--disable-malloc0returnsnull"
              ];
          };
          xvfbLibXkbfileArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libxkbfile";
              version = pkgs.libxkbfile.version;
              src = pkgs.libxkbfile.src;
              deps = [ pkgs.xorgproto xlibArm64Build ];
          };
          xvfbLibXmuArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXmu";
              version = pkgs.libXmu.version;
              src = pkgs.libXmu.src;
              deps = [
                pkgs.xorgproto
                xlibArm64Build
                xvfbLibXextArm64Build
                xvfbLibXtArm64Build
                xvfbLibSMArm64Build
                xvfbLibICEArm64Build
              ];
          };
          xvfbLibXpmArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXpm";
              version = pkgs.libXpm.version;
              src = pkgs.libXpm.src;
              deps = [ pkgs.xorgproto xlibArm64Build ];
          };
          xvfbLibXrandrArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXrandr";
              version = pkgs.libXrandr.version;
              src = pkgs.libXrandr.src;
              deps = [ pkgs.xorgproto xlibArm64Build xvfbLibXrenderArm64Build xvfbLibXextArm64Build ];
          };
          xvfbLibXrenderArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXrender";
              version = pkgs.libXrender.version;
              src = pkgs.libXrender.src;
              deps = [ pkgs.xorgproto xlibArm64Build ];
          };
          xvfbLibXtArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
              pname = "puredarwin-libXt";
              version = pkgs.libXt.version;
              src = pkgs.libXt.src;
              deps = [
                pkgs.xorgproto
                xlibArm64Build
                xvfbLibICEArm64Build
                xvfbLibSMArm64Build
              ];
          };
          xvfbLibxcvtArm64Build = mkArm64Build ./nix/pkgs/xvfb-libxcvt.nix {
              inherit (pkgs) libxcvt;
          };
          xvfbPixmanArm64Build = mkArm64Build ./nix/pkgs/xvfb-pixman.nix {
              inherit (pkgs) pixman;
          };
          pdVirglShimArm64Build =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-pd-virgl-shim-arm64";
              src = userlandSource;
              buildTargets = [ "pd_virgl_shim" ];
              enableProjects = false;
              enableKernel = false;
              enableLibraries = false;
              installUserland = false;
              installKernel = false;
              prebuiltLibSystem = libSystemArm64Build;
              puredarwinArch = "arm64";
              inherit arm64CrossToolchain;
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p $out/usr/lib $out/include
                ar=${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-ar
                mkdir -p repack && ( cd repack && \
                  "$ar" x ../build-nix/src/Userspace/pd-virgl-shim/libpd_virgl_shim.a )
                ${arm64CrossToolchain}/bin/arm64-apple-darwin20.4-clang \
                  -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
                  -L${libSystemArm64Build}/usr/lib \
                  -Wl,-install_name,/usr/lib/libpd_virgl_shim.dylib \
                  -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains \
                  repack/*.o -lSystem \
                  -o $out/usr/lib/libpd_virgl_shim.dylib
                cp src/Libraries/PDVirglShim/include/pd_virgl_shim.h $out/include/
                runHook postInstall
              '';
            });
          # Arch-independent: a host-side tool and a pure data package, so the
          # x86 builds are reused rather than duplicated.
          hostOtoolArm64Build = hostOtoolBuild;
          xkeyboardConfigArm64Build = xkeyboardConfigBuild;

          osmesaFbArm64Build =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-osmesa-fb-arm64";
              src = fbdoomSource;
              buildTargets = [ "osmesa-fb" ];
              enableProjects = false;
              enableKernel = false;
              installUserland = false;
              installKernel = false;
              puredarwinArch = "arm64";
              inherit arm64CrossToolchain;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_OSMESA_FB=ON"
                "-DPUREDARWIN_OSMESA_PREFIX=${mesaArm64Build}/usr"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p $out/usr/bin
                cp build-nix/src/Userspace/osmesa-fb/osmesa-fb $out/usr/bin/osmesa-fb
                runHook postInstall
              '';
            });
          # Arch-independent: X11 locale data, fonts, and a stdenvNoCC shim, so
          # the x86 builds are reused rather than duplicated.
          xlibLocaleArm64Build = xlibLocaleBuild;
          xvfbFontsArm64Build = xvfbFontsBuild;
          i3statusShimArm64Build = i3statusShimBuild;

          securityBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/security.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              src = "${securitySource}/src/Libraries/Security";
            };
          mkSystemConfigurationBuild = { corefoundation, libobjc, security }:
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-systemconfiguration";
              src = systemConfigurationSource;
              buildTargets = [ "SystemConfiguration" "configd" ];
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
                ln -s A "$fw/Versions/Current"
                ln -s Versions/Current/SystemConfiguration "$fw/SystemConfiguration"
                ln -s Versions/Current/Headers "$fw/Headers"
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
                for p in PreferencesMonitor LinkConfiguration KernelEventMonitor IPMonitor; do
                  d="$out/System/Library/SystemConfiguration/$p.bundle/Contents"
                  mkdir -p "$d"
                  cp "src/Libraries/SystemConfiguration/Plugins/$p/Info.plist" "$d/"
                done
                runHook postInstall
              '';
            });
          # IOKitUser's CoreFoundation-based IOKitLib. Built apart from libSystem
          # because it links CoreFoundation, which itself links libSystem.
          iokitCFStaticBuild =
            if isDarwin then null else (mkPureDarwinBuild {
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
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/iomediacheck.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              iokitHeaders = iokitCFStaticBuild;
            };
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
            };
          systemStarterBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/systemstarter.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          launchctlBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/launchctl.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          launchdBuild =
            if isDarwin then null else pkgs.callPackage ./nix/pkgs/launchd.nix {
              inherit darwinCrossToolchain nativeLd;
              libSystem = libSystemBuild;
              corefoundation = coreFoundationBuild;
              iokit = iokitBuild;
              src = libSystemSource;
            };
          libSystemBuild = mkPureDarwinBuild {
            pname = "puredarwin-libsystem";
            src = libSystemSource;
            buildTargets = [ "libSystem_B_stub" "dyld" "libsystem_kernel_static" "libdispatch_static" "XPC_libnv_static" "XPC_libinfo_static" "XPC_libxpc_static" "XPC_launchd_static" "XPC_launchd_mig_static" "XPC_notify_client_static" "notifyd" "logd" ];
            enableUserspace = false;
            enableKernel = false;
            installUserland = false;
            installKernel = false;
            installLibSystem = true;
          };
          libSystemArm64Build = libSystemBuild.override {
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
          };
          icuCoreArm64Build = icuCoreBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
          };
          libcxxabiDylibArm64Build = libcxxabiDylibBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
          };
          libcxxDylibArm64Build = libcxxDylibBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            libcxxabiDylib = libcxxabiDylibArm64Build;
          };
          libobjcArm64Build = libobjcBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            libcxxabiDylib = libcxxabiDylibArm64Build;
          };
          coreFoundationArm64Build = coreFoundationBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            icu = icuCoreArm64Build;
            libobjc = libobjcArm64Build;
          };
          iokitArm64Build = iokitBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            corefoundation = coreFoundationArm64Build;
          };
          launchdArm64Build = launchdBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            corefoundation = coreFoundationArm64Build;
            iokit = iokitArm64Build;
          };
          launchctlArm64Build = launchctlBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            corefoundation = coreFoundationArm64Build;
            iokit = iokitArm64Build;
          };
          # Re-instantiate a package file for arm64. The arm64 toolchain/triple/
          # libSystem are filtered down to the arguments each package actually
          # declares, so the same call works everywhere; callPackage still fills
          # the plain nixpkgs inputs, and `deps` rewires that package's own
          # PureDarwin dependencies onto their arm64 builds.
          mkArm64Build = file: deps:
            if isDarwin then null else
            let
              f = import file;
              common = {
                darwinCrossToolchain = arm64CrossToolchain;
                targetTriple = "arm64-apple-darwin20.4";
                libSystem = libSystemArm64Build;
                inherit nativeLd;
              };
            in
            pkgs.callPackage f
              (builtins.intersectAttrs (builtins.functionArgs f) common // deps);

          xvfbZlibArm64Build = mkArm64Build ./nix/pkgs/xvfb-zlib.nix { inherit (pkgs) zlib; };
          toyboxArm64Build = mkArm64Build ./nix/pkgs/toybox.nix { zlib = xvfbZlibArm64Build; };
          xzArm64Build = mkArm64Build ./nix/pkgs/xz.nix { };
          fileArm64Build = mkArm64Build ./nix/pkgs/file.nix { zlib = xvfbZlibArm64Build; };
          opensslArm64Build = mkArm64Build ./nix/pkgs/openssl.nix { };

          # Core tools. autoconf/automake are host-side scripts with no
          # cross-compiled component, so they are shared with the x86 build
          # rather than re-instantiated.
          bmakeArm64Build = mkArm64Build ./nix/pkgs/bmake.nix { };
          gnumakeArm64Build = mkArm64Build ./nix/pkgs/gnumake.nix { };
          gnum4Arm64Build = mkArm64Build ./nix/pkgs/gnum4.nix { };
          pkgconfArm64Build = mkArm64Build ./nix/pkgs/pkgconf.nix {
            pkgconf = pkgs.pkgconf-unwrapped;
          };
          bisonArm64Build = mkArm64Build ./nix/pkgs/bison.nix { };
          flexArm64Build = mkArm64Build ./nix/pkgs/flex.nix { };
          xxdArm64Build = mkArm64Build ./nix/pkgs/xxd.nix { };
          nanoArm64Build = mkArm64Build ./nix/pkgs/nano.nix {
            ncurses = ncursesArm64Build;
          };

          # Core libraries.
          libffiArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
            pname = "puredarwin-libffi";
            version = pkgs.libffi.version;
            src = pkgs.libffi.src;
            configureFlags = [
              "--disable-docs"
              "--disable-multi-os-directory"
            ];
          };
          expatArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
            pname = "puredarwin-expat";
            version = pkgs.expat.version;
            src = pkgs.expat.src;
            configureFlags = [
              "--without-docbook"
              "--without-examples"
              "--without-tests"
            ];
          };
          pcre2Arm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
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
          libevArm64Build = mkArm64Build ./nix/pkgs/xorg-cross-lib.nix {
            pname = "puredarwin-libev";
            version = pkgs.libev.version;
            src = pkgs.libev.src;
            preConfigureExtra = ''
              export ac_cv_func_poll=yes
              export ac_cv_func_select=yes
              export ac_cv_header_poll_h=yes
            '';
          };
          libpngArm64Build = mkArm64Build ./nix/pkgs/libpng.nix {
            zlib = xvfbZlibArm64Build;
            inherit (pkgs) libpng;
          };
          freetype2Arm64Build = mkArm64Build ./nix/pkgs/xvfb-freetype.nix {
            inherit (pkgs) zlib freetype;
          };
          fontconfigArm64Build = mkArm64Build ./nix/pkgs/fontconfig.nix {
            inherit (pkgs) fontconfig;
            freetype = freetype2Arm64Build;
            expat = expatArm64Build;
          };
          libxml2Arm64Build = mkArm64Build ./nix/pkgs/libxml2.nix {
            inherit (pkgs) libxml2 meson ninja python3 git;
          };
          yajlArm64Build = mkArm64Build ./nix/pkgs/yajl.nix {
            inherit (pkgs) yajl;
          };
          glibArm64Build = mkArm64Build ./nix/pkgs/glib.nix {
            nativeMesonTools = nativeMesonToolsDir;
            inherit (pkgs) glib;
            pcre2 = pcre2Arm64Build;
            libffi = libffiArm64Build;
            zlib = xvfbZlibArm64Build;
            libiconv = libiconvArm64Build;
          };

          libiconvArm64Build = libiconvBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
          };
          ncursesArm64Build = ncursesBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
          };
          zshArm64Build = zshBuild.override {
            darwinCrossToolchain = arm64CrossToolchain;
            targetTriple = "arm64-apple-darwin20.4";
            libSystem = libSystemArm64Build;
            ncurses = ncursesArm64Build;
          };
          userlandArm64Build = userlandBuild.override {
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
            prebuiltLibSystem = libSystemArm64Build;
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
          osmesaFbBuild =
            if isDarwin then null else (mkPureDarwinBuild {
              pname = "puredarwin-osmesa-fb";
              src = fbdoomSource;
              buildTargets = [ "osmesa-fb" ];
              enableProjects = false;
              enableKernel = false;
              installUserland = false;
              installKernel = false;
              extraCmakeFlags = [
                "-DPUREDARWIN_ENABLE_OSMESA_FB=ON"
                "-DPUREDARWIN_OSMESA_PREFIX=${mesaBuild}/usr"
              ];
            }).overrideAttrs (old: {
              installPhase = ''
                runHook preInstall
                mkdir -p $out/usr/bin
                cp build-nix/src/Userspace/osmesa-fb/osmesa-fb $out/usr/bin/osmesa-fb
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
          kernelArm64Build = mkPureDarwinBuild {
            pname = "puredarwin-kernel-arm64";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "RELEASE";
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
          };
          kernelArm64VirtBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel-arm64-virt";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "RELEASE";
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
            extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=VIRT" ];
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
          kernelArm64VirtDebugBuild = mkPureDarwinBuild {
            pname = "puredarwin-kernel-arm64-virt-debug";
            src = kernelSource;
            buildTargets = [ "xnu" ];
            enableUserspace = false;
            installUserland = false;
            installKernel = true;
            xnuKernelConfig = "DEBUG";
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
            extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=VIRT" ];
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
          kextsArm64Build = mkPureDarwinBuild {
            pname = "puredarwin-kexts-arm64";
            src = kextsSource;
            buildTargets = [
              "IOPCIFamily" "IOStorageFamily" "IOVirtIOFamily.kext"
              "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
              "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
              "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext"
              "PDArmPlatformExpert" "PDArmPCI"
              # Arch-neutral drivers, matching what x86 builds: filesystems,
              # USB, the rest of VirtIO, and the remaining storage families.
              "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
              "IOUSBFamily" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
              "IOUSBCompositeDriver.kext"
              "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
              "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IONVMEFamily.kext"
              "RavynHDAudio.kext" "PDE1000.kext"
            ];
            enableUserspace = false;
            installUserland = false;
            installKernel = false;
            installKexts = true;
            installKextNames = [
              "IOPCIFamily.kext" "IOStorageFamily.kext" "IOVirtIOFamily.kext"
              "IOVirtIONet.kext" "IONetworkingFamily.kext" "IOHIDFamily.kext"
              "RavynAHCIPort.kext" "ext4.kext" "Ext4FileSystemDriver.kext"
              "AppleFileSystemDriver.kext" "corecrypto.kext" "pthread.kext"
              "PDArmPlatformExpert.kext" "PDArmPCI.kext"
              "msdosfs.kext" "apfs.kext" "hfs.kext" "HFSEncodings.kext"
              "IOUSBFamily.kext" "AppleUSBEHCI.kext" "AppleUSBOHCI.kext"
              "IOUSBCompositeDriver.kext"
              "IOUSBHIDDriver.kext" "AppleUSBMergeNub.kext"
              "RavynXHCIPort.kext" "IOVirtIOGPU.kext" "IONVMEFamily.kext"
              "RavynHDAudio.kext" "PDE1000.kext"
            ];
            enableIOGraphicsFamily = false;
            puredarwinArch = "arm64";
            inherit arm64CrossToolchain;
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
          fullBuild = mkPureDarwinBuild {
            pname = "puredarwin";
            src = ./.;
            buildTargets = [ "xnu" "kexts" "libsystem_kernel" "pcmplay" ];
            installUserland = false;
            installKernel = false;
            installBaseSystem = true;
            enableIOGraphicsFamily = true;
          };
          splitBaseSystem = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
            mkdir -p "$out"
            cp -a ${kernelBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kextsBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libSystemBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${userlandBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${tccBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${cctoolsBuild}/. "$out/"
          '' + lib.optionalString (!isDarwin && launchdBuild != null) ''
            chmod -R u+w "$out"
            cp -a ${launchdBuild}/. "$out/"
            chmod -R u+w "$out"
          '' + lib.optionalString (!isDarwin && launchctlBuild != null) ''
            cp -a ${launchctlBuild}/. "$out/"
            chmod -R u+w "$out"
            if [ -e "$out/pd-sbin/launchd" ]; then
              mkdir -p "$out/sbin"
              cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
              rm -rf "$out/pd-sbin"
            fi
          ''
          + lib.optionalString (!isDarwin) ''
            chmod -R u+w "$out"
            cp -a ${bmakeBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${xvfbBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${xeyesBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${xkbcompBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${xkeyboardConfigBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${xvfbFontsBuild}/. "$out/"
          '');
          splitBaseSystemStripped = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } (''
            mkdir -p "$out"
            cp -a ${kernelBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kextsBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libSystemBuild}/. "$out/"
          '' + lib.optionalString (!isDarwin && launchdBuild != null) ''
            chmod -R u+w "$out"
            cp -a ${launchdBuild}/. "$out/"
            chmod -R u+w "$out"
          '' + lib.optionalString (!isDarwin && launchctlBuild != null) ''
            cp -a ${launchctlBuild}/. "$out/"
            chmod -R u+w "$out"
            if [ -e "$out/pd-sbin/launchd" ]; then
              mkdir -p "$out/sbin"
              cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
              rm -rf "$out/pd-sbin"
            fi
          '');

          # Stripped base plus the CLI userland (sw_vers, mount, virgl-smoke,
          # etc.) - a lean image that still has usable tools, without the heavy
          # tcc/cctools/X of the full base.
          splitBaseSystemMinimal = pkgs.runCommand "puredarwin-basesystem-split-0.1" { } ''
            mkdir -p "$out"
            cp -a ${splitBaseSystemStripped}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${userlandBuild}/. "$out/"
          '';

          # ARM64 minimal mirrors the x86 minimal split: native libSystem,
          # launchd/launchctl, and the CLI userland are present alongside the
          # ARM64 kernel and kexts. Keep the kernel payload separate so this
          # composition cannot accidentally pull the x86 kernel into the ARM
          # image.
          splitBaseSystemArm64VirtMinimal = pkgs.runCommand "puredarwin-basesystem-arm64-virt-minimal-0.1" { } ''
            mkdir -p "$out"
            cp -a ${libSystemArm64Build}/. "$out/"
            chmod -R u+w "$out"
            # launchd is dynamically linked against the native framework and
            # runtime libraries. Keep these in the minimal image so dyld can
            # resolve launchd before any service jobs are submitted.
            cp -a ${icuCoreArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libcxxabiDylibArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libcxxDylibArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libobjcArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${coreFoundationArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${iokitArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${launchdArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${launchctlArm64Build}/. "$out/"
            chmod -R u+w "$out"
            if [ -e "$out/pd-sbin/launchd" ]; then
              mkdir -p "$out/sbin"
              cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
              rm -rf "$out/pd-sbin"
            fi
            cp -a ${userlandArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kernelArm64VirtDebugBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kextsArm64Build}/. "$out/"
            chmod -R u+w "$out"
          '';

          splitBaseSystemArm64VirtMinimalRelease = pkgs.runCommand "puredarwin-basesystem-arm64-virt-minimal-release-0.1" { } ''
            mkdir -p "$out"
            cp -a ${libSystemArm64Build}/. "$out/"
            chmod -R u+w "$out"
            # Keep the release image's runtime closure identical to debug.
            cp -a ${icuCoreArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libcxxabiDylibArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libcxxDylibArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${libobjcArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${coreFoundationArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${iokitArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${launchdArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${launchctlArm64Build}/. "$out/"
            chmod -R u+w "$out"
            if [ -e "$out/pd-sbin/launchd" ]; then
              mkdir -p "$out/sbin"
              cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
              rm -rf "$out/pd-sbin"
            fi
            cp -a ${userlandArm64Build}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kernelArm64VirtBuild}/. "$out/"
            chmod -R u+w "$out"
            cp -a ${kextsArm64Build}/. "$out/"
            chmod -R u+w "$out"
          '';

          # The same package set as imageExtraPackageSet, resolved to the arm64
          # builds, so the two architectures ship the same userland.
          imageExtraPackageSetArm64 = lib.optionalAttrs (!isDarwin) {
            xvfb = xvfbArm64Build;
            xorg = xorgArm64Build;
            libxcvt = xvfbLibxcvtArm64Build;
            xeyes = xeyesArm64Build;
            xclock = xclockArm64Build;
            xcalc = xcalcArm64Build;
            xmessage = xmessageArm64Build;
            dillo = dilloArm64Build;
            libz-dylib = libzDylibArm64Build;
            libcurl-dylib = libcurlDylibArm64Build;
            dbus = dbusArm64Build;
            libxml2 = libxml2Arm64Build;
            at-spi2-core = atspi2CoreArm64Build;
            libepoxy = libepoxyArm64Build;
            gdk-pixbuf = gdkPixbufArm64Build;
            cairo-gobject = cairoGobjectArm64Build;
            libXrandr = xvfbLibXrandrArm64Build;
            gtk3 = gtk3Arm64Build;
            libpng = libpngArm64Build;
            libwapcaplet = libwapcapletArm64Build;
            libparserutils = libparserutilsArm64Build;
            libnsutils = libnsutilsArm64Build;
            libnsgif = libnsgifArm64Build;
            libnsbmp = libnsbmpArm64Build;
            libutf8proc = libutf8procArm64Build;
            libhubbub = libhubbubArm64Build;
            libcss = libcssArm64Build;
            libdom = libdomArm64Build;
            netsurf = netsurfArm64Build;
            xxd = xxdArm64Build;
            xz = xzArm64Build;
            xterm = xtermArm64Build;
            xkbcomp = xkbcompArm64Build;
            xkeyboard-config = xkeyboardConfigArm64Build;
            libx11-locale = xlibLocaleArm64Build;
            fonts = xvfbFontsArm64Build;
            libiconv = libiconvArm64Build;
            nano = nanoArm64Build;
            bmake = bmakeArm64Build;
            gnumake = gnumakeArm64Build;
            pkgconf = pkgconfArm64Build;
            gnum4 = gnum4Arm64Build;
            autoconf = autoconfArm64Build;
            automake = automakeArm64Build;
            bison = bisonArm64Build;
            flex = flexArm64Build;
            python = pythonArm64Build;
            #perl = perlBuild;
            zsh = zshArm64Build;
            toybox = toyboxArm64Build;
            file = fileArm64Build;
            openssl = opensslArm64Build;
            curl = curlArm64Build;
            openssh = opensshArm64Build;
            git = gitArm64Build;
            migcomDarwin = migcomDarwinArm64Build;
            ioreg = ioregArm64Build;
            xkbcommon = xkbcommonArm64Build;
            fastfetch = fastfetchArm64Build;
            corefoundation = coreFoundationArm64Build;
            icucore = icuCoreArm64Build;
            libcxxabi-dylib = libcxxabiDylibArm64Build;
            libcxx-dylib = libcxxDylibArm64Build;
            libcxx-test = libcxxTestArm64Build;
            mesa = mesaArm64Build;
            mesa-demos = mesaDemosArm64Build;
            pd-virgl-shim = pdVirglShimArm64Build;
            osmesa-tri = osmesaTriArm64Build;
            osmesa-fb = osmesaFbArm64Build;
            libobjc = libobjcArm64Build;
            foundation = foundationArm64Build;
            iokit = iokitArm64Build;
            security = securityArm64Build;
            systemConfiguration = systemConfigurationArm64Build;
            opengl-framework = openglFrameworkArm64Build;
            i3 = i3Arm64Build;
            i3status = i3statusShimArm64Build;
            startup-notification = startupNotificationArm64Build;
            libX11 = xlibArm64Build;
            libxcb = xcbArm64Build;
            libxcb-util = xcbUtilArm64Build;
            libxcb-keysyms = xcbKeysymsArm64Build;
            libxcb-wm = xcbWmArm64Build;
            libxcb-render-util = xcbRenderUtilArm64Build;
            libxcb-image = xcbImageArm64Build;
            libxcb-cursor = xcbCursorArm64Build;
            xcb-util-xrm = xcbXrmArm64Build;
            libev = libevArm64Build;
            pcre2 = pcre2Arm64Build;
            yajl = yajlArm64Build;
            cairo = cairoArm64Build;
            libffi = libffiArm64Build;
            glib = glibArm64Build;
            fribidi = fribidiArm64Build;
            harfbuzz = harfbuzzArm64Build;
            expat = expatArm64Build;
            fontconfig = fontconfigArm64Build;
            freetype2 = freetype2Arm64Build;
            pango = pangoArm64Build;
            libXft = libXftArm64Build;
            dmenu = dmenuArm64Build;
            zlib = xvfbZlibArm64Build;
            libXau = xvfbLibXauArm64Build;
            libXdmcp = xvfbLibXdmcpArm64Build;
            libXext = xvfbLibXextArm64Build;
            libXrender = xvfbLibXrenderArm64Build;
            libXfixes = xvfbLibXfixesArm64Build;
            libXcursor = xvfbLibXcursorArm64Build;
            libICE = xvfbLibICEArm64Build;
            libSM = xvfbLibSMArm64Build;
          };

          imageExtraPackagesArm64 = lib.attrValues imageExtraPackageSetArm64;

          imageExtraPackageSet = lib.optionalAttrs (!isDarwin) {
            xvfb = xvfbBuild;
            xorg = xorgBuild;
            libxcvt = xvfbLibxcvtBuild;
            xeyes = xeyesBuild;
            xclock = xclockBuild;
            xcalc = xcalcBuild;
            xmessage = xmessageBuild;
            dillo = dilloBuild;
            libz-dylib = libzDylibBuild;
            libcurl-dylib = libcurlDylibBuild;
            dbus = dbusBuild;
            libxml2 = libxml2Build;
            at-spi2-core = atspi2CoreBuild;
            libepoxy = libepoxyBuild;
            gdk-pixbuf = gdkPixbufBuild;
            cairo-gobject = cairoGobjectBuild;
            libXrandr = xvfbLibXrandrBuild;
            gtk3 = gtk3Build;
            libpng = libpngBuild;
            libwapcaplet = libwapcapletBuild;
            libparserutils = libparserutilsBuild;
            libnsutils = libnsutilsBuild;
            libnsgif = libnsgifBuild;
            libnsbmp = libnsbmpBuild;
            libutf8proc = libutf8procBuild;
            libhubbub = libhubbubBuild;
            libcss = libcssBuild;
            libdom = libdomBuild;
            netsurf = netsurfBuild;
            xxd = xxdBuild;
            xz = xzBuild;
            xterm = xtermBuild;
            xkbcomp = xkbcompBuild;
            xkeyboard-config = xkeyboardConfigBuild;
            libx11-locale = xlibLocaleBuild;
            fonts = xvfbFontsBuild;
            libiconv = libiconvBuild;
            nano = nanoBuild;
            bmake = bmakeBuild;
            gnumake = gnumakeBuild;
            pkgconf = pkgconfBuild;
            gnum4 = gnum4Build;
            autoconf = autoconfBuild;
            automake = automakeBuild;
            bison = bisonBuild;
            flex = flexBuild;
            python = pythonBuild;
            #perl = perlBuild;
            zsh = zshBuild;
            toybox = toyboxBuild;
            file = fileBuild;
            openssl = opensslBuild;
            curl = curlBuild;
            openssh = opensshBuild;
            git = gitBuild;
            migcomDarwin = migcomDarwinBuild;
            ioreg = ioregBuild;
            xkbcommon = xkbcommonBuild;
            fastfetch = fastfetchBuild;
            corefoundation = coreFoundationBuild;
            icucore = icuCoreBuild;
            libcxxabi-dylib = libcxxabiDylibBuild;
            libcxx-dylib = libcxxDylibBuild;
            libcxx-test = libcxxTestBuild;
            mesa = mesaBuild;
            mesa-demos = mesaDemosBuild;
            pd-virgl-shim = pdVirglShimBuild;
            osmesa-tri = osmesaTriBuild;
            osmesa-fb = osmesaFbBuild;
            libobjc = libobjcBuild;
            foundation = foundationBuild;
            iokit = iokitBuild;
            security = securityBuild;
            systemConfiguration = systemConfigurationBuild;
            diskArbitration = diskArbitrationBuild;
            iomediacheck = iomediacheckBuild;
            opengl-framework = openglFrameworkBuild;
            i3 = i3Build;
            i3status = i3statusShimBuild;
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
            libev = libevBuild;
            pcre2 = pcre2Build;
            yajl = yajlBuild;
            cairo = cairoBuild;
            libffi = libffiBuild;
            glib = glibBuild;
            fribidi = fribidiBuild;
            harfbuzz = harfbuzzBuild;
            expat = expatBuild;
            fontconfig = fontconfigBuild;
            freetype2 = freetype2Build;
            pango = pangoBuild;
            libXft = libXftBuild;
            dmenu = dmenuBuild;
            zlib = xvfbZlibBuild;
            libXau = xvfbLibXauBuild;
            libXdmcp = xvfbLibXdmcpBuild;
            libXext = xvfbLibXextBuild;
            libXrender = xvfbLibXrenderBuild;
            libXfixes = xvfbLibXfixesBuild;
            libXcursor = xvfbLibXcursorBuild;
            libICE = xvfbLibICEBuild;
            libSM = xvfbLibSMBuild;
          };

          arm64Packages = lib.optionalAttrs (!isDarwin) {
            zlib-arm64 = xvfbZlibArm64Build;
            toybox-arm64 = toyboxArm64Build;
            xz-arm64 = xzArm64Build;
            file-arm64 = fileArm64Build;
            openssl-arm64 = opensslArm64Build;
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
            osmesaFb-arm64 = osmesaFbArm64Build;
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
            osmesaTri-arm64 = osmesaTriArm64Build;
            pango-arm64 = pangoArm64Build;
            python-arm64 = pythonArm64Build;
            security-arm64 = securityArm64Build;
            systemConfiguration-arm64 = systemConfigurationArm64Build;
            diskArbitration = diskArbitrationBuild;
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

          commonPackages = {
            userland = userlandBuild;
            tcc = tccBuild;
            cctools = cctoolsBuild;
            libsystem = libSystemBuild;
            libSystem = libSystemBuild;
            libapfsrw = libapfsrwBuild;
            xnu-headers = xnuHeadersBuild;
            xnu = kernelBuild;
            xnu-debug = kernelDebugBuild;
            kernel = kernelBuild;
            kernel-debug = kernelDebugBuild;
            kernel-arm64 = kernelArm64Build;
            kernel-arm64-virt = kernelArm64VirtBuild;
            kernel-arm64-virt-debug = kernelArm64VirtDebugBuild;
            kexts = kextsBuild;
            kexts-arm64 = kextsArm64Build;
            iographics = iographicsBuild;
            basesystem = fullBuild;
            basesystem-split = splitBaseSystem;
            default = fullBuild;
            fbdoom = fbdoomBuild;
          } // imageExtraPackageSet // lib.optionalAttrs (!isDarwin) {
            libX11 = xlibBuild;
            libxcb = xcbBuild;
            freetype2 = freetype2Build;
            ncurses = ncursesBuild;
            libiconv = libiconvBuild;
            libxkbfile = xvfbLibXkbfileBuild;
            libxcb-util = xcbUtilBuild;
            libxcb-keysyms = xcbKeysymsBuild;
            libxcb-wm = xcbWmBuild;
            libxcb-render-util = xcbRenderUtilBuild;
            libxcb-image = xcbImageBuild;
            libxcb-cursor = xcbCursorBuild;
            xcb-util-xrm = xcbXrmBuild;
            libev = libevBuild;
            pcre2 = pcre2Build;
            yajl = yajlBuild;
            startup-notification = startupNotificationBuild;
            cairo = cairoBuild;
            libffi = libffiBuild;
            glib = glibBuild;
            fribidi = fribidiBuild;
            harfbuzz = harfbuzzBuild;
            expat = expatBuild;
            fontconfig = fontconfigBuild;
            pango = pangoBuild;
            i3 = i3Build;
            i3status = i3statusShimBuild;
            openssh = opensshBuild;
            gnumake = gnumakeBuild;
            pkgconf = pkgconfBuild;
            gnum4 = gnum4Build;
            autoconf = autoconfBuild;
            automake = automakeBuild;
            bison = bisonBuild;
            flex = flexBuild;
            python = pythonBuild;
            #perl = perlBuild;
          };

          linuxPackages =
            let
              kcBuild = pkgs.callPackage ./nix/pkgs/kc.nix {
                kernel = kernelBuild;
                kexts = kextsBuild;
                kcTools = kc-tools.packages.${system}.default;
              };
              kcDebugBuild = pkgs.callPackage ./nix/pkgs/kc.nix {
                kernel = kernelDebugBuild;
                kexts = kextsBuild;
                kcTools = kc-tools.packages.${system}.default;
              };
              kcArm64DebugBuild = pkgs.callPackage ./nix/pkgs/kc-arm64.nix {
                kernel = kernelArm64VirtDebugBuild;
                kexts = kextsArm64Build;
                kcTools = kc-tools.packages.${system}.default;
              };
              kcArm64ReleaseBuild = pkgs.callPackage ./nix/pkgs/kc-arm64.nix {
                kernel = kernelArm64VirtBuild;
                kexts = kextsArm64Build;
                kcTools = kc-tools.packages.${system}.default;
              };
              imageExtraPackages = lib.attrValues imageExtraPackageSet
                ++ lib.optional (fbdoomExternalSrc != null) fbdoomBuild;
              imageBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = imageExtraPackages;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                #testAudioFile = /home/vali/development/darwin/stillalive.pcm;
              };
              imageHfsBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = imageExtraPackages;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                hfsprogs = pkgs.hfsprogs;
                libdmg-hfsplus = pkgs.callPackage ./nix/pkgs/libdmg-hfsplus.nix { };
                rootFsType = "hfs";
                #testAudioFile = /home/vali/development/darwin/badapple.pcm;
              };
              imageDebugBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = lib.attrValues imageExtraPackageSet;
                kc = kcDebugBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                imageFileName = "puredarwin-debug.img";
              };
              imageArm64VirtBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystem;
                extraPackages = imageExtraPackages;
                kc = kcArm64DebugBuild;
                xnuLoader = xnu-loader.packages.${system}.arm64-virt;
                apfsprogs = pkgs.apfsprogs;
                efiBinary = "BOOTAA64.EFI";
                imageFileName = "puredarwin-arm64-virt.img";
              };
              imageArm64VirtMinimalBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystemArm64VirtMinimal;
                extraPackages = [ ];
                kc = kcArm64DebugBuild;
                xnuLoader = xnu-loader.packages.${system}.arm64-virt;
                apfsprogs = pkgs.apfsprogs;
                efiBinary = "BOOTAA64.EFI";
                espMB = 64;
                rootMB = 256;
                imageFileName = "puredarwin-arm64-virt-minimal.img";
                bootArgs = "debug=0x219 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 -noprogress gen9_debug=1 vgpu_debug=1 pdtrace=1 ahci_debug=1 no_interrupt_masked_debug=1";
              };
              # Full arm64 image: the same userland as the x86 .#image, on the
              # arm64 base system and release KC. Sized for the whole stack
              # (ICU data and Mesa alone are most of a minimal image).
              imageArm64VirtFullBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystemArm64VirtMinimalRelease;
                extraPackages = imageExtraPackagesArm64;
                kc = kcArm64ReleaseBuild;
                xnuLoader = xnu-loader.packages.${system}.arm64-virt;
                apfsprogs = pkgs.apfsprogs;
                efiBinary = "BOOTAA64.EFI";
                espMB = 64;
                rootMB = 3072;
                imageFileName = "puredarwin-arm64-virt-full.img";
                bootArgs = "serial=3 -noprogress";
              };
              imageArm64VirtMinimalReleaseBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystemArm64VirtMinimalRelease;
                # toybox backs /bin/ls, /bin/cp and friends (they are symlinks
                # to it), so without it those are all dangling.
                extraPackages = [ zshArm64Build libiconvArm64Build toyboxArm64Build ];
                kc = kcArm64ReleaseBuild;
                xnuLoader = xnu-loader.packages.${system}.arm64-virt;
                apfsprogs = pkgs.apfsprogs;
                efiBinary = "BOOTAA64.EFI";
                espMB = 64;
                rootMB = 512;
                imageFileName = "puredarwin-arm64-virt-minimal-release.img";
                bootArgs = "serial=3 -noprogress ahci_debug=1 kext=0xffff io=0xffff";
              };
              strippedExtraPackages = [ zshBuild libiconvBuild coreFoundationBuild icuCoreBuild iokitBuild libcxxabiDylibBuild libcxxDylibBuild libcxxTestBuild libobjcBuild objcTestBuild mesaBuild pdVirglShimBuild osmesaTriBuild osmesaFbBuild mesaDemosBuild ];
              imageStrippedBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystemStripped;
                extraPackages = strippedExtraPackages;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                imageFileName = "puredarwin-stripped.img";
              };
              imageMinimalBuild = pkgs.callPackage ./image.nix {
                baseSystem = splitBaseSystemMinimal;
                extraPackages = strippedExtraPackages;
                kc = kcBuild;
                xnuLoader = xnu-loader.packages.${system}.default;
                apfsprogs = pkgs.apfsprogs;
                imageFileName = "puredarwin-minimal.img";
              };
              runVm = pkgs.writeShellApplication {
                name = "puredarwin-vm";
                runtimeInputs = [ pkgs.qemu ];
                text = ''
                  set -euo pipefail

                  state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.puredarwin-vm}"
                  image="''${PUREDARWIN_IMAGE:-}"
                  ovmf_code="''${PUREDARWIN_OVMF_CODE:-${pkgs.OVMF.fd}/FV/OVMF_CODE.fd}"
                  ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-${pkgs.OVMF.fd}/FV/OVMF_VARS.fd}"
                  ovmf_vars="''${PUREDARWIN_OVMF_VARS:-$state_dir/OVMF_VARS.fd}"

                  if [ -z "$image" ]; then
                    if [ -e "$PWD/puredarwin.img" ]; then
                      image="$PWD/puredarwin.img"
                    elif [ -e "$PWD/result/puredarwin.img" ]; then
                      image="$PWD/result/puredarwin.img"
                    else
                      echo "puredarwin-vm: no image found; set PUREDARWIN_IMAGE or run nix build .#image" >&2
                      exit 1
                    fi
                  fi
                  image_readonly_opt=""
                  if [ ! -w "$image" ]; then
                    image_readonly_opt=",snapshot=on"
                  fi

                  mkdir -p "$state_dir"
                  if [ ! -e "$ovmf_vars" ]; then
                    cp "$ovmf_vars_template" "$ovmf_vars"
                    chmod u+w "$ovmf_vars"
                  fi

                  exec qemu-system-x86_64 \
                    -M q35 \
                    -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
                    -smp "''${PUREDARWIN_VM_SMP:-4}" \
                    -vga "''${PUREDARWIN_VM_VGA:-std}" \
                    -cpu IvyBridge,vendor=GenuineIntel \
                    -fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=2048 \
                    -drive if=pflash,format=raw,unit=0,readonly=on,file="$ovmf_code" \
                    -drive if=pflash,format=raw,unit=1,file="$ovmf_vars" \
                    -drive id=root,format=raw,file="$image"$image_readonly_opt \
                    -device qemu-xhci,id=xhci \
                    -device usb-kbd,bus=xhci.0 \
                    -device usb-mouse,bus=xhci.0 \
                    -device intel-hda,id=hda \
                    -device hda-duplex,audiodev=snd0 \
                    -audiodev "''${PUREDARWIN_VM_AUDIODEV:-none},id=snd0" \
                    -serial mon:stdio \
                    -no-reboot \
                    -no-shutdown \
                    "$@"
                '';
              };
              runKvm = pkgs.writeShellApplication {
                name = "puredarwin-kvm";
                runtimeInputs = [ pkgs.qemu ];
                text = ''
                  set -euo pipefail

                  state_dir="''${PUREDARWIN_VM_STATE_DIR:-$PWD/.puredarwin-kvm}"
                  image="''${PUREDARWIN_IMAGE:-}"
                  ovmf_code="''${PUREDARWIN_OVMF_CODE:-${pkgs.OVMF.fd}/FV/OVMF_CODE.fd}"
                  ovmf_vars_template="''${PUREDARWIN_OVMF_VARS_TEMPLATE:-${pkgs.OVMF.fd}/FV/OVMF_VARS.fd}"
                  ovmf_vars="''${PUREDARWIN_OVMF_VARS:-$state_dir/OVMF_VARS.fd}"

                  if [ -z "$image" ]; then
                    if [ -e "$PWD/puredarwin.img" ]; then
                      image="$PWD/puredarwin.img"
                    elif [ -e "$PWD/result/puredarwin.img" ]; then
                      image="$PWD/result/puredarwin.img"
                    else
                      echo "puredarwin-kvm: no image found; set PUREDARWIN_IMAGE or run nix build .#image" >&2
                      exit 1
                    fi
                  fi
                  image_readonly_opt=""
                  if [ ! -w "$image" ]; then
                    image_readonly_opt=",snapshot=on"
                  fi

                  mkdir -p "$state_dir"
                  if [ ! -e "$ovmf_vars" ]; then
                    cp "$ovmf_vars_template" "$ovmf_vars"
                    chmod u+w "$ovmf_vars"
                  fi

                  exec qemu-system-x86_64 \
                    -machine q35,accel=kvm \
                    -cpu "''${PUREDARWIN_KVM_CPU:-host}" \
                    -smp "''${PUREDARWIN_VM_SMP:-4}" \
                    -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
                    -vga "''${PUREDARWIN_VM_VGA:-std}" \
                    -fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=2048 \
                    -drive if=pflash,format=raw,unit=0,readonly=on,file="$ovmf_code" \
                    -drive if=pflash,format=raw,unit=1,file="$ovmf_vars" \
                    -device ich9-ahci,id=sata \
                    -drive if=none,id=system,file="$image",format=raw,cache=writeback"$image_readonly_opt" \
                    -device ide-hd,bus=sata.0,drive=system \
                    -device e1000-82545em,netdev=net0 \
                    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
                    ''${PUREDARWIN_VM_NETDUMP:+-object filter-dump,id=netdump,netdev=net0,file="$PUREDARWIN_VM_NETDUMP"} \
                    -device qemu-xhci,id=xhci \
                    -device usb-kbd,bus=xhci.0 \
                    -device usb-mouse,bus=xhci.0 \
                    -device intel-hda,id=hda \
                    -device hda-duplex,audiodev=snd0 \
                    -audiodev "''${PUREDARWIN_VM_AUDIODEV:-none},id=snd0" \
                    -serial mon:stdio \
                    -no-reboot \
                    -no-shutdown \
                    "$@"
                '';
              };
              runArm64Uefi = pkgs.writeShellApplication {
                name = "puredarwin-arm64-uefi";
                runtimeInputs = [ pkgs.qemu ];
                text = ''
                  set -euo pipefail

                  state_dir="''${PUREDARWIN_ARM64_UEFI_STATE_DIR:-$PWD/.puredarwin-arm64-uefi}"
                  aavmf_code="''${PUREDARWIN_AAVMF_CODE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_CODE.fd}"
                  aavmf_vars_template="''${PUREDARWIN_AAVMF_VARS_TEMPLATE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_VARS.fd}"
                  aavmf_vars="''${PUREDARWIN_AAVMF_VARS:-$state_dir/AAVMF_VARS.fd}"

                  mkdir -p "$state_dir"
                  if [ "''${PUREDARWIN_ARM64_RESET_VARS:-0}" = 1 ]; then
                    rm -f "$aavmf_vars"
                  fi
                  if [ ! -e "$aavmf_vars" ]; then
                    cp "$aavmf_vars_template" "$aavmf_vars"
                    chmod u+w "$aavmf_vars"
                  fi

                  exec qemu-system-aarch64 \
                    -machine virt,gic-version=3 \
                    -cpu "''${PUREDARWIN_ARM64_VM_CPU:-max}" \
                    -m "''${PUREDARWIN_VM_MEMORY:-1024}" \
                    -drive if=pflash,format=raw,unit=0,readonly=on,file="$aavmf_code" \
                    -drive if=pflash,format=raw,unit=1,file="$aavmf_vars" \
                    -device virtio-gpu-pci \
                    -serial mon:stdio \
                    -display "''${PUREDARWIN_ARM64_UEFI_DISPLAY:-gtk}" \
                    -no-reboot \
                    -no-shutdown \
                    "$@"
                '';
              };
              runArm64Uboot = pkgs.writeShellApplication {
                name = "puredarwin-arm64-uboot";
                runtimeInputs = [ pkgs.qemu ];
                text = ''
                  set -euo pipefail

                  image="''${PUREDARWIN_IMAGE:-}"
                  if [ -z "$image" ]; then
                    if [ -e "$PWD/puredarwin-arm64-virt.img" ]; then
                      image="$PWD/puredarwin-arm64-virt.img"
                    elif [ -e "$PWD/result/puredarwin-arm64-virt.img" ]; then
                      image="$PWD/result/puredarwin-arm64-virt.img"
                    else
                      echo "puredarwin-arm64-uboot: no image found" >&2
                      exit 1
                    fi
                  fi

                  image_readonly_opt=""
                  if [ ! -w "$image" ]; then
                    image_readonly_opt=",snapshot=on"
                  fi

                  exec qemu-system-aarch64 \
                    -machine virt,gic-version=3 \
                    -cpu "''${PUREDARWIN_ARM64_VM_CPU:-max}" \
                    -smp "''${PUREDARWIN_VM_SMP:-4}" \
                    -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
                    -bios "${pkgs.pkgsCross.aarch64-multiplatform.ubootQemuAarch64}/u-boot.bin" \
                    -drive if=none,id=system,file="$image",format=raw$image_readonly_opt \
                    -device ich9-ahci,id=ahci0 \
                    -device ide-hd,drive=system,bus=ahci0.0 \
                    -serial mon:stdio \
                    -display none \
                    -no-reboot \
                    -no-shutdown \
                    "$@"
                '';
              };
              runArm64Virt = pkgs.writeShellApplication {
                name = "puredarwin-arm64-virt";
                runtimeInputs = [ pkgs.qemu ];
                text = ''
                  set -euo pipefail

                  state_dir="''${PUREDARWIN_ARM64_VM_STATE_DIR:-$PWD/.puredarwin-arm64-virt}"
                  image="''${PUREDARWIN_IMAGE:-}"
                  aavmf_code="''${PUREDARWIN_AAVMF_CODE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_CODE.fd}"
                  aavmf_vars_template="''${PUREDARWIN_AAVMF_VARS_TEMPLATE:-${pkgs.pkgsCross.aarch64-multiplatform.OVMF.fd}/FV/AAVMF_VARS.fd}"
                  aavmf_vars="''${PUREDARWIN_AAVMF_VARS:-$state_dir/AAVMF_VARS.fd}"

                  if [ -z "$image" ]; then
                    if [ -e "$PWD/puredarwin-arm64-virt.img" ]; then
                      image="$PWD/puredarwin-arm64-virt.img"
                    elif [ -e "$PWD/result/puredarwin-arm64-virt.img" ]; then
                      image="$PWD/result/puredarwin-arm64-virt.img"
                    else
                      echo "puredarwin-arm64-virt: no image found; run nix build .#image-arm64-virt" >&2
                      exit 1
                    fi
                  fi

                  image_readonly_opt=""
                  if [ ! -w "$image" ]; then
                    image_readonly_opt=",snapshot=on"
                  fi

                  mkdir -p "$state_dir"
                  if [ "''${PUREDARWIN_ARM64_RESET_VARS:-0}" = 1 ]; then
                    rm -f "$aavmf_vars"
                  fi
                  if [ ! -e "$aavmf_vars" ]; then
                    cp "$aavmf_vars_template" "$aavmf_vars"
                    chmod u+w "$aavmf_vars"
                  fi

                  exec qemu-system-aarch64 \
                    -machine virt,gic-version=3 \
                    -boot order=c,strict=on \
                    -cpu "''${PUREDARWIN_ARM64_VM_CPU:-max}" \
                    -smp "''${PUREDARWIN_VM_SMP:-4}" \
                    -m "''${PUREDARWIN_VM_MEMORY:-4096}" \
                    -drive if=pflash,format=raw,unit=0,readonly=on,file="$aavmf_code" \
                    -drive if=pflash,format=raw,unit=1,file="$aavmf_vars" \
                    -drive if=none,id=system,file="$image",format=raw$image_readonly_opt \
                    -device virtio-blk-pci,drive=system,bootindex=1 \
                    -device virtio-net-pci,netdev=net0 \
                    -netdev user,id=net0,hostfwd=tcp::2223-:22 \
                    -serial mon:stdio \
                    -display none \
                    -no-reboot \
                    -no-shutdown \
                    "$@"
                '';
              };
            in {
              darwin-cross-toolchain = darwinCrossToolchain;
              native-ld = nativeLd;
              kc = kcBuild;
              kc-debug = kcDebugBuild;
              kc-arm64-debug = kcArm64DebugBuild;
              kc-arm64 = kcArm64ReleaseBuild;
              corefoundation = coreFoundationBuild;
              icucore = icuCoreBuild;
              libcxxabi-dylib = libcxxabiDylibBuild;
              libcxx-dylib = libcxxDylibBuild;
              libcxx-test = libcxxTestBuild;
              mesa = mesaBuild;
              osmesa-tri = osmesaTriBuild;
              osmesa-fb = osmesaFbBuild;
              libobjc = libobjcBuild;
              objc-test = objcTestBuild;
              foundation = foundationBuild;
              autoconf = autoconfBuild;
              automake = automakeBuild;
              iokit = iokitBuild;
              security = securityBuild;
              systemstarter = systemStarterBuild;
              launchd = launchdBuild;
              launchctl = launchctlBuild;
              image = imageBuild;
              image-arm64-virt = imageArm64VirtBuild;
              image-arm64-virt-minimal = imageArm64VirtMinimalBuild;
              image-arm64-virt-minimal-release = imageArm64VirtMinimalReleaseBuild;
              image-arm64-virt-full = imageArm64VirtFullBuild;
              image-hfs = imageHfsBuild;
              image-debug = imageDebugBuild;
              image-stripped = imageStrippedBuild;
              image-minimal = imageMinimalBuild;
              xorg = xorgBuild;
              libxcvt = xvfbLibxcvtBuild;
              userland = userlandBuild;
              vm-runner = runVm;
              kvm-runner = runKvm;
              arm64-virt-runner = runArm64Virt;
              arm64-uefi-runner = runArm64Uefi;
              arm64-uboot-runner = runArm64Uboot;
            };

          linuxApps =
            let
              runVm = linuxPackages.vm-runner;
              runKvm = linuxPackages.kvm-runner;
              runVirt = linuxPackages.arm64-virt-runner;
            in {
              default = {
                type = "app";
                program = "${runVm}/bin/puredarwin-vm";
              };
              vm = {
                type = "app";
                program = "${runVm}/bin/puredarwin-vm";
              };
              arm64-virt = {
                type = "app";
                program = "${runVirt}/bin/puredarwin-arm64-virt";
              };
              arm64-uefi = {
                type = "app";
                program = "${linuxPackages.arm64-uefi-runner}/bin/puredarwin-arm64-uefi";
              };
              arm64-uboot = {
                type = "app";
                program = "${linuxPackages.arm64-uboot-runner}/bin/puredarwin-arm64-uboot";
              };
              kvm = {
                type = "app";
                program = "${runKvm}/bin/puredarwin-kvm";
              };
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
          packages = commonPackages // arm64Packages // lib.optionalAttrs (!isDarwin) linuxPackages;
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
