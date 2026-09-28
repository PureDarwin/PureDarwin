# riscv64 kernel, kexts, boot kernel collection and userland for qemu virt
{ lib
, pkgs
, mkPureDarwinBuild
, kernelSource
, kextsSource
, riscv64CrossToolchain
, compilerRtRiscv64Kernel
, compilerRtRiscv64Build
, kcTools
, libSystemBuild
, icuCoreBuild
, libcxxabiDylibBuild
, libcxxDylibBuild
, libobjcBuild
, nativeLd
, foundationSource
, coreFoundationBuild
, iokitCFStaticBuild
, iokitBuild
, launchdBuild
, launchctlBuild
, userlandBuild
, libiconvBuild
, ncursesBuild
, zshBuild
  # the riscv64 linux-Image loader, only in the local xnu-loader checkout for now
, xnuLoaderRiscv64 ? null
}:

let
  # cmake target names, in kc-builder load order (dependencies first)
  kextTargets = [
    "corecrypto.kext" "pthread.kext" "amfi.kext" "Sandbox.kext" "PDRiscvPlatformExpert"
    # virtio sits behind pci, PDEcamPCI is the device tree driven host bridge
    "IOPCIFamily" "PDEcamPCI"
    "IOStorageFamily" "IOCDStorageFamily.kext" "IODVDStorageFamily.kext"
    "IOBDStorageFamily.kext"
    "ext4.kext" "AppleFileSystemDriver.kext" "Ext4FileSystemDriver.kext"
    "msdosfs.kext" "apfs.kext" "ApfsFileSystemDriver.kext"
    "hfs.kext" "HFSEncodings.kext"
    "IOHIDFamily.kext"
    "IOUSBFamily" "IOUSBCompositeDriver.kext" "AppleUSBMergeNub.kext"
    "IOUSBHIDDriver.kext"
    "IOGraphicsFamily.kext"
    "IOVirtIOFamily.kext" "IOVirtIOGPU.kext"
    "IONetworkingFamily.kext"
    "IOVirtIOBlock.kext" "IOVirtIONet.kext"
    "PDWatchdog.kext"
  ];
  kextBundle = t: if lib.hasSuffix ".kext" t then t else "${t}.kext";
  kextNames = map kextBundle kextTargets;

  kernelRiscv64VirtDebugBuild = mkPureDarwinBuild {
    pname = "puredarwin-kernel-riscv64-virt-debug";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "DEBUG";
    puredarwinArch = "riscv64";
    inherit riscv64CrossToolchain;
    # soft-float kernel, the double and float helpers come from compiler-rt
    extraCmakeFlags = [
      "-DPUREDARWIN_KERNEL_COMPILER_RT=${compilerRtRiscv64Kernel}/lib/libcompiler_rt.a"
    ];
  };

  # kexts only need the installed kernel headers
  kextsRiscv64Build = mkPureDarwinBuild {
    pname = "puredarwin-kexts-riscv64";
    src = kextsSource;
    buildTargets = kextTargets;
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installKexts = true;
    installKextNames = kextNames;
    enableIOGraphicsFamily = true;
    puredarwinArch = "riscv64";
    inherit riscv64CrossToolchain;
  };

  # userland starts at libSystem, the same cmake targets as arm64
  libSystemRiscv64Build = libSystemBuild.override {
    puredarwinArch = "riscv64";
    inherit riscv64CrossToolchain;
    compilerRtRiscv64 = compilerRtRiscv64Build;
  };

  # userland runtime dylibs, the arm64 recipes built by the riscv64 toolchain
  riscv64Userland = {
    darwinCrossToolchain = riscv64CrossToolchain;
    targetTriple = "riscv64-apple-darwin20.4";
    libSystem = libSystemRiscv64Build;
  };
  libcxxabiDylibRiscv64Build = libcxxabiDylibBuild.override riscv64Userland;
  libcxxDylibRiscv64Build = libcxxDylibBuild.override (riscv64Userland // {
    libcxxabiDylib = libcxxabiDylibRiscv64Build;
  });
  icuCoreRiscv64Build = icuCoreBuild.override (riscv64Userland // {
    libcxxabiDylib = libcxxabiDylibRiscv64Build;
    libcxxDylib = libcxxDylibRiscv64Build;
  });
  libobjcRiscv64Build = libobjcBuild.override (riscv64Userland // {
    libcxxabiDylib = libcxxabiDylibRiscv64Build;
    libcxxDylib = libcxxDylibRiscv64Build;
  });

  # base system: the arm64 minimal set (nix/arm64.nix) built by the riscv64 toolchain
  mkRiscv64Build = file: deps:
    let f = import file;
    in pkgs.callPackage f (builtins.intersectAttrs (builtins.functionArgs f)
      (riscv64Userland // {
        inherit nativeLd;
        # c++ recipes that link with -nostdlib take the runtime explicitly
        libcxxDylib = libcxxDylibRiscv64Build;
        libcxxabiDylib = libcxxabiDylibRiscv64Build;
      }) // deps);

  coreFoundationRiscv64Build = coreFoundationBuild.override (riscv64Userland // {
    icu = icuCoreRiscv64Build;
    libobjc = libobjcRiscv64Build;
  });
  libffiRiscv64Build = mkRiscv64Build ./pkgs/x11/xorg-cross-lib.nix {
    pname = "puredarwin-libffi";
    version = pkgs.libffi.version;
    src = pkgs.libffi.src;
    # the riscv assembly is elf only, mach-o symbol names and sys_icache_invalidate for darwin
    patches = [ ./pkgs/base/libffi-riscv64-darwin.patch ];
    configureFlags = [
      "--disable-docs"
      "--disable-multi-os-directory"
    ];
  };
  libxml2Riscv64Build = mkRiscv64Build ./pkgs/base/libxml2.nix {
    inherit (pkgs) libxml2 meson ninja python3 git;
  };
  foundationRiscv64Build = mkRiscv64Build ./pkgs/apple/foundation.nix {
    libobjc = libobjcRiscv64Build;
    corefoundation = coreFoundationRiscv64Build;
    libffi = libffiRiscv64Build;
    libxml2 = libxml2Riscv64Build;
    dnssdInclude = ../src/Libraries/mDNSResponder/mDNSShared;
    notifyInclude = ../src/Libraries/XPC/notify;
    src = "${foundationSource}/src/Frameworks/Foundation";
  };
  # IOKitCF compiles against these headers, so they have to be the riscv64 ones too
  iokitCFStaticRiscv64Build = iokitCFStaticBuild.override {
    puredarwinArch = "riscv64";
    inherit riscv64CrossToolchain;
    extraCmakeFlags = [
      "-DPUREDARWIN_ENABLE_IOKITCF=ON"
      "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationRiscv64Build}"
      "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjcRiscv64Build}"
      "-DPUREDARWIN_FOUNDATION_PREFIX=${foundationRiscv64Build}"
    ];
  };
  iokitRiscv64Build = iokitBuild.override (riscv64Userland // {
    corefoundation = coreFoundationRiscv64Build;
    iokitCFStatic = iokitCFStaticRiscv64Build;
    libobjc = libobjcRiscv64Build;
    foundation = foundationRiscv64Build;
  });
  launchdRiscv64Build = launchdBuild.override (riscv64Userland // {
    corefoundation = coreFoundationRiscv64Build;
    iokit = iokitRiscv64Build;
  });
  launchctlRiscv64Build = launchctlBuild.override (riscv64Userland // {
    corefoundation = coreFoundationRiscv64Build;
    iokit = iokitRiscv64Build;
  });
  # the userlandBuild cli targets without the xorg ddx drivers, like userlandArm64Sg2002Build
  userlandRiscv64Build = userlandBuild.override {
    pname = "puredarwin-userland-riscv64";
    puredarwinArch = "riscv64";
    inherit riscv64CrossToolchain;
    prebuiltLibSystem = libSystemRiscv64Build;
    xorgDriverIncludes = null;
    buildTargets = [ "sw_vers" "ps" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "cpuctl" "mean" "reboot" "halt" "poweroff" "shutdown" "netsetup" "wslinit" "ping" "pcmplay" "startx" "mousemon" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" ]
      ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
      ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ];
  };

  # the shell and toolbox the arm64 minimal image carries
  libiconvRiscv64Build = libiconvBuild.override riscv64Userland;
  ncursesRiscv64Build = ncursesBuild.override riscv64Userland;
  zshRiscv64Build = zshBuild.override (riscv64Userland // {
    ncurses = ncursesRiscv64Build;
    libiconv = libiconvRiscv64Build;
  });
  zlibRiscv64Build = mkRiscv64Build ./pkgs/x11/xvfb-zlib.nix { inherit (pkgs) zlib; };
  toyboxRiscv64Build = mkRiscv64Build ./pkgs/base/toybox.nix { zlib = zlibRiscv64Build; };
  # no gl or x11 stack on riscv64 yet, the same report arm64 boards without gl get
  fastfetchRiscv64Build = mkRiscv64Build ./pkgs/apps/fastfetch.nix {
    fastfetch = pkgs.fastfetch;
    corefoundation = coreFoundationRiscv64Build;
    foundation = foundationRiscv64Build;
    libobjc = libobjcRiscv64Build;
    iokit = iokitRiscv64Build;
    openglFramework = null;
    mesa = null;
    glu = null;
    withOpenGL = false;
    withX11 = false;
  };

  # same composition order as splitBaseSystemArm64VirtMinimal
  splitBaseSystemRiscv64VirtMinimal = pkgs.runCommand "puredarwin-basesystem-riscv64-virt-minimal-0.1" { } ''
    mkdir -p "$out"
    cp -a ${libSystemRiscv64Build}/. "$out/"
    chmod -R u+w "$out"
    # launchd links CoreFoundation and IOKit, which pull in the rest of the runtime
    for p in ${icuCoreRiscv64Build} ${libcxxabiDylibRiscv64Build} ${libcxxDylibRiscv64Build} \
      ${libobjcRiscv64Build} ${coreFoundationRiscv64Build} ${foundationRiscv64Build} \
      ${iokitRiscv64Build} ${launchdRiscv64Build} ${launchctlRiscv64Build}; do
      cp -a "$p"/. "$out/"
      chmod -R u+w "$out"
    done
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
    for p in ${userlandRiscv64Build} ${kernelRiscv64VirtDebugBuild} ${kextsRiscv64Build}; do
      cp -a "$p"/. "$out/"
      chmod -R u+w "$out"
    done
  '';

  # the loader cannot scan the disk, so the root partition carries a fixed uuid that boot-args
  # names, upper case since IOMedia publishes it the way xnu's uuid_unparse spells it
  riscv64VirtRootUUID = "6F1C2B8E-4D3A-4E5F-9A7B-1C2D3E4F5A6B";
  riscv64VirtBootArgs = "-v debug=0x14e serial=3 keepsyms=1 wdt=-1 boot-uuid=${riscv64VirtRootUUID}";

  imageRiscv64VirtMinimalBuild = pkgs.callPackage ../image.nix {
    baseSystem = splitBaseSystemRiscv64VirtMinimal;
    extraPackages = [ zshRiscv64Build libiconvRiscv64Build toyboxRiscv64Build fastfetchRiscv64Build ];
    kc = kcRiscv64VirtDebugBuild;
    xnuLoader = null;
    apfsprogs = pkgs.apfsprogs;
    espMB = 64;
    rootMB = 512;
    rootUUID = riscv64VirtRootUUID;
    imageFileName = "puredarwin-riscv64-virt-minimal.img";
    bootArgs = riscv64VirtBootArgs;
  };

  # the loader reads EFI/BOOT/kernel and EFI/BOOT/boot-args.txt from its initrd
  initrdRiscv64VirtBuild = pkgs.runCommand "puredarwin-initrd-riscv64-virt-0.1" {
    nativeBuildInputs = [ pkgs.cpio ];
  } ''
    mkdir -p root/EFI/BOOT "$out"
    cp ${kcRiscv64VirtDebugBuild}/kernel root/EFI/BOOT/kernel
    printf '%s' ${lib.escapeShellArg riscv64VirtBootArgs} > root/EFI/BOOT/boot-args.txt
    (cd root && find EFI | sort | cpio -o -H newc --reproducible --quiet) > "$out/initrd.cpio"
  '';

  # boots the minimal image on qemu virt, the disk is a writable copy in the state dir
  runRiscv64Virt = pkgs.writeShellApplication {
    name = "puredarwin-riscv64-virt";
    runtimeInputs = [ pkgs.qemu pkgs.coreutils ];
    text = ''
      state_dir="''${PUREDARWIN_RISCV64_VM_STATE_DIR:-$PWD/.puredarwin-riscv64-virt}"
      image="''${PUREDARWIN_IMAGE:-$state_dir/puredarwin-riscv64-virt-minimal.img}"
      loader="''${PUREDARWIN_LOADER:-${if xnuLoaderRiscv64 == null
        then throw "riscv64 runner: build with --override-input xnu-loader path:$HOME/development/darwin/xnu-loader"
        else "${xnuLoaderRiscv64}/boot/xnu-loader.Image"}}"
      initrd="''${PUREDARWIN_INITRD:-${initrdRiscv64VirtBuild}/initrd.cpio}"

      mkdir -p "$state_dir"
      if [ "''${PUREDARWIN_RISCV64_RESET_DISK:-0}" = 1 ] || [ ! -e "$image" ]; then
        rm -f "$image"
        cp --sparse=always ${imageRiscv64VirtMinimalBuild}/puredarwin-riscv64-virt-minimal.img "$image"
        chmod u+w "$image"
      fi

      exec qemu-system-riscv64 \
        -M virt \
        -smp "''${PUREDARWIN_VM_SMP:-2}" \
        -m "''${PUREDARWIN_VM_MEMORY:-2G}" \
        -nographic \
        -bios default \
        -kernel "$loader" \
        -initrd "$initrd" \
        -drive if=none,id=system,file="$image",format=raw \
        -device virtio-blk-pci,drive=system \
        -device virtio-net-pci,netdev=net0 \
        -netdev user,id=net0 \
        -no-reboot \
        "$@"
    '';
  };

  kcRiscv64VirtDebugBuild = pkgs.callPackage ./pkgs/toolchain/kc-riscv64.nix {
    kernel = kernelRiscv64VirtDebugBuild;
    inherit kernelSource kcTools kextNames;
    kexts = kextsRiscv64Build;
  };
in
{
  inherit
    kextNames
    kernelRiscv64VirtDebugBuild
    kextsRiscv64Build
    kcRiscv64VirtDebugBuild
    libSystemRiscv64Build
    icuCoreRiscv64Build
    libcxxabiDylibRiscv64Build
    libcxxDylibRiscv64Build
    libobjcRiscv64Build
    coreFoundationRiscv64Build
    libffiRiscv64Build
    libxml2Riscv64Build
    foundationRiscv64Build
    iokitCFStaticRiscv64Build
    iokitRiscv64Build
    launchdRiscv64Build
    launchctlRiscv64Build
    userlandRiscv64Build
    libiconvRiscv64Build
    ncursesRiscv64Build
    zshRiscv64Build
    toyboxRiscv64Build
    fastfetchRiscv64Build
    splitBaseSystemRiscv64VirtMinimal
    imageRiscv64VirtMinimalBuild
    initrdRiscv64VirtBuild
    runRiscv64Virt
    ;
}
