export MakeInc_cmd=$(SRCROOT)/makedefs/MakeInc.cmd
export MakeInc_def=$(SRCROOT)/makedefs/MakeInc.def
export MakeInc_rule=$(SRCROOT)/makedefs/MakeInc.rule
export MakeInc_dir=$(SRCROOT)/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

RISCV_HEADER_FILES =	\
		atomic.h \
		cpu_number.h \
		cpu_capabilities.h	\
		lock.h \
		locks.h \
		machine_cpuid.h \
		machine_routines.h \
		memory_types.h \
		pal_routines.h \
		simple_lock.h \
		thread.h \
		trap.h

INSTALL_MD_DIR = riscv

INSTALL_SF_MD_LCL_LIST = cpu_capabilities.h

INSTALL_MD_LIST =

INSTALL_MD_LCL_LIST = cpu_capabilities.h

INSTALL_KF_MD_LIST = $(RISCV_HEADER_FILES)

INSTALL_KF_MD_LCL_LIST = \
		cpu_topology.h \
		pmap_public.h \
		proc_reg.h \
		smp.h \
		$(RISCV_HEADER_FILES)

EXPORT_MD_LIST = \
		cpu_affinity.h \
		cpu_data.h \
		caches_internal.h \
		cpu_data_internal.h \
		cpu_internal.h \
		lowglobals.h \
		machine_cpu.h \
		machparam.h \
		misc_protos.h \
		pal_hibernate.h \
		pmap.h \
		cpu_topology.h \
		pmap_public.h \
		proc_reg.h \
		sched_param.h \
		setjmp.h \
		smp.h \
		static_if.h \
		task.h \
		trap_internal.h \
		vm_tuning.h \
		$(RISCV_HEADER_FILES)

EXPORT_MD_DIR = riscv

include $(MakeInc_rule)
include $(MakeInc_dir)
