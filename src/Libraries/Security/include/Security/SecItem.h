#ifndef _PD_SECITEM_H
#define _PD_SECITEM_H

#include <Security/SecBase.h>

CF_ASSUME_NONNULL_BEGIN

CF_EXPORT const CFStringRef kSecClass;
CF_EXPORT const CFStringRef kSecClassGenericPassword;
CF_EXPORT const CFStringRef kSecClassInternetPassword;
CF_EXPORT const CFStringRef kSecAttrAccount;
CF_EXPORT const CFStringRef kSecAttrService;
CF_EXPORT const CFStringRef kSecAttrServer;
CF_EXPORT const CFStringRef kSecAttrLabel;
CF_EXPORT const CFStringRef kSecValueData;
CF_EXPORT const CFStringRef kSecReturnData;
CF_EXPORT const CFStringRef kSecReturnAttributes;
CF_EXPORT const CFStringRef kSecMatchLimit;
CF_EXPORT const CFStringRef kSecMatchLimitOne;
CF_EXPORT const CFStringRef kSecMatchLimitAll;

OSStatus SecItemCopyMatching(CFDictionaryRef query,
                              CFTypeRef __nullable * __nullable result);
OSStatus SecItemAdd(CFDictionaryRef attributes,
                     CFTypeRef __nullable * __nullable result);
OSStatus SecItemUpdate(CFDictionaryRef query,
                        CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete(CFDictionaryRef query);

CF_ASSUME_NONNULL_END

#endif /* _PD_SECITEM_H */
