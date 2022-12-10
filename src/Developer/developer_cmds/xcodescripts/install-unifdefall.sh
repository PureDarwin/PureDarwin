set -e -x

BINDIR="$DSTROOT"/"$DT_TOOLCHAIN_DIR"/usr/bin
MANDIR="$DSTROOT"/"$DT_TOOLCHAIN_DIR"/usr/share/man/man1

ln -f "$MANDIR"/unifdef.1 "$MANDIR"/unifdefall.1
install -c -o root -g wheel -m 0755 \
	"$PROJECT_DIR"/unifdef/unifdefall.sh \
	"$BINDIR"/unifdefall
