{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, openssh
, openssl
, zlib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-openssh";
  inherit (openssh) version;
  src = openssh.src;

  postPatch = ''
    substituteInPlace openbsd-compat/bsd-setres_id.h \
      --replace-fail '#ifndef HAVE_SETRESGID
int	setresgid(gid_t, gid_t, gid_t);
#endif
#ifndef HAVE_SETRESUID
int	setresuid(uid_t, uid_t, uid_t);
#endif' '#if !defined(HAVE_SETRESGID) || defined(__APPLE__)
int	setresgid(gid_t, gid_t, gid_t);
#endif
#if !defined(HAVE_SETRESUID) || defined(__APPLE__)
int	setresuid(uid_t, uid_t, uid_t);
#endif'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export CPPFLAGS="-I${libSystem}/pd-guest-headers -I${openssl}/include -I${zlib}/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -DPLATFORM_MacOSX -D_DARWIN_C_SOURCE -Wno-nullability-completeness"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${openssl}/lib -L${zlib}/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"
    export LIBS="-Wl,-force_load,${openssl}/lib/libcrypto.a -Wl,-force_load,${zlib}/lib/libz.a -lSystem"

    export ac_cv_func_bcopy=yes
    export ac_cv_func_bzero=yes
    export ac_cv_func_b64_ntop=no
    export ac_cv_func___b64_ntop=no
    export ac_cv_func_b64_pton=no
    export ac_cv_func___b64_pton=no
    export ac_cv_func_arc4random=yes
    export ac_cv_func_arc4random_buf=yes
    export ac_cv_func_arc4random_stir=yes
    export ac_cv_func_arc4random_uniform=yes
    export ac_cv_func_bcrypt_pbkdf=no
    export ac_cv_func_closefrom=no
    export ac_cv_func_close_range=no
    export ac_cv_func_explicit_bzero=no
    export ac_cv_func_freezero=no
    export ac_cv_func_fstatvfs=yes
    export ac_cv_func_getaddrinfo=yes
    export ac_cv_func_getentropy=yes
    export ac_cv_func_getrandom=no
    export ac_cv_func_getnameinfo=yes
    export ac_cv_func_getpagesize=yes
    export ac_cv_func_getpeereid=no
    export ac_cv_func_getpeerucred=no
    export ac_cv_func_getspnam=no
    export ac_cv_func_gettimeofday=yes
    export ac_cv_func_innetgr=no
    export ac_cv_func_login_getcapbool=no
    export ac_cv_func_login_getpwclass=no
    export ac_cv_func_mmap=yes
    export ac_cv_func_proc_pidinfo=no
    export ac_cv_func_reallocarray=no
    export ac_cv_func_realpath=yes
    export ac_cv_func_recallocarray=no
    export ac_cv_func_rresvport_af=yes
    export ac_cv_func_sandbox_init=no
    export ac_cv_func_select=yes
    export ac_cv_func_setenv=yes
    export ac_cv_func_setlogin=no
    export ac_cv_func_setproctitle=no
    export ac_cv_func_sigaction=yes
    export ac_cv_func_snprintf=yes
    export ac_cv_func_statvfs=yes
    export ac_cv_func_strlcpy=yes
    export ac_cv_func_strmode=no
    export ac_cv_func_strnvis=no
    export ac_cv_func_strtonum=no
    export ac_cv_func_sysconf=yes
    export ac_cv_func_unsetenv=yes
    export ac_cv_func_vsnprintf=yes
    export ac_cv_func_EVP_CIPHER_CTX_get_iv=no
    export ac_cv_func_EVP_CIPHER_CTX_get_updated_iv=no
    export ac_cv_func_EVP_CIPHER_CTX_set_iv=no
    export ac_cv_have_decl_HOWMANY=no
    export ac_cv_member_struct_passwd_pw_gecos=yes
    export ac_cv_member_struct_passwd_pw_class=yes
    export ac_cv_member_struct_passwd_pw_change=yes
    export ac_cv_member_struct_passwd_pw_expire=yes

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --bindir=/bin \
      --sbindir=/sbin \
      --libexecdir=/libexec \
      --sysconfdir=/etc/ssh \
      --with-privsep-path=/var/empty \
      --with-ssl-dir=${openssl} \
      --with-zlib=${zlib} \
      --without-openssl-header-check \
      --without-pam \
      --without-kerberos5 \
      --without-ldns \
      --without-libedit \
      --without-audit \
      --without-selinux \
      --disable-utmp \
      --disable-utmpx \
      --disable-wtmp \
      --disable-wtmpx \
      --disable-lastlog

    substituteInPlace Makefile \
      --replace-fail '$(INSTALL) -m 4711 $(STRIP_OPT) ssh-keysign$(EXEEXT) $(DESTDIR)$(SSH_KEYSIGN)$(EXEEXT)' \
                     '$(INSTALL) -m 0755 $(STRIP_OPT) ssh-keysign$(EXEEXT) $(DESTDIR)$(SSH_KEYSIGN)$(EXEEXT)'

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install-nokeys DESTDIR=$out STRIP_OPT=
    cat >> $out/etc/ssh/sshd_config <<'EOF'

# Root is the only real account on a PureDarwin image, so refusing its login
# would leave nobody able to connect. Password auth needs a password set with
# passwd(1) first: the shipped /etc/master.passwd has '*' for every account,
# which no crypt(3) output can match.
PermitRootLogin yes
PasswordAuthentication yes

# launchd (pid 1) puts DYLD_INSERT_LIBRARIES=/usr/lib/libobjc.A.dylib in its
# environment so that every descendant has libobjc mapped: libSystem has
# undefined objc_* references (libdispatch's object.m) that dyld resolves by
# flat-namespace lookup, and it cannot depend on libobjc directly without
# breaking dyld's "libSystem initializes first" rule.
#
# sshd does not inherit its environment into the session - session.c builds a
# fresh one - so without this the login shell starts with no libobjc mapped and
# dyld fails it with "Symbol not found: ___objc_personality_v0". SetEnv is the
# supported hook for putting it back.
SetEnv DYLD_INSERT_LIBRARIES=/usr/lib/libobjc.A.dylib
EOF

    # install-nokeys skips host key generation on purpose - keys made at image
    # build time would be identical on every install, and ssh-keygen is a
    # target binary this build host cannot run anyway. Generate on first boot.
    mkdir -p $out/usr/libexec $out/System/Library/LaunchDaemons
    cat > $out/usr/libexec/pd-sshd <<'EOF'
#!/bin/sh
# launchctl's system bootstrap empties /var/run at boot, so sshd's pid file has
# nowhere to go unless the directory is recreated here.
/bin/mkdir -p /var/run

# Privilege separation: sshd exits if this is missing, or is owned by anyone
# but root, or is group/world writable.
/bin/mkdir -p /var/empty

if ! test -f /etc/ssh/ssh_host_ed25519_key; then
    /bin/ssh-keygen -A
fi

# -D keeps sshd in the foreground so launchd can supervise it; -e sends the log
# to stderr, which the plist captures, instead of syslog.
exec /sbin/sshd -D -e
EOF
    chmod +x $out/usr/libexec/pd-sshd

    cat > $out/System/Library/LaunchDaemons/org.puredarwin.sshd.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>org.puredarwin.sshd</string>
	<key>ProgramArguments</key>
	<array>
		<string>/usr/libexec/pd-sshd</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>KeepAlive</key>
	<true/>
	<key>StandardOutPath</key>
	<string>/var/log/sshd.log</string>
	<key>StandardErrorPath</key>
	<string>/var/log/sshd.log</string>
</dict>
</plist>
EOF
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
