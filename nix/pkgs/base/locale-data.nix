{ stdenv, lib, fetchurl, bison, flex, db

# PureDarwin's libc headers, for the ctype bits and rune magic the tools write
, libcInclude ? ../../../src/Libraries/libSystem/libc/include
}:

# /usr/share/locale as Apple's Libc reads it: LC_CTYPE rune files from adv_cmds mklocale,
# LC_COLLATE from colldef, the rest as text. The tools are Apple's adv_cmds-163, the last
# with mklocale and colldef, built for the build host. The sources are FreeBSD 7.4's, which
# Apple's usr-share-locale was made from.
let
  advCmds = fetchurl {
    url = "https://github.com/apple-oss-distributions/adv_cmds/archive/refs/tags/adv_cmds-163.tar.gz";
    sha256 = "03ygi30pnsir5qa3nbrcwzq8hcavdp5n68hsnqlj3glwnl07rnk6";
  };
  fbsd = path: sha256: fetchurl {
    url = "https://raw.githubusercontent.com/freebsd/freebsd-src/releng/7.4/share/${path}";
    inherit sha256;
  };
  utf8Src = fbsd "mklocale/UTF-8.src" "0qvhi2yj96njglyfc1awmzld8z8rakwfcf21qgwwnmbjib3pw8lf";
  collateSrc = fbsd "colldef/la_LN.US-ASCII.src" "1z0nl3pjvxzpvl2czr6xaa6xqknrybq8vfhgm6fmpckgi2c0rb40";
  numericSrc = fbsd "numericdef/en_US.ISO8859-1.src" "1h2vwcz5vbl0r1l8fwinazrwh4amjv6n4fcy1jy3ks39maciy9iq";
  monetarySrc = fbsd "monetdef/en_US.ISO8859-1.src" "05qxzj79f2s4qg1wqx0kn17svgn5s78a1f3fca06vph8vsffq3sh";
  messagesSrc = fbsd "msgdef/en_US.ISO8859-1.src" "1nq021fq0rw7nj5a03g2q83cv1khbqsagxarjh9xjb9hi742i5a0";
  timeSrc = fbsd "timedef/en_US.ISO8859-1.src" "1d2a72vnbi0m365d3hannh8j2a7p3dfnwwi65h1bz2xhlpyszpdy";
in
stdenv.mkDerivation {
  pname = "puredarwin-locale-data";
  version = "0.1";
  dontUnpack = true;
  nativeBuildInputs = [ bison flex ];

  buildPhase = ''
    runHook preBuild
    tar -xzf ${advCmds}
    adv=adv_cmds-adv_cmds-163

    # the Apple definitions the tools need, from PureDarwin's own headers
    {
      echo '#pragma once'
      echo '#include <stdint.h>'
      echo '#include <wchar.h>'
      grep -h '^#define[[:space:]]*_CTYPE_' ${libcInclude}/_ctype.h
      grep -h 'define[[:space:]]*_RUNE_MAGIC_A\|define[[:space:]]*_CACHED_RUNES' ${libcInclude}/runetype.h
      echo 'typedef wchar_t __darwin_wchar_t;'
      echo '#define __printflike(a, b) __attribute__((format(printf, a, b)))'
      echo '#define __dead2 __attribute__((noreturn))'
      echo '#define __FBSDID(x)'
    } > apple.h
    cflags="-std=gnu89 -O2 -w -D__APPLE__ -include $PWD/apple.h"

    mkdir mk && cp $adv/mklocale/* mk/
    (cd mk && bison -y -d yacc.y && flex lex.l && $CC $cflags -o mklocale y.tab.c lex.yy.c)

    # colldef keeps its tables in dbopen databases, Berkeley DB's 1.85 interface provides them
    mkdir cd
    cp $adv/colldef/parse.y $adv/colldef/scan.l $adv/colldef/common.h $adv/colldef/locale/collate.h cd/
    echo '#include <db_185.h>' > cd/db.h
    echo 'int yywrap(void) { return 1; }' > cd/wrap.c
    (cd cd && bison -y -d parse.y && flex scan.l && \
      $CC $cflags -I. -I${db.dev}/include -o colldef y.tab.c lex.yy.c wrap.c -L${db.out}/lib -ldb -Wl,-rpath,${db.out}/lib)

    mk/mklocale -o LC_CTYPE.utf8 ${utf8Src}
    cd/colldef -o LC_COLLATE.ascii ${collateSrc}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    l=$out/usr/share/locale
    strip() { grep -v '^#' "$1"; }

    # UTF-8 holds only LC_CTYPE, as on macOS: setlocale(LC_CTYPE, "UTF-8") works, LC_ALL does not
    install -Dm644 LC_CTYPE.utf8 $l/UTF-8/LC_CTYPE

    mkdir -p $l/en_US.UTF-8/LC_MESSAGES
    ln -s ../UTF-8/LC_CTYPE $l/en_US.UTF-8/LC_CTYPE
    install -m644 LC_COLLATE.ascii $l/en_US.UTF-8/LC_COLLATE
    strip ${numericSrc} > $l/en_US.UTF-8/LC_NUMERIC
    strip ${monetarySrc} > $l/en_US.UTF-8/LC_MONETARY
    strip ${messagesSrc} > $l/en_US.UTF-8/LC_MESSAGES/LC_MESSAGES
    strip ${timeSrc} > $l/en_US.UTF-8/LC_TIME

    # C.UTF-8 is C with UTF-8 characters, as newer macOS has it: C values for the other categories
    mkdir -p $l/C.UTF-8/LC_MESSAGES
    ln -s ../UTF-8/LC_CTYPE $l/C.UTF-8/LC_CTYPE
    ln -s ../en_US.UTF-8/LC_COLLATE $l/C.UTF-8/LC_COLLATE
    printf '.\n\n\n' > $l/C.UTF-8/LC_NUMERIC
    printf '\n\n\n\n\n\n\n-1\n-1\n-1\n-1\n-1\n-1\n-1\n-1\n' > $l/C.UTF-8/LC_MONETARY
    printf '^[yY]\n^[nN]\n' > $l/C.UTF-8/LC_MESSAGES/LC_MESSAGES
    # the C time names are the English ones, only the formats differ
    strip ${timeSrc} | sed -e '40s|.*|%m/%d/%y|' -e '41s|.*|%a %b %e %H:%M:%S %Y|' -e '44s|.*|%a %b %e %H:%M:%S %Z %Y|' \
      > $l/C.UTF-8/LC_TIME

    # the system-wide language and region, CFLocale and bundle lookup read them from the global domain
    install -Dm644 /dev/stdin $out/Library/Preferences/.GlobalPreferences.plist <<'EOF'
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
    	<key>AppleLanguages</key>
    	<array>
    		<string>en</string>
    	</array>
    	<key>AppleLocale</key>
    	<string>en_US</string>
    </dict>
    </plist>
    EOF
    runHook postInstall
  '';

  meta.description = "Locale data for PureDarwin's libc and the default language preferences";
}
