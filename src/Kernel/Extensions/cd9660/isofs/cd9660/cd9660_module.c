//
// Copyright (C) 2026, Samuel Zormeister.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
//  cd9960_init.c
//  cd9660_kext
//
//  Created by Samuel Zormeister on 24/7/26.
//

#include <sys/types.h>
#include <vfs/vfs_support.h>
#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <mach/kmod.h>
#include <pexpert/pexpert.h>

static vfstable_t cd9660_vfstable;

extern struct vfsops cd9660_vfsops;

extern struct vnodeopv_desc cd9660_vnodeop_opv_desc;
extern struct vnodeopv_desc cd9660_cdxaop_opv_desc;
extern struct vnodeopv_desc cd9660_specop_opv_desc;
extern struct vnodeopv_desc cd9660_fifoop_opv_desc;

struct vnodeopv_desc *cd9660_vnodeop_opv_desc_list[] = {
    [0] = &cd9660_vnodeop_opv_desc,
    [1] = &cd9660_cdxaop_opv_desc,
    [2] = &cd9660_specop_opv_desc,
    [3] = &cd9660_fifoop_opv_desc,
};

kern_return_t cd9660_module_start(kmod_info_t *ki, void *d)
{
    struct vfs_fsentry fsentry;
    
    
    kprintf("cd9660: hello world\n");
    
    cd9660_hashinit();
    
    bzero(&fsentry, sizeof(fsentry));
    fsentry.vfe_vfsops = &cd9660_vfsops;
    fsentry.vfe_vopcnt = 4;
    fsentry.vfe_opvdescs = cd9660_vnodeop_opv_desc_list;
    // no VFS_TBLREADDIR_EXTENDED: cd9660_readdir emits classic dirents and xnu converts them
    fsentry.vfe_flags = VFS_TBLTHREADSAFE   |
                        VFS_TBLFSNODELOCK   |
                        VFS_TBLNOTYPENUM    |
                        VFS_TBLLOCALVOL     |
                        VFS_TBL64BITREADY;

    strlcpy(fsentry.vfe_fsname, "cd9660", sizeof(fsentry.vfe_fsname));
    
    if (vfs_fsadd(&fsentry, &cd9660_vfstable) != 0) {
        return KERN_FAILURE;
    }
    
    return KERN_SUCCESS;
}

kern_return_t cd9660_module_stop(kmod_info_t *ki, void *d)
{
    if (vfs_fsremove(cd9660_vfstable) == KERN_SUCCESS) {
        cd9660_hashfree();
        return KERN_SUCCESS;
    }
    
    return KERN_SUCCESS;
}

