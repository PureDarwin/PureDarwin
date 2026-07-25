#include <Security/Security.h>
#include <errno.h>
#include <string.h>

extern void arc4random_buf(void * buf, size_t nbytes);

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, void * bytes)
{
    (void) rnd;
    arc4random_buf(bytes, count);
    return errSecSuccess;
}

struct __SecRandom { int unused; };
static const struct __SecRandom _kSecRandomDefaultStorage;
const SecRandomRef kSecRandomDefault = &_kSecRandomDefaultStorage;

const CFStringRef kSecClass                 = CFSTR("class");
const CFStringRef kSecClassGenericPassword   = CFSTR("genp");
const CFStringRef kSecClassInternetPassword  = CFSTR("inet");
const CFStringRef kSecAttrAccount            = CFSTR("acct");
const CFStringRef kSecAttrService            = CFSTR("svce");
const CFStringRef kSecAttrServer             = CFSTR("srvr");
const CFStringRef kSecAttrLabel              = CFSTR("labl");
const CFStringRef kSecValueData              = CFSTR("v_Data");
const CFStringRef kSecReturnData             = CFSTR("r_Data");
const CFStringRef kSecReturnAttributes       = CFSTR("r_Attributes");
const CFStringRef kSecMatchLimit             = CFSTR("m_Limit");
const CFStringRef kSecMatchLimitOne          = CFSTR("m_LimitOne");
const CFStringRef kSecMatchLimitAll          = CFSTR("m_LimitAll");

OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef * result)
{
    (void) query;
    if ( result )  *result = NULL;
    return errSecItemNotFound;
}

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef * result)
{
    (void) attributes;
    if ( result )  *result = NULL;
    return errSecUnimplemented;
}

OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    (void) query;
    (void) attributesToUpdate;
    return errSecItemNotFound;
}

OSStatus SecItemDelete(CFDictionaryRef query)
{
    (void) query;
    return errSecItemNotFound;
}
