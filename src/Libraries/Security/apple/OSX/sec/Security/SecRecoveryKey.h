//
//  SecRecoveryKey.h
//
//

#ifndef SecRecoveryKey_h
#define SecRecoveryKey_h

#include <Security/Security.h>
#if __OBJC__
@class SecRecoveryKey;
#else
typedef struct __SecRecoveryKey SecRecoveryKey;
#endif

bool
SecRKRegisterBackupPublicKey(SecRecoveryKey *rk, CFErrorRef *error);

#if __OBJC__

/*
 * Constants for the verifier dictionary returned from SecRKCopyAccountRecoveryVerifier
 */

extern NSString *const kSecRVSalt;
extern NSString *const kSecRVIterations;
extern NSString *const kSecRVProtocol;
extern NSString *const kSecRVVerifier;
extern NSString *const kSecRVMasterID;


SecRecoveryKey *
SecRKCreateRecoveryKey(NSString *recoveryKey);

SecRecoveryKey *
SecRKCreateRecoveryKeyWithError(NSString *masterKey, NSError **error);

NSString *
SecRKCreateRecoveryKeyString(NSError **error);

NSString *
SecRKCopyAccountRecoveryPassword(SecRecoveryKey *rk);

NSData *
SecRKCopyBackupFullKey(SecRecoveryKey *rk);

NSData *
SecRKCopyBackupPublicKey(SecRecoveryKey *rk);

NSDictionary *
SecRKCopyAccountRecoveryVerifier(NSString *recoveryKey,
                                 NSError **error);

#else

SecRecoveryKey *
SecRKCreateRecoveryKey(CFStringRef recoveryKey);

CFDataRef
SecRKCopyBackupFullKey(SecRecoveryKey *rk);

CFDataRef
SecRKCopyBackupPublicKey(SecRecoveryKey *rk);

#endif

#endif
