#pragma once

#include <stdint.h>
#include <stddef.h>

#include "intel.h"

#ifdef __cplusplus
extern "C" {
#endif

enum LilLogType {
	ERROR,
	WARNING,
	INFO,
	VERBOSE,
	DEBUG,
};

#ifndef __STDC_HOSTED__

__attribute__((nonnull(1, 2))) void *memcpy(void *dest, const void *src, size_t n);
__attribute__((nonnull(1, 2))) int memcmp(const void *s1, const void *s2, size_t n);
__attribute__((nonnull(1))) void *memset(void *s, int c, size_t n);
__attribute__((nonnull(1, 2))) int strcmp(const char *s1, const char *s2);

#else

#include <string.h>

#endif

__attribute__((nonnull(1))) void lil_pci_writeb(void* device, uint16_t offset, uint8_t val);
__attribute__((nonnull(1))) uint8_t lil_pci_readb(void* device, uint16_t offset);
__attribute__((nonnull(1))) void lil_pci_writew(void* device, uint16_t offset, uint16_t val);
__attribute__((nonnull(1))) uint16_t lil_pci_readw(void* device, uint16_t offset);
__attribute__((nonnull(1))) void lil_pci_writed(void* device, uint16_t offset, uint32_t val);
__attribute__((nonnull(1))) uint32_t lil_pci_readd(void* device, uint16_t offset);

__attribute__((weak)) void lil_outb(uint16_t port, uint8_t value);
__attribute__((weak)) void lil_outw(uint16_t port, uint16_t value);
__attribute__((weak)) void lil_outd(uint16_t port, uint32_t value);
__attribute__((weak)) uint8_t lil_inb(uint16_t port);
__attribute__((weak)) uint16_t lil_inw(uint16_t port);
__attribute__((weak)) uint32_t lil_ind(uint16_t port);

void lil_sleep(uint64_t ms);
void lil_usleep(uint64_t us);
__attribute__((malloc, returns_nonnull)) void* lil_malloc(size_t s);
__attribute__((nonnull(1))) void lil_free(void* p);
__attribute__((nonnull(1, 3, 4))) void lil_get_bar(void *device, int bar, uintptr_t *obase, uintptr_t *len);
__attribute__((weak, returns_nonnull)) void* lil_map(size_t loc, size_t len);
__attribute__((weak, nonnull(1))) void lil_unmap(void *loc, size_t len);
__attribute__((format(printf, 2, 3), nonnull(2))) void lil_log(enum LilLogType type, const char *fmt, ...);
__attribute__((noreturn, nonnull(1))) void lil_panic(const char* msg);

#define lil_assert(x) if(!(x)) { \
	lil_log(ERROR, "assertion '%s' failed (%s:%u)\n", #x, __FILE__, __LINE__); \
	lil_panic("assertion failed"); \
}

__attribute__((weak)) const struct vbt_header *lil_vbt_locate(struct LilGpu *gpu);

#ifdef __cplusplus
}
#endif
