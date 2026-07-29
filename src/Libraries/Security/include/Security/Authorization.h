#ifndef _SECURITY_AUTHORIZATION_H_
#define _SECURITY_AUTHORIZATION_H_

#include <Security/SecBase.h>

__BEGIN_DECLS

typedef const struct AuthorizationOpaqueRef *AuthorizationRef;

#define kAuthorizationExternalFormLength 32

typedef struct {
	char bytes[kAuthorizationExternalFormLength];
} AuthorizationExternalForm;

typedef uint32_t AuthorizationFlags;

enum {
	kAuthorizationFlagDefaults              = 0,
	kAuthorizationFlagInteractionAllowed    = 1 << 0,
	kAuthorizationFlagExtendRights          = 1 << 1,
	kAuthorizationFlagPartialRights         = 1 << 2,
	kAuthorizationFlagDestroyRights         = 1 << 3,
	kAuthorizationFlagPreAuthorize          = 1 << 4,
	kAuthorizationFlagNoData                = 1 << 20,
};

enum {
	errAuthorizationSuccess                 = 0,
	errAuthorizationInvalidSet              = -60001,
	errAuthorizationInvalidRef              = -60002,
	errAuthorizationInvalidTag              = -60003,
	errAuthorizationInvalidPointer          = -60004,
	errAuthorizationDenied                  = -60005,
	errAuthorizationCanceled                = -60006,
	errAuthorizationInteractionNotAllowed   = -60007,
	errAuthorizationInternal                = -60008,
	errAuthorizationExternalizeNotAllowed   = -60009,
	errAuthorizationInternalizeNotAllowed   = -60010,
	errAuthorizationInvalidFlags            = -60011,
	errAuthorizationToolExecuteFailure      = -60031,
	errAuthorizationToolEnvironmentError    = -60032,
	errAuthorizationBadAddress              = -60033,
};

typedef struct {
	const char *name;
	size_t      valueLength;
	void       *value;
	uint32_t    flags;
} AuthorizationItem;

typedef struct {
	uint32_t           count;
	AuthorizationItem *items;
} AuthorizationItemSet;

typedef AuthorizationItemSet AuthorizationRights;
typedef AuthorizationItemSet AuthorizationEnvironment;

OSStatus AuthorizationCreate(const AuthorizationRights *rights,
			     const AuthorizationEnvironment *environment,
			     AuthorizationFlags flags,
			     AuthorizationRef *authorization);

OSStatus AuthorizationFree(AuthorizationRef authorization,
			   AuthorizationFlags flags);

OSStatus AuthorizationCopyRights(AuthorizationRef authorization,
				 const AuthorizationRights *rights,
				 const AuthorizationEnvironment *environment,
				 AuthorizationFlags flags,
				 AuthorizationRights **authorizedRights);

OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
				       AuthorizationExternalForm *extForm);

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
					     AuthorizationRef *authorization);

__END_DECLS

#endif /* _SECURITY_AUTHORIZATION_H_ */
