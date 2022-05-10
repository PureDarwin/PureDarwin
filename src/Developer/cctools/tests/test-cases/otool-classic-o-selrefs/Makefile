# PLATFORM: MACOS
#
# test the output of otool-classic-o on a simple Objective-C tool that includes
# subclasses, protocols, categories, and properties. Verify enums, counts, and
# symbol names print correctly, without getting hung up on addresses, locations,
# or __DATA vs __DATA_CONST.
#
# originally this was done with some looping logic, but Makefiles are the
# wrong technology for looping. so let's expand this out for now. while we
# consider a better test driver ...

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

# macOSJazz no longer supports i386
VALID_ARCHS := $(filter-out i386,$(VALID_ARCHS))

.PHONY : all all2 clean

# test_dyldinfo compiles and runs the test with dyldinfo bind opcodes
#   $1 is the arch
#   $2 is the check prefix
test_dyldinfo =								     \
	echo $(1);							     \
	${CC} -arch $(1) -framework Foundation -dynamiclib		     \
	  -o test_$(1).dylib Dylib.m 2>&1;				     \
	${CC} -arch $(1) -framework Foundation				     \
	  -o test_$(1).tool Tool.m test_$(1).dylib 2>&1;		     \
	${OTOOLC} -ov test_$(1).tool | ${CHECK} -p $(2);

# test_chains compiles and runs the test with chained fixups
#   $1 is the arch
#   $2 is the check prefix
test_chains =								     \
	echo chained $(1);						     \
	${CC} -arch $(1) -framework Foundation -dynamiclib		     \
	  -Wl,-fixup_chains						     \
	  -o test_chained_$(1).dylib Dylib.m 2>&1;			     \
	${CC} -arch $(1) -framework Foundation				     \
	  -o test_chained_$(1).tool Tool.m test_chained_$(1).dylib 2>&1;     \
	${OTOOLC} -ov test_chained_$(1).tool | ${CHECK} -p $(2);

all:
################################################################################
#
# Binaries compiled using LC_DYLD_INFO binds
#
#
# i386
ifneq (,$(filter i386,$(VALID_ARCHS)))
	echo i386
	${CC} -arch i386 -framework Foundation -dynamiclib		     \
	  -o test_i386.dylib Dylib.m 2>&1
	${CC} -arch i386 -framework Foundation				     \
	  -o test_i386.tool Tool.m test_i386.dylib 2>&1
	${OTOOLC} -ov test_i386.tool | ${CHECK} -p CHECKI386;
endif
#
# x86_64
ifneq (,$(filter x86_64,$(VALID_ARCHS)))
	$(call test_dyldinfo,x86_64,CHECK64)
endif
#
# x86_64h
ifneq (,$(filter x86_64h,$(VALID_ARCHS)))
	$(call test_dyldinfo,x86_64h,CHECK64)
endif
#
# armv7k
ifneq (,$(filter armv7k,$(VALID_ARCHS)))
	$(call test_dyldinfo,armv7k,CHECK32)
endif
#
# armv64_32
ifneq (,$(filter armv64_32,$(VALID_ARCHS)))
	$(call test_dyldinfo,armv64_32,CHECK32)
endif
#
# arm64
ifneq (,$(filter arm64,$(VALID_ARCHS)))
	$(call test_dyldinfo,arm64,CHECK64)
endif
#
# arm64e
ifneq (,$(filter arm64e,$(VALID_ARCHS)))
	$(call test_dyldinfo,arm64e,CHECK64)
endif
################################################################################
#
# Binaries compiled using LC_DYLD_CHAINED_FIXUPS binds
#
#
# i386
ifneq (,$(filter i386,$(VALID_ARCHS)))
	echo chained i386
	${CC} -arch i386 -framework Foundation -dynamiclib		     \
	  -Wl,-fixup_chains						     \
	  -o test_chained_i386.dylib Dylib.m 2>&1
	${CC} -arch i386 -framework Foundation				     \
	  -Wl,-fixup_chains						     \
	  -o test_chained_i386.tool Tool.m test_chained_i386.dylib 2>&1
	${OTOOLC} -ov test_chained_i386.tool | ${CHECK} -p CHECKI386;
endif
#
# x86_64
ifneq (,$(filter x86_64,$(VALID_ARCHS)))
	$(call test_chains,x86_64,CHECK64)
endif
#
# x86_64h
ifneq (,$(filter x86_64h,$(VALID_ARCHS)))
	$(call test_chains,x86_64h,CHECK64)
endif
#
# armv7k
ifneq (,$(filter armv7k,$(VALID_ARCHS)))
	$(call test_chains,armv7k,CHECK32)
endif
#
# armv64_32
ifneq (,$(filter armv64_32,$(VALID_ARCHS)))
	$(call test_chains,armv64_32,CHECK32)
endif
#
# arm64
ifneq (,$(filter arm64,$(VALID_ARCHS)))
	$(call test_chains,arm64,CHECK64)
endif
#
# arm64e
ifneq (,$(filter arm64e,$(VALID_ARCHS)))
	$(call test_chains,arm64e,CHECK64)
endif

	echo PASS

all2:
	for arch in ${VALID_ARCHS};					     \
	do								     \
	  echo testing $$arch ...;					     \
	  ${CC} -arch $$arch -framework Foundation -dynamiclib		     \
	    -o Dylib.$$arch.dylib Dylib.m 2>&1;				     \
	  ${CC} -arch $$arch -framework Foundation			     \
	    -o Tool.$$arch Tool.m Dylib.$$arch.dylib 2>&1;		     \
	  if [ "$$arch" == "i386" ]; then				     \
	    ${OTOOLC} -ov Tool.$$arch | ${CHECK} -p CHECKI386;		     \
	  elif [ "$$arch" == "armv7k" -o "$$arch" == "arm64_32" ]; then	     \
	    ${OTOOLC} -ov Tool.$$arch | ${CHECK} -p CHECK32;		     \
	  else								     \
	    ${OTOOLC} -ov Tool.$$arch | ${CHECK} -p CHECK64;		     \
	  fi;								     \
	  echo testing chained $$arch ...;				     \
	  ${CC} -arch $$arch -framework Foundation -dynamiclib		     \
	    -Wl,-fixup_chains						     \
	    -o Dylib.chained.$$arch.dylib Dylib.m 2>&1;			     \
	  ${CC} -arch $$arch -framework Foundation			     \
	    -Wl,-fixup_chains						     \
	    -o Tool.chained.$$arch Tool.m Dylib.chained.$$arch.dylib 2>&1;   \
	  if [ "$$arch" == "i386" ]; then				     \
	    ${OTOOLC} -ov Tool.chained.$$arch | ${CHECK} -p CHECKI386;	     \
	  elif [ "$$arch" == "armv7k" -o "$$arch" == "arm64_32" ]; then	     \
	    ${OTOOLC} -ov Tool.chained.$$arch | ${CHECK} -p CHECK32;	     \
	  else								     \
	    ${OTOOLC} -ov Tool.chained.$$arch | ${CHECK} -p CHECK64;	     \
	  fi								     \
	done

	echo PASS

################################################################################
#
# test_x86_64.tool:
# CHECK64: Contents of (__DATA,__objc_selrefs) section
# CHECK64:     0x\[\[:xdigit:\]\]+ verb

################################################################################
#
# test_chained_armv7k.tool:
# CHECK32: Contents of (__DATA,__objc_selrefs) section
# CHECK32:     0x\[\[:xdigit:\]\]+ verb

################################################################################
#
# CHECKI386:Objective-C segment

clean:
	rm -f test_*
