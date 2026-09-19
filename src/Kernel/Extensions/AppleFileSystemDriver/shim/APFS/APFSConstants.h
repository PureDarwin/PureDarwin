/*
 * Shim for APFS/APFSConstants.h - only the few constants AppleFileSystemDriver
 * references.  We are booting an HFS+ root, so the APFS matching path is inert;
 * these definitions exist purely to let the driver compile.
 */
#ifndef _APFS_CONSTANTS_SHIM_H
#define _APFS_CONSTANTS_SHIM_H

#define APFS_VOLUME_OBJECT      "AppleAPFSVolume"
#define kAPFSVolGroupUUIDKey    "VolGroupUUID"     /* as ioreg shows it */
#define kAPFSRoleValueKey       "RoleValue"       /* OSNumber; "Role" is the name array */

#define APFS_VOL_ROLE_NONE      0x0000
#define APFS_VOL_ROLE_SYSTEM    0x0001

#endif /* _APFS_CONSTANTS_SHIM_H */
