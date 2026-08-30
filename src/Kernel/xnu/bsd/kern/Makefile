export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

EXPORT_MI_DIR = kern

EXPORT_MI_LIST = qsort.h

# Don't want these XNU-internal headers installed in the SDK
INSTALL_KF_MI_LIST = $(empty)
INSTALL_KF_MI_LCL_LIST = $(empty)

include $(MakeInc_rule)
include $(MakeInc_dir)
