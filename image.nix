{ stdenv
, lib
, baseSystem
, extraPackages ? [ ]
, kc
, xnuLoader
, gptfdisk
, util-linux
, dosfstools
, mtools
, e2fsprogs
, apfsprogs
, hfsprogs ? null
, libdmg-hfsplus ? null
, cacert
, espMB ? 64
, rootMB ? 640
, apfsMB ? 128
  # "ext4": ext4 root + APFS test partition (default, historical layout).
  # "hfs":  EFI + HFS+ root ONLY - no ext4, no APFS. Root is mounted by the
  #         stock hfs.kext instead of our ext4.kext; xnu-loader already
  #         prefers an Apple_HFS partition when deriving boot-uuid.
, rootFsType ? "ext4"
, testAudioFile ? null
, imageFileName ? "puredarwin.img"
  # xnu-loader reads this off the ESP at \EFI\BOOT\boot-args.txt
  # it falls back if it cannot find a boot-args.txt, so not strictly needed here
  # but generally nice to have so we can override things easily now
, bootArgs ? "-v debug=0x219 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1"
}:

assert lib.isDerivation baseSystem;
assert lib.all lib.isDerivation extraPackages;
assert rootFsType == "ext4" || rootFsType == "hfs";
assert rootFsType == "hfs" -> (hfsprogs != null && libdmg-hfsplus != null);

stdenv.mkDerivation {
  pname = "puredarwin-image";
  version = "0.1";

  dontUnpack = true;

  nativeBuildInputs = [ gptfdisk util-linux dosfstools mtools e2fsprogs apfsprogs ]
    ++ lib.optionals (rootFsType == "hfs") [ hfsprogs libdmg-hfsplus ];

  buildPhase = ''
    runHook preBuild
    export MTOOLS_SKIP_CHECK=1
    LINUX_FS_GUID=0FC63DAF-8483-4772-8E79-3D69D8477DE4
    APFS_GUID=7C3457EF-0000-11AA-AA11-00306543ECAC
    # Apple_HFS: xnu-loader scans GPT for this type GUID and derives boot-uuid
    # from the volume header's FinderInfo UUID (see xnu-loader devtree.c
    # read_hfs_uuid_from_blockio); AppleFileSystemDriver then publishes
    # boot-uuid-media for the matching volume.
    HFS_GUID=48465300-0000-11AA-AA11-00306543ECAC

    img=puredarwin.img
    esp_sectors=$((${toString espMB} * 2048))
    root_sectors=$((${toString rootMB} * 2048))
${if rootFsType == "hfs" then ''
    img_sectors=$((2048 + esp_sectors + root_sectors + 2048))
    truncate -s $((img_sectors * 512)) $img
    sgdisk \
      -n 1:2048:+${toString espMB}M -t 1:EF00       -c 1:"EFI System Partition" \
      -n 2:0:+${toString rootMB}M   -t 2:$HFS_GUID  -c 2:"Darwin HFS Root" \
      $img >/dev/null

    read -r root_start root_size <<<"$(sfdisk -d $img | grep -i type=$HFS_GUID \
      | sed -E 's/.*start= *([0-9]+), *size= *([0-9]+),.*/\1 \2/')"
'' else ''
    img_sectors=$((2048 + esp_sectors + root_sectors + 2048))
    truncate -s $((img_sectors * 512)) $img
    sgdisk \
      -n 1:2048:+${toString espMB}M -t 1:EF00            -c 1:"EFI System Partition" \
      -n 2:0:+${toString rootMB}M   -t 2:$LINUX_FS_GUID  -c 2:"Darwin ext4 Root" \
      $img >/dev/null

    read -r root_start root_size <<<"$(sfdisk -d $img | grep -i type=$LINUX_FS_GUID \
      | sed -E 's/.*start= *([0-9]+), *size= *([0-9]+),.*/\1 \2/')"
''}
    read -r esp_start esp_size <<<"$(sfdisk -d $img | grep -i type=C12A7328 \
      | sed -E 's/.*start= *([0-9]+), *size= *([0-9]+),.*/\1 \2/')"

    truncate -s $((esp_size * 512)) esp.img
    mkfs.vfat -F 32 -n EFI esp.img >/dev/null
    mmd -i esp.img ::/EFI ::/EFI/BOOT
    mcopy -o -i esp.img ${xnuLoader}/img/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
    mcopy -o -i esp.img ${kc}/kernel                          ::/EFI/BOOT/kernel
    printf '%s' ${lib.escapeShellArg bootArgs} > boot-args.txt
    mcopy -o -i esp.img boot-args.txt                          ::/EFI/BOOT/boot-args.txt
    dd if=esp.img of=$img bs=512 seek=$esp_start count=$esp_size conv=notrunc status=none

    staging="$PWD/root-staging"
    mkdir -p "$staging"

    copyPackage() {
      local package="$1"

      echo "Installing $package into image staging tree"

      if [ ! -d "$package" ]; then
        echo "error: package path does not exist or is not a directory: $package" >&2
        exit 1
      fi

      cp -a -- "$package"/. "$staging"/
      chmod -R u+rwX "$staging"
    }

    copyPackage "${baseSystem}";

    ${lib.concatMapStringsSep "\n" (package: ''
      copyPackage "${package}";
    '') extraPackages}
    for dir in \
      usr \
      usr/bin \
      usr/lib \
      usr/lib/system \
      usr/sbin \
      usr/share \
      usr/share/X11 \
      bin \
      sbin \
      dev \
      etc \
      etc/fonts \
      etc/init \
      tmp \
      var \
      var/cache \
      var/cache/fontconfig \
      var/root \
      var/run \
      var/log \
      var/tmp \
      var/empty \
      tmp/.X11-unix
    do
      mkdir -p "$staging/$dir"
    done

    # Classic Darwin compatibility names: libc/libm/libpthread/libdl/libinfo
    # are all libSystem symlinks on real Darwin. tcc links "-lc" by default
    # (tcc_add_runtime); other in-guest builds ask for -lm/-lpthread. Done at
    # image assembly (not in the libsystem output) so cross-build autoconf
    # probes don't suddenly start detecting -lc and enabling new code paths.
    for compat in libc libm libpthread libdl libinfo; do
      ln -sf libSystem.B.dylib "$staging/usr/lib/$compat.dylib"
    done

    # System C headers for in-guest compilers (tcc): staged by the libsystem
    # build under pd-guest-headers/ (see build.nix) so cross-built ports never
    # see them; the guest gets them at /usr/include.
    if [ -d "$staging/pd-guest-headers" ]; then
      mkdir -p "$staging/usr/include"
      cp -a "$staging/pd-guest-headers"/. "$staging/usr/include/"
      rm -rf "$staging/pd-guest-headers"
    fi

    cat > $staging/etc/passwd <<'EOF'
root:*:0:0:System Administrator:/var/root:/bin/sh
daemon:*:1:1:System Services:/var/root:/usr/bin/false
sshd:*:74:74:Privilege-separated SSH:/var/empty:/usr/bin/false
nobody:*:-2:-2:Unprivileged User:/var/empty:/usr/bin/false
EOF
    cat > $staging/etc/group <<'EOF'
wheel:*:0:root
daemon:*:1:root
sshd:*:74:
nogroup:*:-1:
nobody:*:-2:
staff:*:20:root
EOF
    cat > $staging/etc/profile <<'EOF'
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export TERM=''${TERM:-vt220}
export SHELL=''${SHELL:-/bin/zsh}
export HOME=''${HOME:-/var/root}
export USER=''${USER:-root}
export LOGNAME=''${LOGNAME:-root}
export FONTCONFIG_FILE=''${FONTCONFIG_FILE:-/etc/fonts/fonts.conf}
export XDG_CONFIG_DIRS=''${XDG_CONFIG_DIRS:-/etc}
export XDG_DATA_DIRS=''${XDG_DATA_DIRS:-/usr/share:/share}
export PS1='# '
EOF
    cat > $staging/etc/zshenv <<'EOF'
export PATH=''${PATH:-/bin:/sbin:/usr/bin:/usr/sbin}
export TERM=''${TERM:-vt220}
export SHELL=''${SHELL:-/bin/zsh}
export HOME=''${HOME:-/var/root}
export USER=''${USER:-root}
export LOGNAME=''${LOGNAME:-root}
export FONTCONFIG_FILE=''${FONTCONFIG_FILE:-/etc/fonts/fonts.conf}
export XDG_CONFIG_DIRS=''${XDG_CONFIG_DIRS:-/etc}
export XDG_DATA_DIRS=''${XDG_DATA_DIRS:-/usr/share:/share}
EOF
    cat > $staging/etc/zprofile <<'EOF'
test -r /etc/profile && . /etc/profile
EOF
    cat > $staging/etc/zshrc <<'EOF'
[[ -o interactive ]] || return

unsetopt monitor
bindkey -e
bindkey '^?' backward-delete-char
bindkey '^H' backward-delete-char
export PS1='# '
EOF
    cat > $staging/etc/resolv.conf <<'EOF'
nameserver 10.0.2.3
EOF
    cat > $staging/etc/shells <<'EOF'
/bin/sh
/bin/zsh
EOF
    cat > $staging/etc/ttys <<'EOF'
/dev/console /bin/zsh -l
EOF
    cat > $staging/etc/init/rc.boot <<'EOF'
#!/bin/sh

${lib.optionalString (rootFsType == "hfs") ''
# The kernel mounts the root volume read-only and hfs.kext honors that
# (ext4.kext force-clears MNT_RDONLY instead, which is why the ext4 image
# never needed this). Classic Darwin does mount -uw / from /etc/rc.
# IOMediaBSDClient may not have published /dev/disk0s2 yet when rc.boot
# runs, so retry until the node shows up.
if test -x /bin/mount; then
    n=0
    while test ! -c /dev/disk0s2 -a ! -b /dev/disk0s2 -a $n -lt 50; do
        /bin/sleep 0.2
        n=$((n+1))
    done
    /bin/mount -t hfs -o update,rw /dev/disk0s2 /
fi
''}
if test -x /bin/hostname; then
    /bin/hostname puredarwin
fi

if test -x /bin/netsetup; then
    /bin/netsetup
fi
EOF
    cat > $staging/var/root/.profile <<'EOF'
test -r /etc/profile && . /etc/profile
EOF
    cat > $staging/var/root/.zprofile <<'EOF'
test -r /etc/zprofile && . /etc/zprofile
EOF
    cat > $staging/var/root/.zshrc <<'EOF'
test -r /etc/zshrc && . /etc/zshrc
EOF
    if [ -f "$staging/etc/i3/config.keycodes" ]; then
      mkdir -p "$staging/var/root/.config/i3"
      cp "$staging/etc/i3/config.keycodes" "$staging/var/root/.config/i3/config"
      chmod 755 "$staging/var/root/.config" "$staging/var/root/.config/i3"
      chmod 644 "$staging/var/root/.config/i3/config"
    elif [ -f "$staging/etc/i3/config" ]; then
      mkdir -p "$staging/var/root/.config/i3"
      cp "$staging/etc/i3/config" "$staging/var/root/.config/i3/config"
      chmod 755 "$staging/var/root/.config" "$staging/var/root/.config/i3"
      chmod 644 "$staging/var/root/.config/i3/config"
    fi
    chmod 644 \
      $staging/etc/passwd \
      $staging/etc/group \
      $staging/etc/profile \
      $staging/etc/zshenv \
      $staging/etc/zprofile \
      $staging/etc/zshrc \
      $staging/etc/resolv.conf \
      $staging/etc/shells \
      $staging/etc/ttys \
      $staging/var/root/.profile \
      $staging/var/root/.zprofile \
      $staging/var/root/.zshrc
    chmod 755 $staging/etc/init/rc.boot

    ${lib.optionalString (testAudioFile != null) ''
      cp ${testAudioFile} $staging/badapple.pcm
    ''}

    # Real Darwin's system-identity plist, read directly by lots of code
    # (CoreFoundation's system-version APIs, etc) rather than through
    # sw_vers - sw_vers itself (src/Userspace/sw_vers/sw_vers.c) still just
    # uses its own compiled-in PRODUCT_NAME/PRODUCT_VERSION constants
    # ("PureDarwin"/"11.3"), matched here so both report the same thing.
    # curl (curl.nix) is built with --with-ca-bundle=/etc/ssl/cert.pem;
    # stage a real CA bundle there so TLS verification actually has
    # something to check against.
    mkdir -p $staging/etc/ssl
    cp ${cacert}/etc/ssl/certs/ca-bundle.crt $staging/etc/ssl/cert.pem
    chmod 644 $staging/etc/ssl/cert.pem

    mkdir -p $staging/System/Library/CoreServices
    cat > $staging/System/Library/CoreServices/SystemVersion.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>ProductBuildVersion</key>
	<string>20D91</string>
	<key>ProductCopyright</key>
	<string>1983-2021 PureDarwin. All rights reserved.</string>
	<key>ProductName</key>
	<string>PureDarwin</string>
	<key>ProductUserVisibleVersion</key>
	<string>11.3</string>
	<key>ProductVersion</key>
	<string>11.3</string>
</dict>
</plist>
EOF
    chmod 644 $staging/System/Library/CoreServices/SystemVersion.plist

    # Xorg config selecting the PureDarwin GOP framebuffer driver. No input
    # autodetection (no udev/hal on PD); the driver reads its geometry live
    # from IOGOPFramebuffer, so no Modeline/resolution is needed here.
    mkdir -p $staging/etc/X11
    cat > $staging/etc/X11/xorg.conf <<'EOF'
Section "ServerFlags"
    Option "AutoAddDevices" "false"
    Option "AutoAddGPU"     "false"
    Option "AutoEnableDevices" "false"
    Option "DontVTSwitch"   "true"
    Option "AllowEmptyInput" "true"
EndSection

Section "Device"
    Identifier "GOP0"
    Driver     "puredarwingop"
EndSection

Section "Monitor"
    Identifier "Monitor0"
EndSection

Section "Screen"
    Identifier "Screen0"
    Device     "GOP0"
    Monitor    "Monitor0"
    DefaultDepth 24
EndSection

Section "InputDevice"
    Identifier "Mouse0"
    Driver     "puredarwininput"
    Option     "PDType" "mouse"
    Option     "Device" "/dev/usb_hid_mouse"
EndSection

Section "InputDevice"
    Identifier "Keyboard0"
    Driver     "puredarwininput"
    Option     "PDType" "keyboard"
    Option     "Device" "/dev/usb_hid_kbd"
EndSection

Section "ServerLayout"
    Identifier "Layout0"
    Screen     "Screen0"
    InputDevice "Mouse0" "CorePointer"
    InputDevice "Keyboard0" "CoreKeyboard"
EndSection
EOF
    chmod 644 $staging/etc/X11/xorg.conf

    for applet in \
      awk \
      chmod \
      clear \
      cmp \
      cp \
      date \
      dd \
      df \
      diff \
      du \
      env \
      expr \
      find \
      grep \
      gunzip \
      gzip \
      id \
      ln \
      ls \
      mkdir \
      mknod \
      more \
      mv \
      netcat \
      patch \
      printf \
      readlink \
      realpath \
      reset \
      rm \
      rmdir \
      sed \
      sort \
      stat \
      tar \
      touch \
      truncate \
      tty \
      vi \
      which \
      whoami \
      xargs
    do
      ln -s toybox "$staging/bin/$applet"
    done
    ln -s toybox "$staging/usr/bin/env"
    ln -sf zsh "$staging/bin/sh"

    chmod 1777 \
      "$staging/tmp" \
      "$staging/var/tmp" \
      "$staging/tmp/.X11-unix"

    chmod 700  $staging/var/root
    chmod 755  $staging/var/empty

    echo "Image X11 executables:"
    for executable in Xvfb Xorg xeyes xterm i3 i3bar i3-msg startx dmenu; do
      found=
      for candidate in \
        "$staging/bin/$executable" \
        "$staging/usr/bin/$executable"
      do
        if [ -x "$candidate" ]; then
          echo "  $candidate"
          found=1
          break
        fi
      done

      if [ -z "$found" ]; then
        echo "warning: $executable was not installed into the image" >&2
      fi
    done

    truncate -s $((root_size * 512)) root.img

${lib.optionalString (rootFsType == "hfs") ''
    # HFS+ root, built without mounting anything: mkfs.hfsplus (hfsprogs)
    # formats the flat file, then libdmg-hfsplus's hfsplus tool unpacks a
    # ustar archive of the staging tree into it (files, dirs, symlinks,
    # mode/uid/gid all come from the tar headers).
    #
    # HFS+ here is case-INSENSITIVE (hfsplus/libdmg only speak the
    # case-insensitive catalog order), so fail loudly on any staged paths
    # that would collide.
    collisions=$( (cd "$staging" && find . | tr 'A-Z' 'a-z' | sort | uniq -d) || true)
    if [ -n "$collisions" ]; then
      echo "error: case-colliding paths in staging tree (HFS+ is case-insensitive):" >&2
      echo "$collisions" >&2
      exit 1
    fi

    mkfs.hfsplus -v PureDarwin root.img >/dev/null

    printf 'PDHFSRT1' | dd of=root.img bs=1 seek=$((1024 + 104)) conv=notrunc status=none
    printf 'PDHFSRT1' | dd of=root.img bs=1 seek=$((root_size * 512 - 1024 + 104)) conv=notrunc status=none

    tar --format=ustar --owner=0 --group=0 --hard-dereference \
      -cf staging.tar -C "$staging" .
    # hfsplus is chatty (one line per file) and its ASSERTs print to stdout,
    # so capture everything and only show it on failure.
    if ! hfsplus root.img untar staging.tar > untar.log 2>&1; then
      echo "hfsplus untar failed; last lines:" >&2
      tail -40 untar.log >&2
      exit 1
    fi

    # Sanity check the populated catalog (fsck.hfsplus refuses plain files:
    # "Can't get device block size"): key paths must be listable so a populate
    # bug fails the build instead of surfacing as mystery corruption at boot.
    for path in /bin /usr/bin /usr/lib /etc; do
      hfsplus root.img ls "$path" >/dev/null
    done
    hfsplus root.img ls /usr/lib | grep -q libSystem.B.dylib
    hfsplus root.img ls /bin | grep -q zsh

    dd if=root.img of=$img bs=512 seek=$root_start count=$root_size conv=notrunc status=none
''}
${lib.optionalString (rootFsType == "ext4") ''
    # ext4.kext now maintains metadata_csum checksums, initializes
    # uninitialized (uninit_bg) block/inode groups on demand, handles 64-bit
    # descriptors (ext4_csum.c), and journals metadata with replay-on-mount
    # (ext4_jbd.c), so format with a journal. Keep ^orphan_file (no
    # orphan-file recovery support).
    mke2fs -q -F -t ext4 \
      -b 4096 \
      -O ^orphan_file \
      -L darwin-ext4 \
      -d $staging \
      root.img >/dev/null
    cat > root-debugfs.cmds <<'EOF'
set_inode_field /var/empty uid 0
set_inode_field /var/empty gid 0
set_inode_field /var/empty mode 040755
EOF
    debugfs -w -f root-debugfs.cmds root.img >/dev/null

    dd if=root.img of=$img bs=512 seek=$root_start count=$root_size conv=notrunc status=none
''}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp puredarwin.img $out/${imageFileName}
    runHook postInstall
  '';

  meta = with lib; {
    description = "Bootable PureDarwin GPT disk image (xnu-loader ESP + kc-tools kernel collection + ext4 BaseSystem root + APFS test container)";
    platforms = platforms.linux;
  };
}
