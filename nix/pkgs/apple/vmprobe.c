/* Dump every input fastfetch's memory line is computed from. */
#include <stdio.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

int main(void)
{
	vm_statistics64_data_t v;
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	uint64_t memsize = 0;
	int pagesize = 0;
	size_t l;

	l = sizeof(memsize);
	sysctl((int[]){ CTL_HW, HW_MEMSIZE }, 2, &memsize, &l, NULL, 0);
	l = sizeof(pagesize);
	sysctl((int[]){ CTL_HW, HW_PAGESIZE }, 2, &pagesize, &l, NULL, 0);
	printf("hw.memsize      %llu (%llu MiB)\n", memsize, memsize >> 20);
	printf("hw.pagesize     %d\n", pagesize);

	count = HOST_VM_INFO64_COUNT;
	kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64,
	    (host_info64_t)&v, &count);
	printf("host_statistics64 kr=%d count=%u (asked %u)\n",
	    kr, count, (unsigned)HOST_VM_INFO64_COUNT);
	if (kr != KERN_SUCCESS) return 1;

	printf("free            %llu\n", (unsigned long long)v.free_count);
	printf("active          %llu\n", (unsigned long long)v.active_count);
	printf("inactive        %llu\n", (unsigned long long)v.inactive_count);
	printf("wire            %llu\n", (unsigned long long)v.wire_count);
	printf("speculative     %llu\n", (unsigned long long)v.speculative_count);
	printf("external        %llu\n", (unsigned long long)v.external_page_count);
	printf("internal        %llu\n", (unsigned long long)v.internal_page_count);
	printf("compressor      %llu\n", (unsigned long long)v.compressor_page_count);
	printf("purgeable       %llu\n", (unsigned long long)v.purgeable_count);
	printf("throttled       %llu\n", (unsigned long long)v.throttled_count);

	printf("\ntotal pages at hw.pagesize %llu\n",
	    pagesize ? memsize / (unsigned)pagesize : 0);
	printf("fastfetch free-spec        %llu\n",
	    (unsigned long long)(v.free_count - v.speculative_count));
	printf("fastfetch (free-spec)+ext  %llu\n",
	    (unsigned long long)((v.free_count - v.speculative_count) +
	    v.external_page_count));
	return 0;
}
