/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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

#ifndef _SYS_DISK_PRIVATE_H_
#define _SYS_DISK_PRIVATE_H_

#include <sys/disk.h>

#ifdef XNU_KERNEL_PRIVATE
#include <mach/boolean.h>
#endif /* XNU_KERNEL_PRIVATE */

#ifdef KERNEL

/* Definitions of option bits for dk_unmap_t */
#define _DK_UNMAP_INITIALIZE                   0x00000100

#ifdef XNU_KERNEL_PRIVATE
typedef struct{
	boolean_t mi_mdev; /* Is this a memdev device? */
	boolean_t mi_phys; /* Physical memory? */
	uint32_t mi_base; /* Base page number of the device? */
	uint64_t mi_size; /* Size of the device (in ) */
} dk_memdev_info_t;

typedef dk_memdev_info_t memdev_info_t;

#define DKIOCGETMEMDEVINFO                    _IOR('d', 90, dk_memdev_info_t)
#endif /* XNU_KERNEL_PRIVATE */
typedef struct _dk_cs_pin {
	dk_extent_t     cp_extent;
	int64_t         cp_flags;
} _dk_cs_pin_t;
/* The following are modifiers to _DKIOCCSPINEXTENT/cp_flags operation */
#define _DKIOCCSPINTOFASTMEDIA          (0)                     /* Pin extent to the fast (SSD) media             */
#define _DKIOCCSPINFORHIBERNATION       (1 << 0)        /* Pin of hibernation file, content not preserved */
#define _DKIOCCSPINDISCARDDENYLIST      (1 << 1)        /* Hibernation complete/error, stop denylist-ing  */
#define _DKIOCCSPINTOSLOWMEDIA          (1 << 2)        /* Pin extent to the slow (HDD) media             */
#define _DKIOCCSTEMPORARYPIN            (1 << 3)        /* Relocate, but do not pin, to indicated media   */
#define _DKIOCCSHIBERNATEIMGSIZE        (1 << 4)        /* Anticipate/Max size of the upcoming hibernate  */
#define _DKIOCCSPINFORSWAPFILE          (1 << 5)        /* Pin of swap file, content not preserved        */

#define _DKIOCCSSETLVNAME                     _IOW('d', 198, char[256])
#define _DKIOCCSPINEXTENT                     _IOW('d', 199, _dk_cs_pin_t)
#define _DKIOCCSUNPINEXTENT                   _IOW('d', 200, _dk_cs_pin_t)
#define _DKIOCGETMIGRATIONUNITBYTESIZE        _IOR('d', 201, uint32_t)

typedef struct _dk_cs_map {
	dk_extent_t     cm_extent;
	uint64_t        cm_bytes_mapped;
} _dk_cs_map_t;

typedef struct _dk_cs_unmap {
	dk_extent_t                  *extents;
	uint32_t                     extentsCount;
	uint32_t                     options;
} _dk_cs_unmap_t;

#define _DKIOCCSMAP                           _IOWR('d', 202, _dk_cs_map_t)
// No longer used: _DKIOCCSSETFSVNODE (203) & _DKIOCCSGETFREEBYTES (204)
#define _DKIOCCSUNMAP                         _IOWR('d', 205, _dk_cs_unmap_t)

typedef enum {
	DK_APFS_ONE_DEVICE = 1,
	DK_APFS_FUSION
} dk_apfs_flavour_t;

#define DKIOCGETAPFSFLAVOUR     _IOR('d', 91, dk_apfs_flavour_t)

// Extent's offset and length returned in bytes
typedef struct dk_apfs_wbc_range {
	dev_t dev;              // Physical device for extents
	uint32_t count;         // Number of extents
	dk_extent_t extents[2]; // Addresses are relative to device we return
} dk_apfs_wbc_range_t;

#define DKIOCAPFSGETWBCRANGE           _IOR('d', 92, dk_apfs_wbc_range_t)
#define DKIOCAPFSRELEASEWBCRANGE       _IO('d', 93)

#define DKIOCGETMAXSWAPWRITE           _IOR('d', 94, uint64_t)

#endif /* KERNEL */

#define _DKIOCSETSTATIC                       _IO('d', 84)

#endif  /* _SYS_DISK_PRIVATE_H_ */
