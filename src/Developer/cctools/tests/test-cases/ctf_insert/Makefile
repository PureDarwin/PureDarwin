# PLATFORM: MACOS IOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

NOTE = ${TESTROOT}/bin/note-tool.pl

.PHONY: all clean

all:
	# create a preload
	#
	$(CC) $(TESTROOT)/src/foo.c -DFOO=start -o preload -arch $(ARCH)     \
	  -static -nostdlib -Wl,-e,_start -Wl,-pie -Wl,-pagezero_size,0x0    \
	  -Wl,-function_starts -Wl,-headerpad,152 -Wl,-no_zero_fill_sections

	#
	# patch it
	#
	echo ctf > ctf.txt
	$(CTF_INSERT) preload -arch $(ARCH) ctf.txt -o preload_ctf

	#
	# verify __CTF exists in the right order and not vm mapped
	#
	$(SIZEC) -lm preload_ctf | $(CHECK) -p SIZE
# SIZE: Segment __TEXT:.*
# SIZE: Segment __CTF: 0 .*
# SIZE: Segment __LINKEDIT: .*

	#
	# verify NORELOC
	#
	$(OTOOLC) -lv preload_ctf | grep "segname __CTF" -A8 |		     \
	  $(CHECK) -p OTOOL
# OTOOL: flags NORELOC

  	#
	# verify the patched segment is zero-filled and target page aligned
	#
	${OTOOLC} -lv preload_ctf | grep __CTF -A4 > lc.txt
	size=`grep filesize lc.txt | awk '{ print $$2 }'`;		     \
	seek=`grep fileoff lc.txt | awk '{ print $$2 }'`;		     \
	dd count=$$size bs=1 if=/dev/zero of=ctf.pad 2>&1;		     \
	dd bs=1 if=ctf.txt of=ctf.pad conv=notrunc 2>&1;		     \
	dd iseek=$$seek count=$$size bs=1 if=preload_ctf of=ctf.out 2>&1
	${FAIL_IF_ERROR} cmp ctf.out ctf.pad

	#
	# Add an LC_NOTE to the preload file, and confirm ctf_insert will fail.
	#
	${NOTE} -add-note MDT "Just a regular note..." -o preload_note preload
	${FAIL_IFF} $(CTF_INSERT) preload_note -arch $(ARCH) ctf.txt	     \
	  -o preload_note 2>&1

clean:
	rm -f preload preload_ctf preload_note ctf.txt ctf.out ctf.pad lc.txt
