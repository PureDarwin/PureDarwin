set -e -x

BINDIR="$DSTROOT/$DT_TOOLCHAIN_DIR"/usr/bin
install -c -o root -g wheel -m 0755 \
	"$PROJECT_DIR"/yacc.sh \
	"$BINDIR"/yacc

SHAREDIR="$DSTROOT/$DT_TOOLCHAIN_DIR"/usr/share/bison
install -d -o root -g wheel -m 0755 "$SHAREDIR"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/data/c++.m4 \
	"$PROJECT_DIR"/data/c.m4 \
	"$PROJECT_DIR"/data/glr.c \
	"$PROJECT_DIR"/data/glr.cc \
	"$PROJECT_DIR"/data/lalr1.cc \
	"$PROJECT_DIR"/data/location.cc \
	"$PROJECT_DIR"/data/README \
	"$PROJECT_DIR"/data/yacc.c \
	"$SHAREDIR"
install -d -o root -g wheel -m 0755 "$SHAREDIR"/m4sugar
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/data/m4sugar/m4sugar.m4 \
	"$SHAREDIR"/m4sugar

MANDIR="$DSTROOT/$DT_TOOLCHAIN_DIR"/usr/share/man/man1
install -d -o root -g wheel -m 0755 "$MANDIR"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/doc/bison.1 \
	"$MANDIR"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/yacc.1 \
	"$MANDIR"

OSV="$DSTROOT"/usr/local/OpenSourceVersions
OSL="$DSTROOT"/usr/local/OpenSourceLicenses
install -d -o root -g wheel -m 0755 "$OSV"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/bison.plist \
	"$OSV"
install -d -o root -g wheel -m 0755 "$OSL"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/COPYING \
	"$OSL"/bison.txt
