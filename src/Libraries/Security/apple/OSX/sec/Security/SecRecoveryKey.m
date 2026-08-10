//
//  SecRecoveryKey.c
//

#import "SecRecoveryKey.h"
#import <dispatch/dispatch.h>


#import <corecrypto/cchkdf.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>
#import <corecrypto/ccrng.h>

#import <utilities/SecCFWrappers.h>
#import <AssertMacros.h>


#import <Security/SecureObjectSync/SOSCloudCircle.h>
#import "keychain/SecureObjectSync/SOSInternal.h"

#if !TARGET_OS_BRIDGE
#include <dlfcn.h>
#include <AppleIDAuthSupport/AppleIDAuthSupport.h>
#define PATH_FOR_APPLEIDAUTHSUPPORTFRAMEWORK "/System/Library/PrivateFrameworks/AppleIDAuthSupport.framework/AppleIDAuthSupport"
#endif

#import "SecCFAllocator.h"
#import "SecPasswordGenerate.h"
#import "SecBase64.h"

typedef struct _CFSecRecoveryKey *CFSecRecoveryKeyRef;


static uint8_t backupPublicKey[] = { 'B', 'a', 'c', 'k', 'u', ' ', 'P', 'u', 'b', 'l', 'i', 'c', 'k', 'e', 'y' };
static uint8_t passwordInfoKey[] = { 'p', 'a', 's', 's', 'w', 'o', 'r', 'd', ' ', 's', 'e', 'c', 'r', 'e', 't' };
#if !(defined(__i386__) || TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE)
static uint8_t masterkeyIDSalt[] = { 'M', 'a', 's', 't', 'e', 'r', ' ', 'K', 'e', 'y', ' ', 'I', 'd', 'e', 't' };
#endif

#define RK_BACKUP_HKDF_SIZE    128
#define RK_PASSWORD_HKDF_SIZE  32

CFGiblisFor(CFSecRecoveryKey);

struct _CFSecRecoveryKey {
    CFRuntimeBase _base;
    CFDataRef basecode;
};

static void
CFSecRecoveryKeyDestroy(CFTypeRef cf)
{
    CFSecRecoveryKeyRef rk = (CFSecRecoveryKeyRef)cf;
    CFReleaseNull(rk->basecode);
}


static CFStringRef
CFSecRecoveryKeyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SecRecoveryKey: %p>"), cf);
}


static bool
ValidateRecoveryKey(CFStringRef masterkey, NSError **error)
{
    CFErrorRef cferror = NULL;
    bool res = SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, masterkey, &cferror);
    if (!res) {
        if (error) {
            *error = CFBridgingRelease(cferror);
        } else {
            CFReleaseNull(cferror);
        }
    }
    return res;
}

NSString *
SecRKCreateRecoveryKeyString(NSError **error)
{
    CFErrorRef cferror = NULL;

    CFStringRef recoveryKey = SecPasswordGenerate(kSecPasswordTypeiCloudRecoveryKey, &cferror, NULL);
    if (recoveryKey == NULL) {
        if (error) {
            *error = CFBridgingRelease(cferror);
        } else {
            CFReleaseNull(cferror);
        }
        return NULL;
    }
    if (!ValidateRecoveryKey(recoveryKey, error)) {
        CFRelease(recoveryKey);
        return NULL;
    }
    return (__bridge NSString *)recoveryKey;
}

SecRecoveryKey *
SecRKCreateRecoveryKey(NSString *masterKey)
{
    return SecRKCreateRecoveryKeyWithError(masterKey, NULL);
}

SecRecoveryKey *
SecRKCreateRecoveryKeyWithError(NSString *masterKey, NSError **error)
{
    if (!ValidateRecoveryKey((__bridge CFStringRef)masterKey, error)) {
        return NULL;
    }

    CFSecRecoveryKeyRef rk = CFTypeAllocate(CFSecRecoveryKey, struct _CFSecRecoveryKey, NULL);
    if (rk == NULL)
        return NULL;

    rk->basecode = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(),
                                                        (__bridge CFStringRef)masterKey,
                                                        kCFStringEncodingUTF8, 0);
    if (rk->basecode == NULL) {
        CFRelease(rk);
        return NULL;
    }
    return (SecRecoveryKey *) CFBridgingRelease(rk);
}

static CFDataRef
SecRKCreateDerivedSecret(CFSecRecoveryKeyRef rk, CFIndex outputLength,
                         const uint8_t *variant, size_t variantLength)
{
    CFMutableDataRef derived;
    int status;

    derived = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), outputLength);
    if (derived == NULL)
        return NULL;

    status = cchkdf(ccsha256_di(),
                    CFDataGetLength(rk->basecode), CFDataGetBytePtr(rk->basecode),
                    4, "salt",
                    variantLength, variant,
                    CFDataGetLength(derived), CFDataGetMutableBytePtr(derived));
    if (status) {
        CFReleaseNull(derived);
    }
    return derived;
}


NSString *
SecRKCopyAccountRecoveryPassword(SecRecoveryKey *rk)
{
    CFStringRef base64Data = NULL;
    CFDataRef derived = NULL;
    void *b64string = NULL;
    size_t base64Len = 0;

    derived = SecRKCreateDerivedSecret((__bridge CFSecRecoveryKeyRef)rk,
                                       RK_PASSWORD_HKDF_SIZE,
                                       passwordInfoKey, sizeof(passwordInfoKey));
    require(derived, fail);

    base64Len = SecBase64Encode(CFDataGetBytePtr(derived), CFDataGetLength(derived), NULL, 0);
    assert(base64Len < 1024);

    b64string = malloc(base64Len);
    require(b64string, fail);

    SecBase64Encode(CFDataGetBytePtr(derived), CFDataGetLength(derived), b64string, base64Len);

    base64Data = CFStringCreateWithBytes(SecCFAllocatorZeroize(),
                                         (const UInt8 *)b64string, base64Len,
                                         kCFStringEncodingUTF8, false);
    require(base64Data, fail);

fail:
    if (b64string) {
        cc_clear(base64Len, b64string);
        free(b64string);
    }
    CFReleaseNull(derived);

    return (__bridge NSString *)base64Data;
}

// We should gen salt/iteration - use S2K for kdf for the time being
// Pass back a dictionary of the parms
//
// Need companion call to respond with MRK on the "iforgot" sequence.

NSString *const kSecRVSalt = @"s";
NSString *const kSecRVIterations = @"i";
NSString *const kSecRVProtocol = @"p";
NSString *const kSecRVVerifier = @"v";
NSString *const kSecRVMasterID = @"mkid";

#if !TARGET_OS_BRIDGE

CFStringRef localProtocolSRPGROUP;
CFDataRef (*localAppleIDauthSupportCreateVerifierPtr) (CFStringRef proto,
                                                CFStringRef username,
                                                CFDataRef salt,
                                                CFNumberRef iter,
                                                CFStringRef password,
                                                CFErrorRef *error);

#if !(defined(__i386__) || TARGET_OS_SIMULATOR)
static CFStringRef getdlsymforString(void *framework, const char *symbol) {
    CFStringRef retval = NULL;
    void *tmpptr = dlsym(framework, symbol);
    if(tmpptr) {
        retval = *(CFStringRef*) tmpptr;
    }
    return retval;
}

static bool connectAppleIDFrameworkSymbols(void) {
    static dispatch_once_t onceToken;
    static void* framework = NULL;
    dispatch_once(&onceToken, ^{
        localAppleIDauthSupportCreateVerifierPtr = NULL;
        localProtocolSRPGROUP = NULL;
        framework = dlopen(PATH_FOR_APPLEIDAUTHSUPPORTFRAMEWORK, RTLD_NOW);
        if(framework) {
            localProtocolSRPGROUP = getdlsymforString(framework,
                "kAppleIDAuthSupportProtocolSRPGROUP2048SHA256PBKDF");
            localAppleIDauthSupportCreateVerifierPtr =
                dlsym(framework, "AppleIDAuthSupportCreateVerifier");
        }
    });
    return (framework != NULL && localProtocolSRPGROUP != NULL &&
            localAppleIDauthSupportCreateVerifierPtr != NULL);
}
#endif
#endif

NSDictionary *
SecRKCopyAccountRecoveryVerifier(NSString *recoveryKey,
                                 NSError **error) {

#if defined(__i386__) || TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE
    abort();
    return NULL;
#else
    CFErrorRef localError = NULL;
    CFStringRef username = CFSTR("foo");
    NSDictionary *retval = nil;
    if(!connectAppleIDFrameworkSymbols()) {
        SOSCreateError(kSOSErrorUnsupported, CFSTR("Recovery Key Creation Not Supported on this platform"), NULL, &localError);
        if(error) *error = (__bridge_transfer NSError *) localError;
        return NULL;
    }

    NSData *salt = (__bridge_transfer NSData*) CFDataCreateWithRandomBytes(32);
    NSNumber *iterations = @40000;
    NSString *protocol = (__bridge NSString*) localProtocolSRPGROUP;
    NSData *verifier = (__bridge_transfer NSData*) localAppleIDauthSupportCreateVerifierPtr(
                                    localProtocolSRPGROUP,
                                    username,
                                    (__bridge CFDataRef) salt,
                                    (__bridge CFNumberRef) iterations,
                                    (__bridge CFStringRef) (recoveryKey),
                                    &localError);
    SecRecoveryKey *srk = SecRKCreateRecoveryKey(recoveryKey);
    NSData *masterKeyID = (__bridge_transfer NSData*) SecRKCreateDerivedSecret(
                                    (__bridge CFSecRecoveryKeyRef) srk,
                                    RK_PASSWORD_HKDF_SIZE,
                                    masterkeyIDSalt,
                                    sizeof(masterkeyIDSalt));
    if(verifier && masterKeyID) {
        retval = @{ kSecRVSalt: salt,
                    kSecRVIterations: iterations,
                    kSecRVProtocol: protocol,
                    kSecRVVerifier: verifier,
                    kSecRVMasterID: masterKeyID };
        
    } else {
        if(error && localError) *error = (__bridge NSError *) localError;
    }
    return retval;
#endif

}

// This recreates the key pair using the recovery key string.
static NSData *
RKBackupCreateECKey(SecRecoveryKey *rk, bool returnFullkey)
{
    CFMutableDataRef keyData = NULL;
    CFDataRef derivedSecret = NULL;
    ccec_const_cp_t cp = ccec_cp_256();
    CFDataRef result = NULL;
    int status;

    ccec_full_ctx_decl_cp(cp, fullKey);

    derivedSecret = SecRKCreateDerivedSecret((__bridge CFSecRecoveryKeyRef)rk, RK_BACKUP_HKDF_SIZE,
                                             backupPublicKey, sizeof(backupPublicKey));
    require(derivedSecret, fail);

    status = ccec_generate_key_deterministic(cp,
                                             CFDataGetLength(derivedSecret), CFDataGetBytePtr(derivedSecret),
                                             ccrng(NULL),
                                             CCEC_GENKEY_DETERMINISTIC_COMPACT,
                                             fullKey);
    require_noerr(status, fail);

    size_t space = ccec_compact_export_size(returnFullkey, ccec_ctx_pub(fullKey));
    keyData = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), space);
    require_quiet(keyData, fail);

    ccec_compact_export(returnFullkey, CFDataGetMutableBytePtr(keyData), fullKey);

    CFTransferRetained(result, keyData);
fail:
    CFReleaseNull(derivedSecret);
    CFReleaseNull(keyData);

    return (__bridge NSData *)result;
}

NSData *
SecRKCopyBackupFullKey(SecRecoveryKey *rk)
{
    return RKBackupCreateECKey(rk, true);
}


NSData *
SecRKCopyBackupPublicKey(SecRecoveryKey *rk)
{
    return RKBackupCreateECKey(rk, false);
}

bool
SecRKRegisterBackupPublicKey(SecRecoveryKey *rk, CFErrorRef *error)
{
    CFDataRef backupKey = (__bridge CFDataRef)SecRKCopyBackupPublicKey(rk);
    bool res = false;

    require_action_quiet(backupKey, fail, SOSCreateError(kSOSErrorBadKey, CFSTR("Failed to create key from rk"), NULL, error));

    res = SOSCCRegisterRecoveryPublicKey(backupKey, error);

fail:
    CFReleaseNull(backupKey);

    return res;
}
