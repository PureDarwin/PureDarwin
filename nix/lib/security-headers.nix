{
  # Directories published wholesale (*.h).
  dirs = [
    "apple/trust/headers"
    "apple/keychain/headers"
    "include/Security"
  ];

  # Apple's own API headers. base/Security.h is deliberately absent: its
  # SEC_OS_OSX_INCLUDES branch pulls the whole CDSA header set, which is not
  # vendored - include/Security/Security.h is the umbrella over what exists.
  apiFiles = [
    "apple/base/SecBase.h"
    "apple/base/SecBasePriv.h"
    "apple/base/SecRandom.h"
    "apple/cssm/certextensions.h"
    "apple/sectask/SecTask.h"
    "apple/sectask/SecTaskPriv.h"
    "apple/sectask/SecEntitlements.h"
  ];

  # Referenced from the public headers' #if SEC_OS_OSX blocks, which a consumer
  # takes because it does not define SEC_IOS_ON_OSX the way this build does.
  # Declarations only - the CDSA, code-signing and CMS implementations are not
  # vendored, so calling them fails at link time, which is the honest outcome.
  # This is the include closure of the set above; keep it that way.
  closureFiles = [
    "apple/OSX/libsecurity_cssm/lib/cssmconfig.h"
    "apple/OSX/libsecurity_cssm/lib/cssmtype.h"
    "apple/OSX/libsecurity_cssm/lib/cssmerr.h"
    "apple/OSX/libsecurity_cssm/lib/x509defs.h"
    "apple/cssm/cssmapple.h"
    "apple/OSX/libsecurity_codesigning/lib/CSCommon.h"
    "apple/OSX/libsecurity_codesigning/lib/SecCode.h"
    "apple/OSX/libsecurity_keychain/lib/SecAccess.h"
    "apple/OSX/libsecurity_asn1/lib/SecAsn1Types.h"
    "apple/CMS/SecCMS.h"
    "apple/OSX/libsecurity_keychain/lib/SecKeychain.h"
    "apple/OSX/libsecurity_keychain/lib/SecKeychainItem.h"
    "apple/OSX/libsecurity_keychain/lib/SecTrustedApplication.h"
  ];
}
