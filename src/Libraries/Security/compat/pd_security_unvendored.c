#include <Security/Security.h>
#include <Security/SecTask.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* SecImportExport.h declares these only under SEC_OS_OSX, and the sec core is
 * built as the iOS flavour, so they are absent there - but SecItemExport still
 * needs a signature. */
#if !defined(SEC_OS_OSX) || !SEC_OS_OSX
typedef uint32_t SecExternalFormat;
typedef uint32_t SecItemImportExportFlags;
#endif

struct AuthorizationOpaqueRef { int unused; };
static const struct AuthorizationOpaqueRef _pd_authorization_token;

OSStatus
AuthorizationCreate(const AuthorizationRights *rights,
                    const AuthorizationEnvironment *environment,
                    AuthorizationFlags flags, AuthorizationRef *authorization)
{
    (void) rights;
    (void) environment;
    (void) flags;
    if ( authorization )  *authorization = &_pd_authorization_token;
    return errAuthorizationSuccess;
}

OSStatus
AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags)
{
    (void) authorization;
    (void) flags;
    return errAuthorizationSuccess;
}

OSStatus
AuthorizationCopyRights(AuthorizationRef authorization,
                        const AuthorizationRights *rights,
                        const AuthorizationEnvironment *environment,
                        AuthorizationFlags flags,
                        AuthorizationRights **authorizedRights)
{
    (void) authorization;
    (void) rights;
    (void) environment;
    (void) flags;
    if ( authorizedRights )  *authorizedRights = NULL;
    return ( geteuid() == 0 ) ? errAuthorizationSuccess : errAuthorizationDenied;
}

OSStatus
AuthorizationMakeExternalForm(AuthorizationRef authorization,
                              AuthorizationExternalForm *extForm)
{
    (void) authorization;
    if ( extForm == NULL )  return errAuthorizationInvalidPointer;
    memset(extForm, 0, sizeof(*extForm));
    return errAuthorizationSuccess;
}

OSStatus
AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
                                    AuthorizationRef *authorization)
{
    (void) extForm;
    if ( authorization )  *authorization = &_pd_authorization_token;
    return errAuthorizationSuccess;
}

OSStatus
SecKeychainAddGenericPassword(SecKeychainRef keychain,
                              UInt32 serviceNameLength, const char *serviceName,
                              UInt32 accountNameLength, const char *accountName,
                              UInt32 passwordLength, const void *passwordData,
                              SecKeychainItemRef *itemRef)
{
    (void) keychain;
    (void) serviceNameLength; (void) serviceName;
    (void) accountNameLength; (void) accountName;
    (void) passwordLength; (void) passwordData;
    if ( itemRef )  *itemRef = NULL;
    return errSecNotAvailable;
}

OSStatus
SecKeychainFindGenericPassword(CFTypeRef keychainOrArray,
                               UInt32 serviceNameLength, const char *serviceName,
                               UInt32 accountNameLength, const char *accountName,
                               UInt32 *passwordLength, void **passwordData,
                               SecKeychainItemRef *itemRef)
{
    (void) keychainOrArray;
    (void) serviceNameLength; (void) serviceName;
    (void) accountNameLength; (void) accountName;
    if ( passwordLength )  *passwordLength = 0;
    if ( passwordData )    *passwordData = NULL;
    if ( itemRef )         *itemRef = NULL;
    return errSecItemNotFound;
}

OSStatus
SecKeychainGetUserInteractionAllowed(Boolean *state)
{
    if ( state )  *state = false;
    return errSecSuccess;
}

OSStatus
SecKeychainSetUserInteractionAllowed(Boolean state)
{
    (void) state;
    return errSecSuccess;
}

OSStatus
SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef,
                                     SecKeychainAttributeInfo *info,
                                     SecItemClass *itemClass,
                                     SecKeychainAttributeList **attrList,
                                     UInt32 *length, void **outData)
{
    (void) itemRef;
    (void) info;
    if ( itemClass )  *itemClass = 0;
    if ( attrList )   *attrList = NULL;
    if ( length )     *length = 0;
    if ( outData )    *outData = NULL;
    return errSecItemNotFound;
}

OSStatus
SecKeychainItemDelete(SecKeychainItemRef itemRef)
{
    (void) itemRef;
    return errSecItemNotFound;
}

OSStatus
SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList,
                                     void *data)
{
    (void) attrList;
    (void) data;
    return errSecSuccess;
}

OSStatus
SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef,
                                       const SecKeychainAttributeList *attrList,
                                       UInt32 length, const void *data)
{
    (void) itemRef;
    (void) attrList;
    (void) length;
    (void) data;
    return errSecItemNotFound;
}

OSStatus
SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef,
                          SecKeychainItemRef *itemRef)
{
    (void) searchRef;
    if ( itemRef )  *itemRef = NULL;
    return errSecItemNotFound;
}

OSStatus
SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray,
                                      SecItemClass itemClass,
                                      const SecKeychainAttributeList *attrList,
                                      SecKeychainSearchRef *searchRef)
{
    (void) keychainOrArray;
    (void) itemClass;
    (void) attrList;
    if ( searchRef )  *searchRef = NULL;
    return errSecItemNotFound;
}

OSStatus
SecTrustSettingsCopyCertificates(SecTrustSettingsDomain domain,
                                 CFArrayRef *certArray)
{
    (void) domain;
    if ( certArray )  *certArray = NULL;
    return errSecNoTrustSettings;
}

OSStatus
SecItemExport(CFTypeRef secItemOrArray, SecExternalFormat outputFormat,
              SecItemImportExportFlags flags, const void *keyParams,
              CFDataRef *exportedData)
{
    (void) secItemOrArray;
    (void) outputFormat;
    (void) flags;
    (void) keyParams;
    if ( exportedData )  *exportedData = NULL;
    return errSecUnimplemented;
}

/*
 * simulate_crash.m soft-links CrashReporterSupport, which PureDarwin does not
 * have. Security calls this to file a non-fatal crash report for a state it
 * can recover from, so doing nothing loses only telemetry.
 */
void __security_stackshotreport(const char *reason, int32_t code);
void __security_stackshotreport(const char *reason, int32_t code)
{
    (void)reason;
    (void)code;
}

/*
 * On macOS SecRandomCopyBytes lives in libsecurity_keychain - the vendored
 * sec core compiles its copy out under #if !TARGET_OS_OSX. arc4random_buf is
 * the same CSPRNG the framework would reach.
 */
extern void arc4random_buf(void *buf, size_t nbytes);

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, void *bytes)
{
    (void)rnd;
    arc4random_buf(bytes, count);
    return errSecSuccess;
}

/*
 * simulate_crash.m soft-links CrashReporterSupport, which PureDarwin does not
 * have; these two record diagnostics for recoverable states.
 */
bool __security_simulatecrash_enabled(void);
bool __security_simulatecrash_enabled(void)
{
    return false;
}

void __security_simulatecrash(CFStringRef reason, uint32_t code);
void __security_simulatecrash(CFStringRef reason, uint32_t code)
{
    (void)reason;
    (void)code;
}

/*
 * vproc_swap_string is launchd SPI that securityd's client uses to read the
 * session's bootstrap name. PureDarwin's launchd does not export it; reporting
 * failure makes the client fall back to the system context.
 */
void *vproc_swap_string(void *vp, int64_t key, const char *inval, char **outval);
void *vproc_swap_string(void *vp, int64_t key, const char *inval, char **outval)
{
    (void)vp;
    (void)key;
    (void)inval;
    if (outval != NULL)
        *outval = NULL;
    return (void *)-1;
}

/*
 * On macOS this lives in libsecurity_keychain's SecBase.cpp, where it maps a
 * few hundred OSStatus values to localised strings out of the framework's
 * bundle. PureDarwin has no such bundle, so callers get the numeric form -
 * which is what the real one falls back to for an unrecognised status anyway.
 */
CFStringRef SecCopyErrorMessageString(OSStatus status, void *reserved)
{
    (void)reserved;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                    CFSTR("OSStatus %d"), (int)status);
}

/*
 * SecureObjectSync is not vendored, but SecCFError.c annotates every error it
 * builds with these two keys. Values match Apple's SOSInternal.m so an error
 * dictionary produced here reads the same as one from a full Security.
 */
const CFStringRef kSOSErrorDomain = CFSTR("com.apple.security.sos.error");
const CFStringRef kSOSCountKey = CFSTR("numberOfErrorsDeep");
