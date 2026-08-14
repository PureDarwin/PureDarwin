/*
 * Security.framework umbrella header.
 *
 * Apple's own base/Security.h is not shipped: under SEC_OS_OSX_INCLUDES it
 * pulls the entire CDSA header set (cssm*.h, AuthSession.h, emmspi.h), which
 * PureDarwin does not vendor, so including it would fail to compile rather
 * than fail to link. This lists the headers whose implementations are actually
 * present.
 *
 * Everything named here is Apple's real header, from Security-59754.120.12.
 * The one exception is Authorization.h, which is ours: libsecurity_authorization
 * is not vendored, and the functions it declares resolve to the placeholder
 * implementations in compat/pd_security_unvendored.c. The keychain headers are
 * Apple's own even though libsecurity_keychain is likewise not built, so that
 * the placeholders match the real signatures exactly.
 */
#ifndef _PUREDARWIN_SECURITY_H_
#define _PUREDARWIN_SECURITY_H_

#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecAccessControl.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecPolicy.h>
#include <Security/SecRandom.h>
#include <Security/SecImportExport.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTask.h>

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecTrustedApplication.h>

#include <Security/Authorization.h>

#endif /* _PUREDARWIN_SECURITY_H_ */
