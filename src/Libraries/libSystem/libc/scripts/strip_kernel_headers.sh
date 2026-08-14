#!/bin/sh
#
# Strip kernel-only blocks from the installed userspace headers.
#
# (IPConfiguration does this in five files.) With the blocks present, that
# define exposes struct user32_kevent from sys/event.h, which is built from
# user32_addr_t - and that type lives behind #ifdef KERNEL in i386/types.h,
# which an earlier include has usually already passed with its include guard
# set. The result is a struct whose member types do not exist. Against Apple's
# stripped headers the idiom is simply a no-op, which is why it works there.
#
# unifdef exits 0 when a file was unchanged and 1 when it was modified; only 2
# means a real error, so anything above 1 is fatal here.

set -u

INCDIR="$1"
UNIFDEF="${2:-unifdef}"

[ -d "$INCDIR" ] || exit 0

find "$INCDIR" -name '*.h' -print | while read -r header; do
	grep -q -e KERNEL_PRIVATE -e 'ifdef[[:space:]]*KERNEL' \
	        -e 'defined[[:space:]]*(*KERNEL' "$header" || continue

	chmod u+w "$header" || exit 1
	cp "$header" "$header.orig" || exit 1

	"$UNIFDEF" -UKERNEL -UKERNEL_PRIVATE -UBSD_KERNEL_PRIVATE \
	           -UXNU_KERNEL_PRIVATE -UMACH_KERNEL_PRIVATE \
	           "$header.orig" > "$header"
	status=$?
	if [ "$status" -gt 1 ]; then
		echo "strip_kernel_headers: unifdef failed on $header" >&2
		mv "$header.orig" "$header"
		exit 1
	fi

	rm -f "$header.orig"
done

exit 0
