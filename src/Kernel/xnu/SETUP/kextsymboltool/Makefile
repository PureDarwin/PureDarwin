export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

OBJS = kextsymboltool.o

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OBJS += swap.o
endif

CFLAGS = -isysroot $(HOST_SDKROOT) -mmacosx-version-min=$(HOST_OS_VERSION) -g -O0 -I$(SOURCE) -I.

WARNFLAGS = -Wall

LDFLAGS = -isysroot $(HOST_SDKROOT) -mmacosx-version-min=$(HOST_OS_VERSION) -lstdc++

# If we are a Linux host, append luuid
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -luuid
endif

kextsymboltool: $(OBJS)
	@$(LOG_HOST_LD) "$@"
	$(_v)$(HOST_CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	@$(LOG_HOST_CC) "$@"
	$(_v)$(HOST_CC) $(WARNFLAGS) $(CFLAGS) -c -o $@ $<

swap.o: $(SRCROOT)/../../../tools/cctools/libmacho/swap.c
	@$(LOG_HOST_CC) "$@"
	$(_v)$(HOST_CC) $(WARNFLAGS) $(CFLAGS) -c -o $@ $<

do_build_setup:: kextsymboltool

include $(MakeInc_rule)
include $(MakeInc_dir)
