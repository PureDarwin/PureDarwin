# PLATFORM: MACOS
#
# verify codesign_allocate was built with the following requirement:
#
#   info["Application-Group"]="com.apple.tool.codesign_allocate
# 
# locally built cctools is not Apple-signed, so we do not check that:
#
#   anchor apple
#
# For more information, see the Security project's signerutils.cpp's
# appleReq[] symbol

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${PASS_IFF} codesign -v						     \
	  -R='info["Application-Group"]="com.apple.tool.codesign_allocate"'  \
	  ${CS_ALLOC}
	  
clean:
