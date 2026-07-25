#pragma once

#define GTT_PAGE_PRESENT 1

#define GTT64_ENTRY(gpu, gpu_addr) (*(volatile uint64_t *) ((gpu)->gtt_address + ((gpu_addr) >> 12) * 8))
#define GTT32_ENTRY(gpu, gpu_addr) (*(volatile uint32_t *) ((gpu)->gtt_address + ((gpu_addr) >> 12) * 4))
