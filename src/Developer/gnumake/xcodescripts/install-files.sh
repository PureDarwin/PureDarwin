set -e -x

BINDIR="$DSTROOT/$DEVELOPER_DIR"/usr/bin
ln "$BINDIR"/gnumake "$BINDIR"/make

MANDIR="$DSTROOT/$DEVELOPER_DIR"/usr/share/man/man1
install -d -o root -g wheel -m 0755 "$MANDIR"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/make.1 \
	"$MANDIR"
ln "$MANDIR"/make.1 "$MANDIR"/gnumake.1

OSV="$DSTROOT/$DEVELOPER_DIR"/usr/local/OpenSourceVersions
OSL="$DSTROOT/$DEVELOPER_DIR"/usr/local/OpenSourceLicenses
install -d -o root -g wheel -m 0755 "$OSV"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/gnumake.plist \
	"$OSV"
install -d -o root -g wheel -m 0755 "$OSL"
install -c -o root -g wheel -m 0644 \
	"$PROJECT_DIR"/COPYING \
	"$OSL"/gnumake.txt
