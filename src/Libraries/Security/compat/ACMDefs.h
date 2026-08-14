/*
 * LocalAuthentication's ACM (Access Control Module) key names. The framework is
 * closed source, but these are only ever used as dictionary keys inside an ACL
 * blob that Security itself both writes and reads, so the names only have to be
 * stable and distinct. They are spelled as they appear in the ACL dictionaries
 * to keep the encoding recognisable.
 */
#ifndef PD_ACMDEFS_H
#define PD_ACMDEFS_H

#define kACMPolicyDeviceOwnerAuthentication "com.apple.LocalAuthentication.DeviceOwnerAuthentication"

#define kACMKeyAclConstraintPolicy       "opa"
#define kACMKeyAclConstraintUserPasscode "upa"
#define kACMKeyAclConstraintBio          "bio"
#define kACMKeyAclConstraintWatch        "wch"
#define kACMKeyAclConstraintKofN         "knm"

#define kACMKeyAclParamBioCatacombUUID   "ccu"
#define kACMKeyAclParamBioDatabaseHash   "dbh"
#define kACMKeyAclParamKofN              "knp"

#endif /* PD_ACMDEFS_H */
