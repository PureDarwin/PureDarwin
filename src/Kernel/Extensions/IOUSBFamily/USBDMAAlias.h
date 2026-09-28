#ifndef _USB_DMA_ALIAS_H
#define _USB_DMA_ALIAS_H

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMemoryDescriptor.h>

// cpu view of a dma buffer, cached when the bus snoops and write-combine otherwise
// a non-coherent soc controller never sees stale cache lines through the alias
static inline void *
usbDMAAlias(IOBufferMemoryDescriptor *mem, bool coherent, IOMemoryMap **mapOut)
{
	IOMemoryMap *map;

	*mapOut = NULL;
	if (mem == NULL)
		return NULL;
	if (coherent)
		return mem->getBytesNoCopy();

	// clean and drop the cached alias first so it can never write back over dma data
	mem->performOperation(kIOMemoryIncoherentIOFlush, 0, mem->getLength());
	map = mem->createMappingInTask(kernel_task, 0,
	    kIOMapAnywhere | kIOMapWriteCombineCache);
	if (map == NULL)
		return NULL;
	*mapOut = map;
	return (void *)map->getVirtualAddress();
}

// clean and invalidate a per-transfer buffer, before handing it over and before reading it back
static inline void
usbDMASync(IOMemoryDescriptor *mem, bool coherent)
{
	if (!coherent && mem != NULL)
		mem->performOperation(kIOMemoryIncoherentIOFlush, 0, mem->getLength());
}

// order cpu writes to dma memory before the controller is told to look at it
static inline void
usbDMAWriteBarrier(void)
{
#if defined(__arm64__)
	__asm__ volatile ("dsb sy" ::: "memory");
#else
	__sync_synchronize();
#endif
}

#endif
