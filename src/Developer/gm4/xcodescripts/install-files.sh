set -e -x

BINDIR="$DSTROOT/$DT_TOOLCHAIN_DIR"/usr/bin
ln "$BINDIR"/gm4 "$BINDIR"/m4

MANDIR="$DSTROOT/$DT_TOOLCHAIN_DIR"/usr/share/man/man1
install -d -o root -g wheel -m 0755 "$MANDIR"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/doc/m4.1 \
	"$MANDIR"
ln "$MANDIR"/m4.1 "$MANDIR"/gm4.1

OSV="$DSTROOT"/usr/local/OpenSourceVersions
OSL="$DSTROOT"/usr/local/OpenSourceLicenses
install -d -o root -g wheel -m 0755 "$OSV"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/gm4.plist \
	"$OSV"
install -d -o root -g wheel -m 0755 "$OSL"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/COPYING \
	"$OSL"/gm4.txt
