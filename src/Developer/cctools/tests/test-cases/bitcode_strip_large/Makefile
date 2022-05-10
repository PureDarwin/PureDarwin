# PLATFORM: WATCHOS
#
# test large files rdar://43285000  
PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

TMPDIR = /tmp/$(TEST).tmp

all:
	# clean some local storage
	rm -rf $(TMPDIR)
	$(MKDIRS) $(TMPDIR)

	# make a big file of zeroes
	dd if=/dev/zero of=$(TMPDIR)/zeroes bs=1024000 count=1000 \
		2>&1 1>/dev/null

	# link our file
	$(CC) -arch $(ARCH) \
		-Wl,-sectcreate,__ZERO,__zero1,$(TMPDIR)/zeroes \
		-Wl,-sectcreate,__ZERO,__zero2,$(TMPDIR)/zeroes \
		-Wl,-sectcreate,__ZERO,__zero3,$(TMPDIR)/zeroes \
		-fembed-bitcode-marker 				\
		-o $(TMPDIR)/hello ${TESTROOT}/src/hello.c

	# strip
	$(PASS_IFF) $(BITCODE_STRIP) -m -o $(TMPDIR)/hello.bs \
		$(TMPDIR)/hello

clean:
	rm -rf $(TMPDIR)
