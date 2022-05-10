# PLATFORM: MACOS IOS WATCHOS TVOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

THIN_ARCH_OPTION= $(patsubst %,-arch %,$($ARCH))
FAT_ARCH_OPTIONS= $(patsubst %,-arch %,$(VALID_ARCHS))
FAT_ARCH_COUNT  = $(words $(VALID_ARCHS))

all:
	if [ $(FAT_ARCH_COUNT) -gt 1 ];					     \
	then								     \
	  $(CC) -arch $(ARCH) -o hello ${TESTROOT}/src/hello.c;		     \
	  ${LIPO} -info hello |						     \
	    grep "Non-fat file: hello is architecture: $(ARCH)" |	     \
	    ${FAIL_IF_EMPTY};						     \
	  $(CC) $(FAT_ARCH_OPTIONS) -o hello ${TESTROOT}/src/hello.c;	     \
	  ${LIPO} -info hello |						     \
	    grep "Architectures in the fat file: hello are:" |		     \
	    grep "${FAT_ARCHS}" | ${FAIL_IF_EMPTY};			     \
	fi
	${PASS_IFF_SUCCESS} true

clean:
	rm -rf hello
