#!/bin/sh
# pdinstall: installs PureDarwin from the running system onto another disk
# the app reads its stdout, one line each: "step <name>", "progress <0-100>", "run <command>", "error <text>", "done <text>"

# the app starts this from the desktop session, whose PATH may not have the disk tools
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH
export PATH

LOG=/tmp/pdinstall.log
TARGET=/Volumes/PDInstall
ESP=/Volumes/PDInstallEFI
BOOTESP=/Volumes/PDBootEFI
STANDALONE=/usr/standalone/PureDarwin

say() { echo "$*"; echo "$*" >> "$LOG"; }
fail() { say "error $*"; cleanup; exit 1; }

# runs a command, logging it and its output
run() {
	say "run $*"
	"$@" >> "$LOG" 2>&1 || fail "$* exited with $?"
}

cleanup() {
	umount "$BOOTESP" 2>/dev/null
	umount "$ESP" 2>/dev/null
	umount "$TARGET" 2>/dev/null
}

usage() {
	cat >&2 <<EOF
usage: pdinstall disks
       pdinstall erase <disk> [-y]        the whole disk, a new GPT with an EFI system partition
       pdinstall free <disk> [-y]         the largest free gap, beside what is already on the disk
EOF
	exit 2
}

# the disk holding the running root filesystem, e.g. disk1
boot_disk() {
	root=$(stat -c %D /)
	for n in /dev/disk*s*; do
		[ -b "$n" ] || continue
		dev=$(printf '%x' $(( (0x$(stat -c %t "$n") << 24) | 0x$(stat -c %T "$n") )))
		if [ "$dev" = "$root" ]; then
			basename "$n" | sed 's/s[0-9]*$//'
			return
		fi
	done
	# a live system runs from a ramdisk, its medium is the disk whose EFI partition carries ramdisk.img
	probe=/Volumes/PDProbe
	mkdir -p "$probe"
	for n in /dev/disk*s*; do
		[ -b "$n" ] || continue
		mount -t msdos -o rdonly "$n" "$probe" 2>/dev/null || continue
		found=""
		[ -f "$probe/ramdisk.img" ] && found=1
		umount "$probe"
		if [ -n "$found" ]; then
			basename "$n" | sed 's/s[0-9]*$//'
			return
		fi
	done
}

# whole disks from the registry: name, size in bytes, removable
whole_disks() {
	# properties come in any order, so an entry is printed when the next one starts
	ioreg -r -c IOMedia -l | awk '
		function flush() { if (whole == "Yes" && name != "") print name, size, rem, wr; whole = name = size = rem = wr = "" }
		/\+-o / { flush() }
		/"Whole" = / { whole = $NF }
		/"Size" = / { size = $NF }
		/"Removable" = / { rem = $NF }
		/"Writable" = / { wr = $NF }
		/"BSD Name" = / { name = $NF; gsub(/"/, "", name) }
		END { flush() }
	'
}

# the free gaps of a disk in 512 byte blocks, largest first: start size
free_gaps() {
	gpt show "$1" 2>/dev/null | awk '$3 == "Unused" && $1 ~ /^[0-9]+$/ && $2 ~ /^[0-9]+$/ { print $1, $2 }' | sort -k2 -n -r
}

# partition indices whose type gpt names with the given text, e.g. "EFI System"
parts_of_type() {
	gpt show "$1" 2>/dev/null | awk -v t="$2" 'index($0, t) && $3 ~ /^[0-9]+$/ { print $3 }'
}

cmd_disks() {
	boot=$(boot_disk)
	whole_disks | while read -r name size rem wr; do
		flag=""
		[ "$name" = "$boot" ] && flag=" boot"
		[ "$rem" = "Yes" ] && flag="$flag removable"
		# a CD, or a stick written read-only, can only be the medium we started from
		[ "$wr" = "No" ] && flag="$flag readonly"
		echo "disk $name $size$flag"
		gpt show "$name" 2>/dev/null | awk -v d="$name" '$3 ~ /^[0-9]+$/ { $1 = $1; print "part " d "s" $3, $0 }'
		free_gaps "$name" | head -1 | awk -v d="$name" '{ print "free " d, $1, $2 * 512 }'
	done
}

confirm() {
	[ "$2" = "-y" ] && return
	printf '%s, type yes to go on: ' "$1" >&2
	read -r ok
	[ "$ok" = "yes" ] || exit 1
}

check_disk() {
	[ -n "$1" ] || usage
	[ -c "/dev/r$1" ] || fail "no disk $1"
	[ "$1" != "$(boot_disk)" ] || fail "$1 holds the running system"
}

# the running system onto the mounted target: everything at the top of / except what only exists while running
copy_system() {
	say "step Copying the system"
	say "progress 15"
	set --
	for e in /* /.[!.]*; do
		[ -e "$e" ] || [ -L "$e" ] || continue
		case "$e" in
		/dev|/Volumes|/lost+found|/tmp|/private) ;;
		*) set -- "$@" "${e#/}" ;;
		esac
	done
	say "run tar -C / -cf - $* | tar -C $TARGET -xpf -"
	(tar -C / -cf - "$@" | tar -C "$TARGET" -xpf -) >> "$LOG" 2>&1 || fail "copying the system failed"
	say "progress 70"
	# /private without the running system's temporary and runtime state
	(cd / && tar -cf - --exclude private/tmp/* --exclude private/var/run/* --exclude private/var/tmp/* \
	    --exclude private/var/vm/* private) | tar -C "$TARGET" -xpf - >> "$LOG" 2>&1 || fail "copying /private failed"
	[ -L /tmp ] && ln -s "$(readlink /tmp)" "$TARGET/tmp" || mkdir -p "$TARGET/tmp"
	mkdir -p "$TARGET/dev" "$TARGET/Volumes" "$TARGET/private/tmp" "$TARGET/private/var/run"
	chmod 1777 "$TARGET/private/tmp"
	# the installed system starts the desktop on its own, not the installer
	rm -f "$TARGET/.pd-install-media"
	say "progress 85"
}

# loader, kernel and boot-args.txt into EFI/PureDarwin, with the root named by its ext4 uuid so another
# Linux partition on the disk is never taken for it. removable=1 also fills EFI/BOOT, only on a disk we erased
install_loader() {
	disk=$1 esp=$2 root=$3 removable=$4
	say "step Installing the loader"
	mkdir -p "$ESP"
	run mount -t msdos "/dev/${disk}s${esp}" "$ESP"
	# the system tree carries its own loader and kernel, a live CD's ESP is not a partition we can mount
	if [ -f "$STANDALONE/kernel" ]; then
		src=$STANDALONE
	else
		boot=$(boot_disk)
		mkdir -p "$BOOTESP"
		run mount -t msdos "/dev/${boot}s1" "$BOOTESP"
		src="$BOOTESP/EFI/BOOT"
		[ -f "$BOOTESP/EFI/PureDarwin/kernel" ] && src="$BOOTESP/EFI/PureDarwin"
	fi
	uuid=$ROOTUUID
	# the installed system boots from its own disk, never the live ramdisk
	args=$(sed -e 's/ *boot-uuid=[^ ]*//' -e 's/ *root-uuid=[^ ]*//' -e 's/ *ext4-uuid=[^ ]*//' -e 's/ *rd=[^ ]*//' \
	    "$src/boot-args.txt" 2>/dev/null)

	mkdir -p "$ESP/EFI/PureDarwin"
	run cp "$src/BOOTX64.EFI" "$src/kernel" "$ESP/EFI/PureDarwin/"
	# ext4-uuid is the filesystem's own uuid, boot-uuid would be matched against the gpt partition's
	printf '%s ext4-uuid=%s' "$args" "$uuid" > "$ESP/EFI/PureDarwin/boot-args.txt"
	say "run boot-args: $(cat "$ESP/EFI/PureDarwin/boot-args.txt")"
	if [ "$removable" = 1 ]; then
		mkdir -p "$ESP/EFI/BOOT"
		run cp "$src/BOOTX64.EFI" "$src/kernel" "$ESP/EFI/BOOT/"
		cp "$ESP/EFI/PureDarwin/boot-args.txt" "$ESP/EFI/BOOT/boot-args.txt"
	fi
	umount "$BOOTESP" 2>/dev/null
	if pdbless list > /dev/null 2>&1; then
		run pdbless add "$disk" "$esp" /EFI/PureDarwin/BOOTX64.EFI PureDarwin
	elif [ "$removable" = 1 ]; then
		say "run pdbless: no firmware variables, the disk boots from EFI/BOOT"
	else
		say "run pdbless: no firmware variables, add a boot entry for EFI/PureDarwin/BOOTX64.EFI in the firmware"
	fi
	say "progress 92"
}

finish() {
	disk=$1 esp=$2 root=$3
	say "step Checking"
	mkdir -p "$TARGET/private/var/log"
	cp "$LOG" "$TARGET/private/var/log/install.log"
	sync
	cleanup
	run fsck_msdos -n "/dev/r${disk}s${esp}"
	run e2fsck -fn "/dev/${disk}s${root}"
	say "progress 100"
	say "done PureDarwin is installed on ${disk}s${root}"
}

format_root() {
	say "step Formatting"
	say "progress 8"
	run mke2fs -q -t ext4 -L PureDarwin "/dev/$1"
	# read before the mount, a mounted block device is busy for tune2fs
	ROOTUUID=$(tune2fs -l "/dev/r$1" 2>/dev/null | awk '/Filesystem UUID/ { print $3 }')
	[ -n "$ROOTUUID" ] || fail "no filesystem uuid on $1"
	mkdir -p "$TARGET"
	run mount -t ext4 "/dev/$1" "$TARGET"
}

cmd_erase() {
	disk=$1
	check_disk "$disk"
	confirm "everything on $disk is deleted" "$2"
	: > "$LOG"

	say "step Preparing the disk"
	say "progress 2"
	gpt destroy "$disk" >> "$LOG" 2>&1
	run gpt create -f "$disk"
	run gpt add -t efi -s 200m -l EFI "$disk"
	run gpt add -t linux-data -l PureDarwin "$disk"
	sleep 1
	[ -b "/dev/${disk}s2" ] || fail "the new partitions did not appear"
	run newfs_msdos -F 32 -v EFI "/dev/r${disk}s1"
	format_root "${disk}s2"
	copy_system
	install_loader "$disk" 1 2 1
	finish "$disk" 1 2
}

cmd_free() {
	disk=$1 yes=$2
	check_disk "$disk"
	gpt show "$disk" > /dev/null 2>&1 || fail "$disk has no GPT, use erase"
	gap=$(free_gaps "$disk" | head -1)
	[ -n "$gap" ] || fail "$disk has no free space"
	set -- $gap
	# the gap aligned to 1 MiB at both ends, in 512 byte blocks
	start=$(( ($1 + 2047) / 2048 * 2048 ))
	size=$(( ($2 - (start - $1)) / 2048 * 2048 ))
	[ "$size" -ge $((8 * 1024 * 1024 * 2)) ] || fail "the largest free gap on $disk is under 8 GB"
	esp=$(parts_of_type "$disk" "EFI System" | head -1)
	confirm "PureDarwin goes into $((size / 2048)) MB of free space on $disk, nothing else is touched" "$yes"
	: > "$LOG"

	say "step Preparing the disk"
	say "progress 2"
	before=$(gpt show "$disk" | awk '$3 ~ /^[0-9]+$/ { print $3 }' | sort -n | tr '\n' ' ')
	if [ -z "$esp" ]; then
		run gpt add -t efi -b "$start" -s 409600 -l EFI "$disk"
		start=$((start + 409600))
		size=$((size - 409600))
		sleep 1
		esp=$(parts_of_type "$disk" "EFI System" | head -1)
		[ -n "$esp" ] || fail "the EFI system partition did not appear"
		run newfs_msdos -F 32 -v EFI "/dev/r${disk}s${esp}"
		before="$before $esp"
	fi
	run gpt add -t linux-data -b "$start" -s "$size" -l PureDarwin "$disk"
	sleep 1
	root=$(gpt show "$disk" | awk '$3 ~ /^[0-9]+$/ { print $3 }' | sort -n | while read -r i; do
		case " $before " in *" $i "*) ;; *) echo "$i" ;; esac
	done | head -1)
	[ -n "$root" ] && [ -b "/dev/${disk}s${root}" ] || fail "the new partition did not appear"
	format_root "${disk}s${root}"
	copy_system
	install_loader "$disk" "$esp" "$root" 0
	finish "$disk" "$esp" "$root"
}

case "$1" in
disks) cmd_disks ;;
erase) shift; cmd_erase "$@" ;;
free) shift; cmd_free "$@" ;;
*) usage ;;
esac
