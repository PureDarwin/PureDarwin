export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

PRIVATE_DATAFILES = \
	board_config.h \
	boot.h \
	protos.h

PRIVATE_KERNELFILES = \
	board_config.h \
	boot.h \
	protos.h

INSTALL_MD_LIST = ${PRIVATE_DATAFILES}

INSTALL_MD_DIR = pexpert/riscv

INSTALL_SF_MD_LCL_LIST = ${PRIVATE_DATAFILES}

EXPORT_MD_LIST = ${PRIVATE_KERNELFILES}

EXPORT_MD_DIR = pexpert/riscv

include $(MakeInc_rule)
include $(MakeInc_dir)
