# PLATFORM: MACOS 
#
# test large files rdar://43285000

PLATFORM = MACOS
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
		-o $(TMPDIR)/hello ${TESTROOT}/src/hello.c

	$(PASS_IFF) $(CS_ALLOC) -i $(TMPDIR)/hello -o $(TMPDIR)/hello.cs \
		-a $(ARCH) 1024

clean:
	rm -rf $(TMPDIR)
