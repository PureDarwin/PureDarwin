# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a test program
	#
	# ideally we'd use the ${ARCH} and ${VALID_ARCHS} flags from
	# common.makefile, but check.pl doesn't currently allow variable
	# expansion in CHECK comments. So we'll hardcode arch values for now.
	#
	${CC} -arch x86_64 -o hello ${TESTROOT}/src/hello.c
	${CC} -arch x86_64 -arch x86_64h -o hello_fat ${TESTROOT}/src/hello.c

	##
	## test -remove-build-version
	##

	# remove all build versions
	${VTOOL} -r -set-build-version macos 1 1 -o hello2 hello
	${VTOOL} -remove-build-version macos -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RM_ALL_VERSIONS

	${VTOOL} -arch x86_64 -r -set-build-version macos 1 1 -o hello2 hello
	${VTOOL} -arch x86_64 -remove-build-version macos -o hello2 hello2
	${VTOOL} -arch x86_64 -show-build hello2 | ${CHECK} -p RM_ALL_VERSIONS

	${VTOOL} -arch x86_64 -r -set-build-version macos 1 1		     \
	    -o hello2 hello_fat
	${VTOOL} -arch x86_64 -remove-build-version macos		     \
	    -o hello2 hello2
	${VTOOL} -arch x86_64 -show-build hello2 |			     \
	    ${CHECK} -p RM_ALL_VERSIONS

	${VTOOL} -arch x86_64h -r -set-build-version macos 1 1		     \
	    -o hello2 hello_fat
	${VTOOL} -arch x86_64h -remove-build-version macos		     \
	    -o hello2 hello2
	${VTOOL} -arch x86_64h -show-build hello2 |			     \
	    ${CHECK} -p RM_ALL_VERSIONS
# RM_ALL_VERSIONS:     hello2.*:
# RM_ALL_VERSIONS-NOT: cmd LC_BUILD_VERSION

	#   all archs
	${VTOOL} -arch x86_64 -arch x86_64h -r -set-build-version macos 1 1  \
	    -o hello2 hello_fat
	${VTOOL} -arch x86_64 -arch x86_64h -remove-build-version macos	     \
	    -o hello2 hello2
	${VTOOL} -arch x86_64 -arch x86_64h -show-build hello2 |	     \
	    ${CHECK} -p RM_ALL_VERSIONS_FAT

	${VTOOL} -r -set-build-version macos 1 1  \
	    -o hello2 hello_fat
	${VTOOL} -remove-build-version macos	     \
	    -o hello2 hello2
	${VTOOL} -show-build hello2 |	     \
	    ${CHECK} -p RM_ALL_VERSIONS_FAT
# RM_ALL_VERSIONS_FAT:     hello2.*:
# RM_ALL_VERSIONS_FAT-NOT: cmd LC_BUILD_VERSION
# RM_ALL_VERSIONS_FAT:     hello2.*:
# RM_ALL_VERSIONS_FAT-NOT: cmd LC_BUILD_VERSION

	# remove the first build version
	${VTOOL} -r -set-build-version macos 1 2			     \
	           -set-build-version 6 3 4			     \
		   -set-build-version driverkit 5 6			     \
		   -o hello2 hello
	${VTOOL} -remove-build-version macos -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBV2
#RBV2:     hello2:
#RBV2:     cmd LC_BUILD_VERSION
#RBV2-NOT: platform MACOS
#RBV2-NOT: platform DRIVERKIT
#RBV2-NOT: cmd LC_BUILD_VERSION
#RBV2:     platform MACCATALYST
#RBV2:     cmd LC_BUILD_VERSION
#RBV2-NOT: platform MACOS
#RBV2-NOT: platform MACCATALYST
#RBV2-NOT: cmd LC_BUILD_VERSION
#RBV2:     platform DRIVERKIT
#RBV2-NOT: cmd LC_BUILD_VERSION

	# remove the mjiddle build version
	${VTOOL} -r -set-build-version macos 1 2			     \
	           -set-build-version 6 3 4			     \
		   -set-build-version driverkit 5 6			     \
		   -o hello2 hello
	${VTOOL} -remove-build-version 6 -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBV3
#RBV3:     hello2:
#RBV3:     cmd LC_BUILD_VERSION
#RBV3-NOT: platform MACCATALYST
#RBV3-NOT: platform DRIVERKIT
#RBV3-NOT: cmd LC_BUILD_VERSION
#RBV3:     platform MACOS
#RBV3:     cmd LC_BUILD_VERSION
#RBV3-NOT: platform MACOS
#RBV3-NOT: platform MACCATALYST
#RBV3-NOT: cmd LC_BUILD_VERSION
#RBV3:     platform DRIVERKIT
#RBV3-NOT: cmd LC_BUILD_VERSION

	# remove the last build version
	${VTOOL} -r -set-build-version macos 1 2			     \
	           -set-build-version 6 3 4			     \
		   -set-build-version driverkit 5 6			     \
		   -o hello2 hello
	${VTOOL} -remove-build-version driverkit -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBV4
#RBV4:     hello2:
#RBV4:     cmd LC_BUILD_VERSION
#RBV4-NOT: platform MACCATALYST
#RBV4-NOT: platform DRIVERKIT
#RBV4-NOT: cmd LC_BUILD_VERSION
#RBV4:     platform MACOS
#RBV4:     cmd LC_BUILD_VERSION
#RBV4-NOT: platform MACOS
#RBV4-NOT: platform DRIVERKIT
#RBV4-NOT: cmd LC_BUILD_VERSION
#RBV4:     platform MACCATALYST
#RBV4-NOT: cmd LC_BUILD_VERSION

	##
	## test -remove-build-tool
	##

	${VTOOL} -r -set-build-version macos  1 2 -tool 1 1 -tool 2 2	     \
	            -set-build-version 6 3 4 -tool 1 3 -tool 2 4	     \
		    -o hello2 hello
	${VTOOL} -remove-build-tool macos clang -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBT1
# RBT1:      Load command \d+
# RBT1-NEXT:      cmd LC_BUILD_VERSION
# RBT1-NEXT:  cmdsize 40
# RBT1-NEXT: platform MACCATALYST
# RBT1-NEXT:    minos 3.0
# RBT1-NEXT:      sdk 4.0
# RBT1-NEXT:   ntools 2
# RBT1-NEXT:     tool CLANG
# RBT1-NEXT:  version 3.0
# RBT1-NEXT:     tool SWIFT
# RBT1-NEXT:  version 4.0
# RBT1:     Load command \d+
# RBT1-NEXT:      cmd LC_BUILD_VERSION
# RBT1-NEXT:  cmdsize 32
# RBT1-NEXT: platform MACOS
# RBT1-NEXT:    minos 1.0
# RBT1-NEXT:      sdk 2.0
# RBT1-NEXT:   ntools 1
# RBT1-NEXT:     tool SWIFT
# RBT1-NEXT:  version 2.0

	${VTOOL} -r -set-build-version macos  1 2 -tool 1 1 -tool 2 2	     \
	            -set-build-version 6 3 4 -tool 1 3 -tool 2 4	     \
		    -o hello2 hello
	${VTOOL} -remove-build-tool 6 clang -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBT2
# RBT2:      Load command \d+
# RBT2-NEXT:      cmd LC_BUILD_VERSION
# RBT2-NEXT:  cmdsize 40
# RBT2-NEXT: platform MACOS
# RBT2-NEXT:    minos 1.0
# RBT2-NEXT:      sdk 2.0
# RBT2-NEXT:   ntools 2
# RBT2-NEXT:     tool CLANG
# RBT2-NEXT:  version 1.0
# RBT2-NEXT:     tool SWIFT
# RBT2-NEXT:  version 2.0
# RBT2:     Load command \d+
# RBT2-NEXT:      cmd LC_BUILD_VERSION
# RBT2-NEXT:  cmdsize 32
# RBT2-NEXT: platform MACCATALYST
# RBT2-NEXT:    minos 3.0
# RBT2-NEXT:      sdk 4.0
# RBT2-NEXT:   ntools 1
# RBT2-NEXT:     tool SWIFT
# RBT2-NEXT:  version 4.0

	${VTOOL} -r -set-build-version macos  1 2 -tool 1 1 -tool 2 2	     \
	            -set-build-version 6 3 4 -tool 1 3 -tool 2 4	     \
		    -o hello2 hello
	${VTOOL} -remove-build-tool macos swift -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBT3
# RBT3:      Load command \d+
# RBT3-NEXT:      cmd LC_BUILD_VERSION
# RBT3-NEXT:  cmdsize 40
# RBT3-NEXT: platform MACCATALYST
# RBT3-NEXT:    minos 3.0
# RBT3-NEXT:      sdk 4.0
# RBT3-NEXT:   ntools 2
# RBT3-NEXT:     tool CLANG
# RBT3-NEXT:  version 3.0
# RBT3-NEXT:     tool SWIFT
# RBT3-NEXT:  version 4.0
# RBT3:     Load command \d+
# RBT3-NEXT:      cmd LC_BUILD_VERSION
# RBT3-NEXT:  cmdsize 32
# RBT3-NEXT: platform MACOS
# RBT3-NEXT:    minos 1.0
# RBT3-NEXT:      sdk 2.0
# RBT3-NEXT:   ntools 1
# RBT3-NEXT:     tool CLANG
# RBT3-NEXT:  version 1.0

	${VTOOL} -r -set-build-version macos 1 2  -tool 1 1 -tool 2 2	     \
	            -set-build-version 6 3 4 -tool 1 3 -tool 2 4	     \
		    -o hello2 hello
	${VTOOL} -remove-build-tool 6 swift -o hello2 hello2
	${VTOOL} -show-build hello2 | ${CHECK} -p RBT4
# RBT4:      Load command \d+
# RBT4-NEXT:      cmd LC_BUILD_VERSION
# RBT4-NEXT:  cmdsize 40
# RBT4-NEXT: platform MACOS
# RBT4-NEXT:    minos 1.0
# RBT4-NEXT:      sdk 2.0
# RBT4-NEXT:   ntools 2
# RBT4-NEXT:     tool CLANG
# RBT4-NEXT:  version 1.0
# RBT4-NEXT:     tool SWIFT
# RBT4-NEXT:  version 2.0
# RBT4:     Load command \d+
# RBT4-NEXT:      cmd LC_BUILD_VERSION
# RBT4-NEXT:  cmdsize 32
# RBT4-NEXT: platform MACCATALYST
# RBT4-NEXT:    minos 3.0
# RBT4-NEXT:      sdk 4.0
# RBT4-NEXT:   ntools 1
# RBT4-NEXT:     tool CLANG
# RBT4-NEXT:  version 3.0

	##
	## test -remove-source-version
	##

	${VTOOL} -r -set-build-version macos 1 2 -tool 1 1 -tool 2 2	     \
	            -set-source-version 1.2.3.4.5			     \
		    -o hello2 hello
	${VTOOL} -remove-source-version -o hello2 hello2
	${VTOOL} -show hello2 | ${CHECK} -p RSV
#RSV-NOT: cmd LC_SOURCE_VERSION
#RSV:     cmd LC_BUILD_VERSION
#RSV-NOT: cmd LC_SOURCE_VERSION

	echo PASS

clean:
	rm -rf hello hello_fat hello2
