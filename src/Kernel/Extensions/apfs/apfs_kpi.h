/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#ifndef _PUREDARWIN_APFS_KPI_H_
#define _PUREDARWIN_APFS_KPI_H_

// container operations apfs.kext exports to other kexts (ApfsFileSystemDriver).
// apfs.kext owns every libapfsrw write to a container, mounted or not

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct apfsrw_kern_dev;

// add an empty volume to the container with this nx_uuid: under the mounts' lock when mounted,
// else on fallback (ENOENT if NULL). returns an errno, the nx_fs_oid[] slot in *slot
int pd_apfs_volume_add(const uint8_t container_uuid[16],
    struct apfsrw_kern_dev *fallback, uint64_t fallback_blocks,
    const char *name, uint16_t role, const uint8_t volume_uuid[16],
    uint32_t *slot);

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_APFS_KPI_H_ */
