//
//  SOSAccountGhost.h
//  sec
//
//
//

#ifndef SOSAccountGhost_h
#define SOSAccountGhost_h

#include "SOSAccount.h"
#include "keychain/SecureObjectSync/SOSTypes.h"

#define GHOSTBUST_PERIODIC 1

bool SOSAccountGhostResultsInReset(SOSAccount* account);
CF_RETURNS_RETAINED SOSCircleRef SOSAccountCloneCircleWithoutMyGhosts(SOSAccount* account, SOSCircleRef startCircle);

#if __OBJC__
@class SOSAuthKitHelpers;

/*
 * Ghostbust devices that are not in circle
 *
 * @param account account to operate on
 * @param akh injection of parameters
 * @param mincount if circle is smaller the
 *
 * @return true if there was a device busted
 */

bool SOSAccountGhostBustCircle(SOSAccount *account, SOSAuthKitHelpers *akh, SOSAccountGhostBustingOptions options, int mincount);

#endif

#endif /* SOSAccountGhost_h */
