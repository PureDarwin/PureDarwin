# PLATFORM: MACOS
#
# test large files rdar://43285000  

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

TMPDIR = /tmp/$(TEST).tmp

# we need to make sure the final binary is larger than 2GB. But due to
# another issue we need to make sure the final binary is less than 4 GB.
# so write out two files (x86_64, x86_64h) at 1.5 GB each.
VALID_ARCHS := $(filter-out i386, $(VALID_ARCHS))

all:
	# clean some local storage
	rm -rf $(TMPDIR)
	$(MKDIRS) $(TMPDIR)

	# make a big file of zeroes
	dd if=/dev/zero of=$(TMPDIR)/zeroes bs=1024000 count=1500	     \
		2>&1 1>/dev/null

	# link our file
	for arch in $(VALID_ARCHS);					     \
	do								     \
	  ofile="$(TMPDIR)/hello.$$arch";				     \
	  files="$$ofile $$files";					     \
	  cc -arch $$arch -o $$ofile ${TESTROOT}/src/hello.c		     \
	    -Wl,-sectcreate,__ZERO,__zero1,$(TMPDIR)/zeroes;		     \
	done;								     \
									     \
	$(PASS_IFF) $(LIPO) -create -output $(TMPDIR)/hello $$files

clean:
	rm -rf $(TMPDIR)
