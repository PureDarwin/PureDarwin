#ifndef _PUREDARWIN_SECCERTIFICATEPRIV_H_
#define _PUREDARWIN_SECCERTIFICATEPRIV_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct OpaqueSecCertificateRef        *SecCertificateRef;
typedef struct OpaqueSecTrustedApplicationRef *SecTrustedApplicationRef;

typedef uint32_t SecItemClass;
typedef uint32_t SecKeychainAttrType;
typedef uint32_t SecPreferencesDomain;
typedef uint32_t SecAccessOwnerType;

typedef struct {
	SecKeychainAttrType  tag;
	uint32_t             length;
	void                *data;
} SecKeychainAttribute;

typedef SecKeychainAttribute *SecKeychainAttributePtr;

typedef struct {
	uint32_t              count;
	SecKeychainAttribute *attr;
} SecKeychainAttributeList;

enum {
	kSecGenericPasswordItemClass  = 'genp',
	kSecInternetPasswordItemClass = 'inet',
	kSecCertificateItemClass      = 0x80001000,
};

enum {
	kSecDescriptionItemAttr = 'desc',
	kSecAccountItemAttr     = 'acct',
	kSecServiceItemAttr     = 'svce',
	kSecLabelItemAttr       = 'labl',
};

enum {
	kSecPreferencesDomainUser   = 0,
	kSecPreferencesDomainSystem = 1,
	kSecPreferencesDomainCommon = 2,
	kSecPreferencesDomainDynamic = 3,
};

/* SecAccessCreateWithOwnerAndACL owner types. */
enum {
	kSecUseOnlyUID = 1,
	kSecUseOnlyGID = 2,
	kSecHonorRoot  = 0x100,
	kSecMatchBits  = (kSecUseOnlyUID | kSecUseOnlyGID),
};

OSStatus SecKeychainOpen(const char *pathName, SecKeychainRef *keychain);

OSStatus SecKeychainCopyDomainDefault(SecPreferencesDomain domain,
				      SecKeychainRef *keychain);

OSStatus SecKeychainSetDomainDefault(SecPreferencesDomain domain,
				     SecKeychainRef keychain);

OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef,
				    SecItemClass *itemClass,
				    SecKeychainAttributeList *attrList,
				    UInt32 *length, void **outData);

OSStatus SecKeychainItemCreateFromContent(SecItemClass itemClass,
					  SecKeychainAttributeList *attrList,
					  UInt32 length, const void *data,
					  SecKeychainRef keychainRef,
					  SecAccessRef initialAccess,
					  SecKeychainItemRef *itemRef);

OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef);

OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList,
				    void *data);

OSStatus SecKeychainItemModifyContent(SecKeychainItemRef itemRef,
				      const SecKeychainAttributeList *attrList,
				      UInt32 length, const void *data);

OSStatus SecAccessCreate(CFStringRef descriptor, CFArrayRef trustedlist,
			 SecAccessRef *accessRef);

OSStatus SecAccessCreateWithOwnerAndACL(uid_t userId, gid_t groupId,
					SecAccessOwnerType ownerType,
					CFArrayRef acls,
					CFErrorRef *error);

OSStatus SecTrustedApplicationCreateFromPath(const char *path,
					     SecTrustedApplicationRef *app);

SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator,
					       CFDataRef data);

#endif /* _PUREDARWIN_SECCERTIFICATEPRIV_H_ */
