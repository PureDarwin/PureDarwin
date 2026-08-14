/*
 * No CryptoTokenKit on PureDarwin: there is no token daemon to talk to, so
 * every token lookup fails with kTKErrorCodeTokenNotFound. Security treats that
 * as "this item does not live on a token" and uses the keychain database.
 */
#include <ctkclient/ctkclient.h>

static void pd_no_token(CFErrorRef *error, CFIndex code)
{
    if (error == NULL || *error != NULL)
        return;
    *error = CFErrorCreate(kCFAllocatorDefault, CFSTR(kTKErrorDomain), code, NULL);
}

TKTokenRef TKTokenCreate(CFDictionaryRef attributes, CFErrorRef *error)
{
    (void)attributes;
    pd_no_token(error, kTKErrorCodeTokenNotFound);
    return NULL;
}

CFDataRef TKTokenCopyObjectData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    (void)token; (void)objectID;
    pd_no_token(error, kTKErrorCodeObjectNotFound);
    return NULL;
}

CFDataRef TKTokenCopyObjectAccessControl(TKTokenRef token, CFTypeRef objectOrAttributes, CFErrorRef *error)
{
    (void)token; (void)objectOrAttributes;
    pd_no_token(error, kTKErrorCodeObjectNotFound);
    return NULL;
}

CFDataRef TKTokenCopyPublicKeyData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    (void)token; (void)objectID;
    pd_no_token(error, kTKErrorCodeObjectNotFound);
    return NULL;
}

CFDataRef TKTokenCreateOrUpdateObject(TKTokenRef token, CFDataRef objectID,
                                      CFMutableDictionaryRef attributes, CFErrorRef *error)
{
    (void)token; (void)objectID; (void)attributes;
    pd_no_token(error, kTKErrorCodeTokenNotFound);
    return NULL;
}

bool TKTokenDeleteObject(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    (void)token; (void)objectID;
    pd_no_token(error, kTKErrorCodeObjectNotFound);
    return false;
}

CFTypeRef TKTokenCopyOperationResult(TKTokenRef token, CFDataRef objectID, CFIndex operation,
                                     CFArrayRef algorithms, CFIndex mode,
                                     CFTypeRef in1, CFTypeRef in2, CFErrorRef *error)
{
    (void)token; (void)objectID; (void)operation; (void)algorithms;
    (void)mode; (void)in1; (void)in2;
    pd_no_token(error, kTKErrorCodeNotImplemented);
    return NULL;
}

CFDictionaryRef TKTokenControl(TKTokenRef token, CFDictionaryRef attributes, CFErrorRef *error)
{
    (void)token; (void)attributes;
    pd_no_token(error, kTKErrorCodeNotImplemented);
    return NULL;
}
