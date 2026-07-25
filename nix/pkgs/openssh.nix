{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, openssh
, openssl
, zlib
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
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

    # The bring-up passwd(1) stores "plain:<password>" until PureDarwin has
    # crypt(3)/shadow plumbing. Keep this compatibility inside OpenSSH so libc
    # does not gain process-wide plaintext password semantics.
    substituteInPlace openbsd-compat/xcrypt.c \
      --replace-fail 'char *crypted;' 'char *crypted;

	if (salt != NULL && strncmp(salt, "plain:", 6) == 0)
		return strcmp(password, salt + 6) == 0 ? (char *)salt : (char *)"";'
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"
    export AR="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar"
    export RANLIB="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-strip"
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
      --host=x86_64-apple-darwin20.4 \
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

# PureDarwin bring-up defaults. Revisit once accounts, shadow passwords, and
# service management are complete.
PermitRootLogin yes
PasswordAuthentication yes
EOF
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
