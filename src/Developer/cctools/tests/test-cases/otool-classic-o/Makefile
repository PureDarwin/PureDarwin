# PLATFORM: MACOS IOS WATCHOS TVOS
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
# CHECK64: Contents of (__DATA\(_CONST\)?,__objc_classlist) section
# CHECK64-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_DylibSubclass
# CHECK64-NEXT:     isa        0x\[\[:xdigit:\]\]+ _OBJC_METACLASS_$_DylibSubclass
# CHECK64-NEXT:     superclass 0x0 _OBJC_CLASS_$_Dylib
# CHECK64-NEXT:     cache      0x0 __objc_empty_cache
# CHECK64-NEXT:     vtable     0x0
# CHECK64-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_CLASS_RO_$_DylibSubclass
# CHECK64-NEXT:         flags          0x0
# CHECK64-NEXT:         instanceStart  12
# CHECK64-NEXT:         instanceSize   12
# CHECK64-NEXT:         reserved       0x0
# CHECK64-NEXT:         ivarLayout     0x0
# CHECK64-NEXT:         name           0x\[\[:xdigit:\]\]+ DylibSubclass
# CHECK64-NEXT:         baseMethods    0x0
# CHECK64-NEXT:         baseProtocols  0x0
# CHECK64-NEXT:         ivars          0x0
# CHECK64-NEXT:         weakIvarLayout 0x0
# CHECK64-NEXT:         baseProperties 0x0
# CHECK64-NEXT: Meta Class
# CHECK64-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# CHECK64-NEXT:     superclass 0x0 _OBJC_METACLASS_$_Dylib
# CHECK64-NEXT:     cache      0x0 __objc_empty_cache
# CHECK64-NEXT:     vtable     0x0
# CHECK64-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_METACLASS_RO_$_DylibSubclass
# CHECK64-NEXT:         flags          0x1 RO_META
# CHECK64-NEXT:         instanceStart  40
# CHECK64-NEXT:         instanceSize   40
# CHECK64-NEXT:         reserved       0x0
# CHECK64-NEXT:         ivarLayout     0x0
# CHECK64-NEXT:         name           0x\[\[:xdigit:\]\]+ DylibSubclass
# CHECK64-NEXT:         baseMethods    0x0
# CHECK64-NEXT:         baseProtocols  0x0
# CHECK64-NEXT:         ivars          0x0
# CHECK64-NEXT:         weakIvarLayout 0x0
# CHECK64-NEXT:         baseProperties 0x0
# CHECK64-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_Tool
# CHECK64-NEXT:     isa        0x\[\[:xdigit:\]\]+ _OBJC_METACLASS_$_Tool
# CHECK64-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# CHECK64-NEXT:     cache      0x0 __objc_empty_cache
# CHECK64-NEXT:     vtable     0x0
# CHECK64-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_CLASS_RO_$_Tool
# CHECK64-NEXT:         flags          0x0
# CHECK64-NEXT:         instanceStart  8
# CHECK64-NEXT:         instanceSize   12
# CHECK64-NEXT:         reserved       0x0
# CHECK64-NEXT:         ivarLayout     0x0
# CHECK64-NEXT:         name           0x\[\[:xdigit:\]\]+ Tool
# CHECK64-NEXT:         baseMethods    0x\[\[:xdigit:\]\]+ __OBJC_$_INSTANCE_METHODS_Tool
# CHECK64-SKIP:             entsize 24
# CHECK64-SKIP:             count   2
# CHECK64-SKIP:             name    0x\[\[:xdigit:\]\]+ verb
# CHECK64-SKIP:             types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:             imp     0x\[\[:xdigit:\]\]+ -[Tool verb]
# CHECK64-SKIP:             name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-SKIP:             types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:             imp     0x\[\[:xdigit:\]\]+ -[Tool intProperty]
# CHECK64:         baseProtocols  0x\[\[:xdigit:\]\]+ __OBJC_CLASS_PROTOCOLS_$_Tool
# CHECK64-NEXT:             count    1
# CHECK64-NEXT:             list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK64-NEXT:                 isa       0x0
# CHECK64-NEXT:                 name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK64-NEXT:                 protocols 0x0
# CHECK64-NEXT:                 instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   2
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ verb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK64-NEXT:                     entsize    16
# CHECK64-NEXT:                     count      1
# CHECK64-NEXT:                     name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:                     attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64-NEXT:         ivars          0x\[\[:xdigit:\]\]+ __OBJC_$_INSTANCE_VARIABLES_Tool
# CHECK64-NEXT:             entsize   32
# CHECK64-NEXT:             count     1
# CHECK64-NEXT:             offset    0x\[\[:xdigit:\]\]+ _OBJC_IVAR_$_Tool.m_intProperty 8
# CHECK64-NEXT:             name      0x\[\[:xdigit:\]\]+ m_intProperty
# CHECK64-NEXT:             type      0x\[\[:xdigit:\]\]+ i
# CHECK64-NEXT:             alignment 2
# CHECK64-NEXT:             size      4
# CHECK64-NEXT:         weakIvarLayout 0x0
# CHECK64-NEXT:         baseProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Tool
# CHECK64-NEXT:             entsize    16
# CHECK64-NEXT:             count      1
# CHECK64-NEXT:             name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:             attributes 0x\[\[:xdigit:\]\]+ Ti,R,N,Vm_intProperty
# CHECK64-NEXT: Meta Class
# CHECK64-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# CHECK64-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# CHECK64-NEXT:     cache      0x0 __objc_empty_cache
# CHECK64-NEXT:     vtable     0x0
# CHECK64-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_METACLASS_RO_$_Tool
# CHECK64-NEXT:         flags          0x1 RO_META
# CHECK64-NEXT:         instanceStart  40
# CHECK64-NEXT:         instanceSize   40
# CHECK64-NEXT:         reserved       0x0
# CHECK64-NEXT:         ivarLayout     0x0
# CHECK64-NEXT:         name           0x\[\[:xdigit:\]\]+ Tool
# CHECK64-NEXT:         baseMethods    0x\[\[:xdigit:\]\]+ __OBJC_$_CLASS_METHODS_Tool
# CHECK64-SKIP:             entsize 24
# CHECK64-SKIP:             count   1
# CHECK64-SKIP:             name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK64-SKIP:             types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:             imp     0x\[\[:xdigit:\]\]+ \+[Tool classVerb]
# CHECK64:         baseProtocols  0x\[\[:xdigit:\]\]+ __OBJC_CLASS_PROTOCOLS_$_Tool
# CHECK64-NEXT:             count    1
# CHECK64-NEXT:             list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK64-NEXT:                 isa       0x0
# CHECK64-NEXT:                 name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK64-NEXT:                 protocols 0x0
# CHECK64-NEXT:                 instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   2
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ verb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK64-SKIP:                     entsize 24
# CHECK64-SKIP:                     count   1
# CHECK64-SKIP:                     name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK64-SKIP:                     types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                     imp     0x0
# CHECK64:                 instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK64-NEXT:                     entsize    16
# CHECK64-NEXT:                     count      1
# CHECK64-NEXT:                     name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:                     attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64-NEXT:         ivars          0x0
# CHECK64-NEXT:         weakIvarLayout 0x0
# CHECK64-NEXT:         baseProperties 0x0
# CHECK64-NEXT: Contents of (__DATA\(_CONST\)?,__objc_classrefs) section
# CHECK64-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_DylibSubclass
# CHECK64-NEXT: Contents of (__DATA\(_CONST\)?,__objc_catlist) section
# CHECK64-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ __OBJC_$_CATEGORY_Dylib_$_Category
# CHECK64-NEXT:     name      0x\[\[:xdigit:\]\]+ Category
# CHECK64-NEXT:     cls       0x0 _OBJC_CLASS_$_Dylib
# CHECK64-NEXT:     instanceMethods 0x\[\[:xdigit:\]\]+ .*
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   3
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryAddition
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x\[\[:xdigit:\]\]+ -[Dylib(Category) categoryAddition]
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ genericMethod
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x\[\[:xdigit:\]\]+ -[Dylib(Category) genericMethod]
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryProperty
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:         imp     0x\[\[:xdigit:\]\]+ -[Dylib(Category) categoryProperty]
# CHECK64:     classMethods 0x\[\[:xdigit:\]\]+ .*
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   1
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryClass
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x\[\[:xdigit:\]\]+ \+[Dylib(Category) categoryClass]
# CHECK64:     protocols 0x\[\[:xdigit:\]\]+ .*
# CHECK64-NEXT:         count    1
# CHECK64-NEXT:         list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK64-NEXT:             isa       0x0
# CHECK64-NEXT:             name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK64-NEXT:             protocols 0x0
# CHECK64-NEXT:             instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK64-SKIP:                 entsize 24
# CHECK64-SKIP:                 count   2
# CHECK64-SKIP:                 name    0x\[\[:xdigit:\]\]+ verb
# CHECK64-SKIP:                 types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                 imp     0x0
# CHECK64-SKIP:                 name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-SKIP:                 types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:                 imp     0x0
# CHECK64:             classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK64-SKIP:                 entsize 24
# CHECK64-SKIP:                 count   1
# CHECK64-SKIP:                 name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK64-SKIP:                 types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                 imp     0x0
# CHECK64:             optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK64-SKIP:                 entsize 24
# CHECK64-SKIP:                 count   1
# CHECK64-SKIP:                 name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK64-SKIP:                 types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                 imp     0x0
# CHECK64:             optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK64-SKIP:                 entsize 24
# CHECK64-SKIP:                 count   1
# CHECK64-SKIP:                 name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK64-SKIP:                 types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:                 imp     0x0
# CHECK64:             instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK64-NEXT:                 entsize    16
# CHECK64-NEXT:                 count      1
# CHECK64-NEXT:                 name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:                 attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64-NEXT:     instanceProperties 0x\[\[:xdigit:\]\]+.*
# CHECK64-NEXT:         entsize    16
# CHECK64-NEXT:         count      2
# CHECK64-NEXT:         name       0x\[\[:xdigit:\]\]+ categoryProperty
# CHECK64-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64-NEXT:         name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64-NEXT: Contents of (__DATA\(_CONST\)?,__objc_protolist) section
# CHECK64-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK64-NEXT:     isa       0x0
# CHECK64-NEXT:     name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK64-NEXT:     protocols 0x0
# CHECK64-NEXT:     instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   2
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ verb
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x0
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ i16@0:8
# CHECK64-SKIP:         imp     0x0
# CHECK64:     classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   1
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x0
# CHECK64:     optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   1
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x0
# CHECK64:     optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK64-SKIP:         entsize 24
# CHECK64-SKIP:         count   1
# CHECK64-SKIP:         name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK64-SKIP:         types   0x\[\[:xdigit:\]\]+ v16@0:8
# CHECK64-SKIP:         imp     0x0
# CHECK64:     instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK64-NEXT:         entsize    16
# CHECK64-NEXT:         count      1
# CHECK64-NEXT:         name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK64-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK64: Contents of (__DATA\(_CONST\)?,__objc_imageinfo) section
# CHECK64-NEXT:     version 0
# CHECK64-NEXT:     flags   0x40

################################################################################
#
# test_chained_armv7k.tool:
# CHECK32: Contents of (__DATA_CONST,__objc_classlist) section
# CHECK32-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_DylibSubclass
# CHECK32-NEXT:     isa        0x\[\[:xdigit:\]\]+ _OBJC_METACLASS_$_DylibSubclass
# CHECK32-NEXT:     superclass 0x0 _OBJC_CLASS_$_Dylib
# CHECK32-NEXT:     cache      0x0 __objc_empty_cache
# CHECK32-NEXT:     vtable     0x0
# CHECK32-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_CLASS_RO_$_DylibSubclass
# CHECK32-NEXT:         flags          0x0
# CHECK32-NEXT:         instanceStart  8
# CHECK32-NEXT:         instanceSize   8
# CHECK32-NEXT:         ivarLayout     0x0
# CHECK32-NEXT:         name           0x\[\[:xdigit:\]\]+ DylibSubclass
# CHECK32-NEXT:         baseMethods    0x0
# CHECK32-NEXT:         baseProtocols  0x0
# CHECK32-NEXT:         ivars          0x0
# CHECK32-NEXT:         weakIvarLayout 0x0
# CHECK32-NEXT:         baseProperties 0x0
# CHECK32-NEXT: Meta Class
# CHECK32-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# CHECK32-NEXT:     superclass 0x0 _OBJC_METACLASS_$_Dylib
# CHECK32-NEXT:     cache      0x0 __objc_empty_cache
# CHECK32-NEXT:     vtable     0x0
# CHECK32-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_METACLASS_RO_$_DylibSubclass
# CHECK32-NEXT:         flags          0x1 RO_META
# CHECK32-NEXT:         instanceStart  20
# CHECK32-NEXT:         instanceSize   20
# CHECK32-NEXT:         ivarLayout     0x0
# CHECK32-NEXT:         name           0x\[\[:xdigit:\]\]+ DylibSubclass
# CHECK32-NEXT:         baseMethods    0x0
# CHECK32-NEXT:         baseProtocols  0x0
# CHECK32-NEXT:         ivars          0x0
# CHECK32-NEXT:         weakIvarLayout 0x0
# CHECK32-NEXT:         baseProperties 0x0
# CHECK32-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_Tool
# CHECK32-NEXT:     isa        0x\[\[:xdigit:\]\]+ _OBJC_METACLASS_$_Tool
# CHECK32-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# CHECK32-NEXT:     cache      0x0 __objc_empty_cache
# CHECK32-NEXT:     vtable     0x0
# CHECK32-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_CLASS_RO_$_Tool
# CHECK32-NEXT:         flags          0x0
# CHECK32-NEXT:         instanceStart  4
# CHECK32-NEXT:         instanceSize   8
# CHECK32-NEXT:         ivarLayout     0x0
# CHECK32-NEXT:         name           0x\[\[:xdigit:\]\]+ Tool
# CHECK32-NEXT:         baseMethods    0x\[\[:xdigit:\]\]+ __OBJC_$_INSTANCE_METHODS_Tool
# CHECK32-SKIP:             entsize 12
# CHECK32-SKIP:             count   2
# CHECK32-SKIP:             name    0x\[\[:xdigit:\]\]+ verb
# CHECK32-SKIP:             types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:             imp     0x\[\[:xdigit:\]\]+
# CHECK32-SKIP:             name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-SKIP:             types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:             imp     0x\[\[:xdigit:\]\]+
# CHECK32:         baseProtocols  0x\[\[:xdigit:\]\]+ __OBJC_CLASS_PROTOCOLS_$_Tool
# CHECK32-NEXT:             count    1
# CHECK32-NEXT:             list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK32-NEXT:                 isa       0x0
# CHECK32-NEXT:                 name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK32-NEXT:                 protocols 0x0
# CHECK32-NEXT:                 instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   2
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ verb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK32-NEXT:                     entsize    8
# CHECK32-NEXT:                     count      1
# CHECK32-NEXT:                     name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:                     attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32-NEXT:         ivars          0x\[\[:xdigit:\]\]+ __OBJC_$_INSTANCE_VARIABLES_Tool
# CHECK32-NEXT:             entsize   20
# CHECK32-NEXT:             count     1
# CHECK32-NEXT:             offset    0x\[\[:xdigit:\]\]+ _OBJC_IVAR_$_Tool.m_intProperty 4
# CHECK32-NEXT:             name      0x\[\[:xdigit:\]\]+ m_intProperty
# CHECK32-NEXT:             type      0x\[\[:xdigit:\]\]+ i
# CHECK32-NEXT:             alignment 2
# CHECK32-NEXT:             size      4
# CHECK32-NEXT:         weakIvarLayout 0x0
# CHECK32-NEXT:         baseProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Tool
# CHECK32-NEXT:             entsize    8
# CHECK32-NEXT:             count      1
# CHECK32-NEXT:             name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:             attributes 0x\[\[:xdigit:\]\]+ Ti,R,N,Vm_intProperty
# CHECK32-NEXT: Meta Class
# CHECK32-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# CHECK32-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# CHECK32-NEXT:     cache      0x0 __objc_empty_cache
# CHECK32-NEXT:     vtable     0x0
# CHECK32-NEXT:     data       0x\[\[:xdigit:\]\]+ __OBJC_METACLASS_RO_$_Tool
# CHECK32-NEXT:         flags          0x1 RO_META
# CHECK32-NEXT:         instanceStart  20
# CHECK32-NEXT:         instanceSize   20
# CHECK32-NEXT:         ivarLayout     0x0
# CHECK32-NEXT:         name           0x\[\[:xdigit:\]\]+ Tool
# CHECK32-NEXT:         baseMethods    0x\[\[:xdigit:\]\]+ __OBJC_$_CLASS_METHODS_Tool
# CHECK32-SKIP:             entsize 12
# CHECK32-SKIP:             count   1
# CHECK32-SKIP:             name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK32-SKIP:             types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:             imp     0x\[\[:xdigit:\]\]+
# CHECK32:         baseProtocols  0x\[\[:xdigit:\]\]+ __OBJC_CLASS_PROTOCOLS_$_Tool
# CHECK32-NEXT:             count    1
# CHECK32-NEXT:             list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK32-NEXT:                 isa       0x0
# CHECK32-NEXT:                 name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK32-NEXT:                 protocols 0x0
# CHECK32-NEXT:                 instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   2
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ verb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK32-SKIP:                     entsize 12
# CHECK32-SKIP:                     count   1
# CHECK32-SKIP:                     name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK32-SKIP:                     types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                     imp     0x0
# CHECK32:                 instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK32-NEXT:                     entsize    8
# CHECK32-NEXT:                     count      1
# CHECK32-NEXT:                     name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:                     attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32-NEXT:         ivars          0x0
# CHECK32-NEXT:         weakIvarLayout 0x0
# CHECK32-NEXT:         baseProperties 0x0
# CHECK32-NEXT: Contents of (__DATA,__objc_classrefs) section
# CHECK32-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ _OBJC_CLASS_$_DylibSubclass
# CHECK32-NEXT: Contents of (__DATA_CONST,__objc_catlist) section
# CHECK32-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ __OBJC_$_CATEGORY_Dylib_$_Category
# CHECK32-NEXT:     name      0x\[\[:xdigit:\]\]+ Category
# CHECK32-NEXT:     cls       0x0 _OBJC_CLASS_$_Dylib
# CHECK32-NEXT:     instanceMethods 0x\[\[:xdigit:\]\]+ .*
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   3
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryAddition
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x\[\[:xdigit:\]\]+
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ genericMethod
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x\[\[:xdigit:\]\]+
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryProperty
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:         imp     0x\[\[:xdigit:\]\]+
# CHECK32:     classMethods 0x\[\[:xdigit:\]\]+ .*
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   1
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ categoryClass
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x\[\[:xdigit:\]\]+
# CHECK32:     protocols 0x\[\[:xdigit:\]\]+ .*
# CHECK32-NEXT:         count    1
# CHECK32-NEXT:         list[0]  0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK32-NEXT:             isa       0x0
# CHECK32-NEXT:             name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK32-NEXT:             protocols 0x0
# CHECK32-NEXT:             instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK32-SKIP:                 entsize 12
# CHECK32-SKIP:                 count   2
# CHECK32-SKIP:                 name    0x\[\[:xdigit:\]\]+ verb
# CHECK32-SKIP:                 types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                 imp     0x0
# CHECK32-SKIP:                 name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-SKIP:                 types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:                 imp     0x0
# CHECK32:             classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK32-SKIP:                 entsize 12
# CHECK32-SKIP:                 count   1
# CHECK32-SKIP:                 name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK32-SKIP:                 types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                 imp     0x0
# CHECK32:             optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK32-SKIP:                 entsize 12
# CHECK32-SKIP:                 count   1
# CHECK32-SKIP:                 name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK32-SKIP:                 types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                 imp     0x0
# CHECK32:             optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK32-SKIP:                 entsize 12
# CHECK32-SKIP:                 count   1
# CHECK32-SKIP:                 name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK32-SKIP:                 types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:                 imp     0x0
# CHECK32:             instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK32-NEXT:                 entsize    8
# CHECK32-NEXT:                 count      1
# CHECK32-NEXT:                 name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:                 attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32-NEXT:     instanceProperties 0x\[\[:xdigit:\]\]+.*
# CHECK32-NEXT:         entsize    8
# CHECK32-NEXT:         count      2
# CHECK32-NEXT:         name       0x\[\[:xdigit:\]\]+ categoryProperty
# CHECK32-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32-NEXT:         name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32-NEXT: Contents of (__DATA_CONST,__objc_protolist) section
# CHECK32-NEXT: \[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ __OBJC_PROTOCOL_$_Verbing
# CHECK32-NEXT:     isa       0x0
# CHECK32-NEXT:     name      0x\[\[:xdigit:\]\]+ Verbing
# CHECK32-NEXT:     protocols 0x0
# CHECK32-NEXT:     instanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_Verbing
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   2
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ verb
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x0
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ i8@0:4
# CHECK32-SKIP:         imp     0x0
# CHECK32:     classMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_Verbing
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   1
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ classVerb
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x0
# CHECK32:     optionalInstanceMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Verbing
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   1
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ optionalVerb
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x0
# CHECK32:     optionalClassMethods 0x\[\[:xdigit:\]\]+ __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Verbing
# CHECK32-SKIP:         entsize 12
# CHECK32-SKIP:         count   1
# CHECK32-SKIP:         name    0x\[\[:xdigit:\]\]+ classOptionalVerb
# CHECK32-SKIP:         types   0x\[\[:xdigit:\]\]+ v8@0:4
# CHECK32-SKIP:         imp     0x0
# CHECK32:     instanceProperties 0x\[\[:xdigit:\]\]+ __OBJC_$_PROP_LIST_Verbing
# CHECK32-NEXT:         entsize    8
# CHECK32-NEXT:         count      1
# CHECK32-NEXT:         name       0x\[\[:xdigit:\]\]+ intProperty
# CHECK32-NEXT:         attributes 0x\[\[:xdigit:\]\]+ Ti,R,N
# CHECK32: Contents of (__DATA_CONST,__objc_imageinfo) section
# CHECK32-NEXT:     version 0
# CHECK32-NEXT:     flags   0x40

clean:
	rm -f test_*
