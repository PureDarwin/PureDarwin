# PLATFORM: MACOS IOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	# create a preload with classic relocations and v2 split-seg.
	#
	$(CC) preload.c -o preload -o preload -arch $(ARCH)		     \
	  -static -nostdlib -Wl,-e,_start -Wl,-pie -Wl,-add_split_seg_info   \
	  -Wl,-function_starts -Wl,-headerpad,152 -Wl,-no_zero_fill_sections \
	  -Wl,-pagezero_size,0x0

	#
	# whether you get v1 or v2 split-seg info is an implementation
	# detail of the linker.
	#
	# this is a little unfortunate. if we had a reliable way to force
	# ld to always write v1 or always write v2 we could use dyldinfo to
	# validate the preload data is correct. but we don't, and history
	# tells us that when ld changes its implementation, we won't find out
	# until some time later when cctools tests start failing. Switching
	# to binary test files is not the direction I want to go here.
	#
	# the solution is to ask the linker to produce a binary with
	# split-seg data, then manually manually hack the binary to make
	# it look (vaguely) like v1 or v2 to ctf_insert for the purposes of
	# the test. We can then compare the pre-ctf_insert linkedit to the
	# post-ctf_insert linkedit, and if they match EXACTLY we know the data
	# is correct.
	#
	cp preload preload_v1
	cp preload preload_v2
	${OTOOLC} -lv preload | grep LC_SEGMENT_SPLIT_INFO -A3 > lc.txt
	seek=`grep dataoff lc.txt | awk '{ print $$2 }'`;		     \
	printf "\000" |							     \
	  dd count=1 bs=1 of=preload_v1 seek=$$seek conv=notrunc 2>&1;	     \
	printf "\177" |							     \
	  dd count=1 bs=1 of=preload_v2 seek=$$seek conv=notrunc 2>&1;	     \

	#
	# attempt to patch the v1 file. this should fail.
	#
	echo ctf > ctf.txt
	${FAIL_IF_SUCCESS} $(CTF_INSERT) preload_v1 -arch $(ARCH) ctf.txt    \
	  -o preload_v1 2>&1
	$(OTOOLC) -lv preload_v1 | ${CHECK} -p V1
# V1-NOT: segname __CTF

	#
	# patch the v2 file. this should succeed
	#
	${FAIL_IF_ERROR} $(CTF_INSERT) preload_v2 -arch $(ARCH) ctf.txt	     \
	  -o preload_v2_ctf 2>&1
	$(OTOOLC) -lv preload_v2_ctf | ${CHECK} -p V2
# V2: sectname __ctf
# V2:  segname __CTF
# V2:     size .*4

	#
	# compare the unpatched and patched linkedit
	#
	${OTOOLC} -lv preload_v2     | grep __LINKEDIT -A4 > lc.txt
	${OTOOLC} -lv preload_v2_ctf | grep __LINKEDIT -A4 > lc2.txt
	seek1=`grep fileoff lc.txt  | awk '{ print $$2 }'`;		     \
	seek2=`grep fileoff lc2.txt | awk '{ print $$2 }'`;		     \
	echo cmp -bl -i $$seek1:$$seek2 preload_v2 preload_v2_ctf;	     \
	${PASS_IFF} cmp -bl -i $$seek1:$$seek2 preload_v2 preload_v2_ctf

clean:
	rm -f preload preload_v1 preload_v2 preload_v2_ctf ctf.txt	     \
	      lc.txt lc2.txt
