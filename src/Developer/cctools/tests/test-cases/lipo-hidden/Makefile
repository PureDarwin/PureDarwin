# PLATFORM: WATCHOS

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# fail if hiding files that aren't MH_EXECUTE
	for arch in armv7k arm64e arm64 arm64_32;			     \
	do								     \
	  ofile="empty.$$arch";						     \
	  $(CC) -arch $$arch -o $$ofile -dynamiclib empty.c > /dev/null 2>&1;\
	  files="$$files $$ofile";					     \
	done;								     \
	$(FAIL_IF_SUCCESS) ${LIPO} -create -output empty.dylib		     \
	  -hideARM64 $$files 2>&1;					     \
	rm -f $$files
	
	# verify arm64 can be hidden
# 	files="";							     
	for arch in armv7k arm64e;					     \
	do								     \
	  ofile="empty.$$arch";						     \
	  $(CC) -arch $$arch -o $$ofile empty.c > /dev/null 2>&1;	     \
	  files="$$files $$ofile";					     \
	done;								     \
	$(FAIL_IF_ERROR) ${LIPO} -create -output empty			     \
	  -hideARM64 $$files;
	${LIPO} -info empty | grep -e 'armv7k arm64e'			     \
		| ${FAIL_IF_EMPTY}
	${LIPO} -detailed_info empty | grep -e 'arm64e (hidden)'	     \
		| ${FAIL_IF_EMPTY}
		
	# verify arm64 isn't always hidden
	for arch in armv7k arm64e;					     \
	do								     \
	  ofile="empty.$$arch";						     \
	  files="$$files $$ofile";					     \
	done;								     \
	$(FAIL_IF_ERROR) ${LIPO} -create -output empty $$files
	${LIPO} -info empty | grep -e 'armv7k arm64e'			     \
		| ${FAIL_IF_EMPTY}
	${LIPO} -detailed_info empty | grep -e 'arm64e (hidden)'	     \
		| ${FAIL_IF_STDIN}

	# verify 
	${PASS_IFF_SUCCESS} true

clean:
	rm -rf empty.armv7k empty.arm64 empty.arm64e empty.arm64_32 \
	  empty.dylib empty

