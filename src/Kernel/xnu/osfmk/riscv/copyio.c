/*
 * Copyright (c) 2012-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <riscv/cpu_data_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/caches_internal.h>
#include <kern/thread.h>
#include <kern/zalloc_internal.h>
#include <sys/errno.h>
#include <vm/pmap.h>
#include <vm/vm_map_xnu.h>
#include <san/kasan.h>
#include <riscv/pmap.h>

#undef copyin
#undef copyout

extern int _bcopyin(const user_addr_t src, char *dst, vm_size_t len);
extern int _bcopyinstr(const user_addr_t src, char *dst, vm_size_t max, vm_size_t *actual);
extern int _bcopyout(const char *src, user_addr_t dst, vm_size_t len);
extern int _copyin_atomic32(const user_addr_t src, uint32_t *dst);
extern int _copyin_atomic32_wait_if_equals(const user_addr_t src, uint32_t value);
extern int _copyin_atomic64(const user_addr_t src, uint64_t *dst);
extern int _copyout_atomic32(uint32_t u32, user_addr_t dst);
extern int _copyout_atomic64(uint64_t u64, user_addr_t dst);

extern int copyoutstr_prevalidate(const void *kaddr, user_addr_t uaddr, size_t len);

// copyio_flags_t: COPYIO_IN (user -> kernel) or COPYIO_OUT (kernel -> user) is always set.
// ALLOW_KERNEL_TO_KERNEL permits a kernel user_address, VALIDATE_USER_ONLY checks only it, ATOMIC needs alignment.
__options_decl(copyio_flags_t, uint32_t, {
	COPYIO_IN                       = 0x0001,
	COPYIO_OUT                      = 0x0002,
	COPYIO_ALLOW_KERNEL_TO_KERNEL   = 0x0004,
	COPYIO_VALIDATE_USER_ONLY       = 0x0008,
	COPYIO_ATOMIC                   = 0x0010,
});

// Copy sizes bigger than this value will cause a kernel panic. The limit is arbitrary,
// but copying more between user and wired kernel memory at once is almost certainly a bug.
const int copysize_limit_panic = (64 * 1024 * 1024);

static inline bool
is_kernel_to_kernel_copy(pmap_t pmap)
{
	return pmap == kernel_pmap;
}

static int
copy_validate_user_addr(vm_map_t map, const user_addr_t user_addr, vm_size_t nbytes)
{
	user_addr_t user_addr_last;

	if (__improbable(user_addr < vm_map_min(map) ||
	    os_add_overflow(user_addr, nbytes, &user_addr_last) ||
	    user_addr_last > vm_map_max(map))) {
		return EFAULT;
	}

	return 0;
}

static void
copy_validate_kernel_addr(uintptr_t kernel_addr, vm_size_t nbytes)
{
	uintptr_t kernel_addr_last;

	if (__improbable(os_add_overflow(kernel_addr, nbytes, &kernel_addr_last))) {
		panic("%s(%p, %lu) - kaddr not in kernel", __func__,
		    (void *)kernel_addr, nbytes);
	}

	// the physical aperture sits inside the kernel half of sv39
	bool in_kva = (VM_KERNEL_STRIP_PTR(kernel_addr) >= VM_MIN_KERNEL_ADDRESS) &&
	    (VM_KERNEL_STRIP_PTR(kernel_addr_last) <= VM_MAX_KERNEL_ADDRESS);

	if (__improbable(!in_kva)) {
		panic("%s(%p, %lu) - kaddr not in kernel", __func__,
		    (void *)kernel_addr, nbytes);
	}

	zone_element_bounds_check(kernel_addr, nbytes);
}

// Validate the arguments to copy{in,out} on this platform. Returns EXDEV when the
// current thread pmap is the kernel's, which is non fatal for certain routines.
static inline __attribute__((always_inline)) int
copy_validate(vm_map_t map, const user_addr_t user_addr, uintptr_t kernel_addr,
    vm_size_t nbytes, copyio_flags_t flags)
{
	int ret;

	if (__improbable(nbytes > copysize_limit_panic)) {
		return EINVAL;
	}

	ret = copy_validate_user_addr(map, user_addr, nbytes);
	if (__improbable(ret)) {
		return ret;
	}

	if (flags & COPYIO_ATOMIC) {
		if (__improbable(user_addr & (nbytes - 1))) {
			return EINVAL;
		}
	}

	if ((flags & COPYIO_VALIDATE_USER_ONLY) == 0) {
		copy_validate_kernel_addr(kernel_addr, nbytes);
#if KASAN
		/* For user copies, asan-check the kernel-side buffer */
		if (flags & COPYIO_IN) {
			__asan_storeN(kernel_addr, nbytes);
		} else {
			__asan_loadN(kernel_addr, nbytes);
		}
#endif
	}

	if (is_kernel_to_kernel_copy(map->pmap)) {
		if (__improbable((flags & COPYIO_ALLOW_KERNEL_TO_KERNEL) == 0)) {
			return EFAULT;
		}
		return EXDEV;
	}

	return 0;
}

int
copyin_kern(const user_addr_t user_addr, char *kernel_addr, vm_size_t nbytes)
{
	bcopy((const char*)(uintptr_t)user_addr, kernel_addr, nbytes);

	return 0;
}

int
copyout_kern(const char *kernel_addr, user_addr_t user_addr, vm_size_t nbytes)
{
	bcopy(kernel_addr, (char *)(uintptr_t)user_addr, nbytes);

	return 0;
}

int
copyin(const user_addr_t user_addr, void *kernel_addr, vm_size_t nbytes)
{
	vm_map_t map = current_thread()->map;
	int result;

	if (__improbable(nbytes == 0)) {
		return 0;
	}

	result = copy_validate(map, user_addr, (uintptr_t)kernel_addr, nbytes,
	    COPYIO_IN | COPYIO_ALLOW_KERNEL_TO_KERNEL);
	if (result == EXDEV) {
		return copyin_kern(user_addr, kernel_addr, nbytes);
	}
	if (__improbable(result)) {
		return result;
	}

	return _bcopyin(user_addr, kernel_addr, nbytes);
}

// copy{in,out}_atomic{32,64}: read or store an aligned userspace value as a single
// memory transaction, for userspace synchronization features.
int
copyin_atomic32(const user_addr_t user_addr, uint32_t *kernel_addr)
{
	vm_map_t map = current_thread()->map;
	int result = copy_validate(map, user_addr, (uintptr_t)kernel_addr, 4,
	    COPYIO_IN | COPYIO_ATOMIC);
	if (__improbable(result)) {
		return result;
	}

	return _copyin_atomic32(user_addr, kernel_addr);
}

int
copyin_atomic32_wait_if_equals(const user_addr_t user_addr, uint32_t value)
{
	vm_map_t map = current_thread()->map;
	int result = copy_validate(map, user_addr, 0, 4,
	    COPYIO_OUT | COPYIO_ATOMIC | COPYIO_VALIDATE_USER_ONLY);
	if (__improbable(result)) {
		return result;
	}

	return _copyin_atomic32_wait_if_equals(user_addr, value);
}

int
copyin_atomic64(const user_addr_t user_addr, uint64_t *kernel_addr)
{
	vm_map_t map = current_thread()->map;
	int result = copy_validate(map, user_addr, (uintptr_t)kernel_addr, 8,
	    COPYIO_IN | COPYIO_ATOMIC);
	if (__improbable(result)) {
		return result;
	}

	return _copyin_atomic64(user_addr, kernel_addr);
}

int
copyout_atomic32(uint32_t value, user_addr_t user_addr)
{
	vm_map_t map = current_thread()->map;
	int result = copy_validate(map, user_addr, 0, 4,
	    COPYIO_OUT | COPYIO_ATOMIC | COPYIO_VALIDATE_USER_ONLY);
	if (__improbable(result)) {
		return result;
	}

	return _copyout_atomic32(value, user_addr);
}

int
copyout_atomic64(uint64_t value, user_addr_t user_addr)
{
	vm_map_t map = current_thread()->map;
	int result = copy_validate(map, user_addr, 0, 8,
	    COPYIO_OUT | COPYIO_ATOMIC | COPYIO_VALIDATE_USER_ONLY);
	if (__improbable(result)) {
		return result;
	}

	return _copyout_atomic64(value, user_addr);
}

int
copyinstr(const user_addr_t user_addr, char *kernel_addr, vm_size_t nbytes, vm_size_t *lencopied)
{
	vm_map_t map = current_thread()->map;
	int result;
	vm_size_t bytes_copied = 0;

	*lencopied = 0;
	if (__improbable(nbytes == 0)) {
		return ENAMETOOLONG;
	}

	result = copy_validate(map, user_addr, (uintptr_t)kernel_addr, nbytes, COPYIO_IN);
	if (__improbable(result)) {
		return result;
	}

	result = _bcopyinstr(user_addr, kernel_addr, nbytes, &bytes_copied);

	if (result != EFAULT) {
		*lencopied = bytes_copied;
	}
	return result;
}

int
copyout(const void *kernel_addr, user_addr_t user_addr, vm_size_t nbytes)
{
	vm_map_t map = current_thread()->map;
	int result;

	if (nbytes == 0) {
		return 0;
	}

	result = copy_validate(map, user_addr, (uintptr_t)kernel_addr, nbytes,
	    COPYIO_OUT | COPYIO_ALLOW_KERNEL_TO_KERNEL);
	if (result == EXDEV) {
		return copyout_kern(kernel_addr, user_addr, nbytes);
	}
	if (__improbable(result)) {
		return result;
	}

	return _bcopyout(kernel_addr, user_addr, nbytes);
}

int
copyoutstr_prevalidate(const void *__unused kaddr, user_addr_t __unused uaddr, size_t __unused len)
{
	vm_map_t map = current_thread()->map;

	if (__improbable(is_kernel_to_kernel_copy(map->pmap))) {
		return EFAULT;
	}

	return 0;
}

// Validate the user address range of a copy without doing it,
// EFAULT when any of it lies outside the current map.
int
copyio_check_user_addr(user_addr_t user_addr, vm_size_t nbytes)
{
	vm_map_t map = current_thread()->map;

	if (__improbable(nbytes > copysize_limit_panic)) {
		return EINVAL;
	}

	return copy_validate_user_addr(map, user_addr, nbytes);
}

#if (DEBUG || DEVELOPMENT)
int
verify_write(const void *source, void *dst, size_t size)
{
	int rc;
	disable_preemption();
	rc = _bcopyout((const char*)source, (user_addr_t)dst, size);
	enable_preemption();
	return rc;
}
#endif

// Routine: ml_nofault_copy, a physical mode copy if source and destination have valid kernel pmap
// translations. They are assumed wired, nothing guarantees they stay valid during the copy.
vm_size_t
ml_nofault_copy(vm_offset_t virtsrc, vm_offset_t virtdst, vm_size_t size)
{
	addr64_t        cur_phys_dst, cur_phys_src;
	vm_size_t       count, nbytes = 0;

	while (size > 0) {
		if (!(cur_phys_src = kvtophys(virtsrc))) {
			break;
		}
		if (!(cur_phys_dst = kvtophys(virtdst))) {
			break;
		}
		if (!pmap_valid_address(trunc_page_64(cur_phys_dst)) ||
		    !pmap_valid_address(trunc_page_64(cur_phys_src))) {
			break;
		}
		count = PAGE_SIZE - (cur_phys_src & PAGE_MASK);
		if (count > (PAGE_SIZE - (cur_phys_dst & PAGE_MASK))) {
			count = PAGE_SIZE - (cur_phys_dst & PAGE_MASK);
		}
		if (count > size) {
			count = size;
		}

		bcopy_phys(cur_phys_src, cur_phys_dst, count);

		nbytes += count;
		virtsrc += count;
		virtdst += count;
		size -= count;
	}

	return nbytes;
}

// Routine: ml_validate_nofault, TRUE if the range has valid kernel pmap translations and will not fault.
// Translations are assumed wired, nothing guarantees they persist after the check.

boolean_t
ml_validate_nofault(
	vm_offset_t virtsrc, vm_size_t size)
{
	addr64_t cur_phys_src;
	uint32_t count;

	while (size > 0) {
		if (!(cur_phys_src = kvtophys(virtsrc))) {
			return FALSE;
		}
		if (!pmap_valid_address(trunc_page_64(cur_phys_src))) {
			return FALSE;
		}
		count = (uint32_t)(PAGE_SIZE - (cur_phys_src & PAGE_MASK));
		if (count > size) {
			count = (uint32_t)size;
		}

		virtsrc += count;
		size -= count;
	}

	return TRUE;
}

#define COPYPV_SRC_IS_PHYS(flags) (((flags) & cppvPsrc) != 0)
#define COPYPV_DST_IS_PHYS(flags) (((flags) & cppvPsnk) != 0)
#define COPYPV_SRC_IS_USER(flags) (((flags) & (cppvPsrc | cppvKmap)) == 0)
#define COPYPV_DST_IS_USER(flags) (((flags) & (cppvPsnk | cppvKmap)) == 0)

// a kernel address for a physical one, the aperture for default cacheable pages
// else a per cpu copy window, which keeps preemption off until copypv_unmap_phys
static char *
copypv_map_phys(addr64_t pa, vm_prot_t prot, vm_size_t *count, int *window)
{
	ppnum_t pn = (ppnum_t)(pa >> PAGE_SHIFT);
	unsigned int wimg = pmap_cache_attributes(pn);

	*window = -1;
	if (pmap_valid_address(pa) && (wimg & VM_WIMG_MASK) == VM_WIMG_DEFAULT) {
		return (char *)phystokv_range((pmap_paddr_t)pa, count);
	}

	mp_disable_preemption();
	*window = (int)pmap_map_cpu_windows_copy(pn, prot, wimg);
	if (*count > PAGE_SIZE - (pa & PAGE_MASK)) {
		*count = PAGE_SIZE - (pa & PAGE_MASK);
	}
	return (char *)(pmap_cpu_windows_copy_addr(cpu_number(), *window) + (pa & PAGE_MASK));
}

static void
copypv_unmap_phys(int window)
{
	if (window >= 0) {
		pmap_unmap_cpu_windows_copy((unsigned int)window);
		mp_enable_preemption();
	}
}

kern_return_t
copypv(addr64_t source, addr64_t sink, unsigned int size, int which)
{
	kern_return_t res = KERN_SUCCESS;
	addr64_t src = source, dst = sink;
	vm_size_t bytes = size;

	if ((which & (cppvPsrc | cppvPsnk)) == 0) {     /* Make sure that only one is virtual */
		panic("%s: no more than 1 parameter may be virtual", __func__);
	}

	if (COPYPV_SRC_IS_PHYS(which) && COPYPV_DST_IS_PHYS(which)) {
		bcopy_phys(source, sink, size);
		bytes = 0;
	}

	while (bytes > 0 && res == KERN_SUCCESS) {
		vm_size_t count = bytes;
		char *tmp_src = (char *)src;
		char *tmp_dst = (char *)dst;
		int window;

		if (COPYPV_SRC_IS_PHYS(which)) {
			tmp_src = copypv_map_phys(src, VM_PROT_READ, &count, &window);
		} else {
			tmp_dst = copypv_map_phys(dst, VM_PROT_READ | VM_PROT_WRITE, &count, &window);
		}

		if (COPYPV_SRC_IS_USER(which)) {
			res = copyin((user_addr_t)src, tmp_dst, count);
		} else if (COPYPV_DST_IS_USER(which)) {
			res = copyout(tmp_src, (user_addr_t)dst, count);
		} else {
			bcopy(tmp_src, tmp_dst, count);
		}

		copypv_unmap_phys(window);

		src += count;
		dst += count;
		bytes -= count;
	}

	if (which & cppvFsrc) {
		flush_dcache64(source, size, ((which & cppvPsrc) == cppvPsrc));
	}

	if (which & cppvFsnk) {
		flush_dcache64(sink, size, ((which & cppvPsnk) == cppvPsnk));
	}

	return res;
}
