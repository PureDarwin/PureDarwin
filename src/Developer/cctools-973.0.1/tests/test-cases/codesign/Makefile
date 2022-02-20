# PLATFORM: MACOS
#
# run codesign on a new binary with the specified codesign_allocate and verify
# both the codesign_allocate tool is acceptable to codesign as well as the
# codesign_allocate output.
#
# codesign will only accept the codesign_allocae helper tool if it has been
# signed by Apple. If codesign_allocate is not Apple-signed (because it was 
# built locally, perhaps) this test will XFAIL. 

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	@echo CS_ALLOC=${CS_ALLOC}
	@echo
	
	# compile a test file
	$(CC) -arch ${ARCH} -o hello ${TESTROOT}/src/hello.c

	# verify cs_allocate is signed by apple
	-codesign -v -R="anchor apple" ${CS_ALLOC};			     \
	status=$$?;							     \
	if [ "$$status" != "0" ]; then					     \
	  echo "codesign is not Apple-signed";				     \
	  echo "XFAIL";							     \
	else								     \
	  CODESIGN_ALLOCATE=${CS_ALLOC} ${PASS_IFF} codesign -s - hello;     \
	fi

clean:
	rm -rf hello hello.cstemp
