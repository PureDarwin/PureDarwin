export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

IIG_INCFLAGS = -I$(SRCROOT)/iokit -I$(SRCROOT)/osfmk -I$(SRCROOT)/bsd -I$(OBJROOT)/bsd $(INCFLAGS_EXTERN)

INCDIR = $(FRAMEDIR)/$(DKIT_INCDIR)
DRIVERKITINCDIR = $(DRIVERKITFRAMEDIR)/$(DRIVERKIT_DKIT_INCDIR)
LCLDIR = $(FRAMEDIR)/$(DKIT_PINCDIR)

include $(MakeInc_cmd)
include $(MakeInc_def)

ALL_DEFS = $(notdir $(wildcard $(SOURCE)*.iig))
ALL_HDRS = $(notdir $(wildcard $(SOURCE)*.h))

EXPINC_SUBDIRS = ${INSTINC_SUBDIRS}

INSTALL_MI_DIR = .

EXPORT_MI_DIR = DriverKit

INSTALL_MI_LIST = $(ALL_DEFS)
INSTALL_DRIVERKIT_MI_LIST = $(ALL_DEFS)

OTHER_HEADERS = IOTypes.h IOReturn.h IORPC.h IOKitKeys.h

EXPORT_MI_GEN_LIST = $(ALL_HDRS)
INSTALL_MI_GEN_LIST = $(ALL_HDRS)
INSTALL_DRIVERKIT_MI_GEN_LIST = $(ALL_HDRS)
INSTALL_KF_MI_GEN_LIST = $(ALL_HDRS)

include $(MakeInc_rule)
include $(MakeInc_dir)
