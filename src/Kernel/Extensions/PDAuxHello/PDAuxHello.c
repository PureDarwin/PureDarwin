// an example kext built outside the boot KC, it says hello from the auxiliary KC it was linked into
#include <mach/mach_types.h>
#include <libkern/libkern.h>

extern void IOLog(const char *format, ...);

static int pd_aux_hello_calls;

kern_return_t
pd_aux_hello_start(kmod_info_t *ki, void *data)
{
	(void)data;
	pd_aux_hello_calls++;
	printf("PDAuxHello: started from the auxiliary KC, kmod %s at %p\n", ki ? ki->name : "?", (void *)ki);
	IOLog("PDAuxHello: start called %d time(s)\n", pd_aux_hello_calls);
	return KERN_SUCCESS;
}

kern_return_t
pd_aux_hello_stop(kmod_info_t *ki, void *data)
{
	(void)ki;
	(void)data;
	printf("PDAuxHello: stopped\n");
	return KERN_SUCCESS;
}
