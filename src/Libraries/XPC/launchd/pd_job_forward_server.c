#include <mach/mach.h>
#include <bootstrap_priv.h>
#include "core.h"

kern_return_t
job_mig_look_up2_forward(job_t j, mach_port_t rp, name_t servicename,
    pid_t targetpid, uuid_t instanceid, uint64_t flags)
{
	(void)j;
	(void)rp;
	(void)servicename;
	(void)targetpid;
	(void)instanceid;
	(void)flags;
	return BOOTSTRAP_NOT_PRIVILEGED;
}

kern_return_t
job_mig_parent_forward(job_t j, mach_port_t rp)
{
	(void)j;
	(void)rp;
	return BOOTSTRAP_NOT_PRIVILEGED;
}
