//
//  SOSPeerInfoRingState.h
//  sec
//
//  Created by Richard Murphy on 3/6/15.
//
//

#ifndef _sec_SOSPeerInfoRingState_
#define _sec_SOSPeerInfoRingState_

#include <AssertMacros.h>
#include <TargetConditionals.h>

#include "SOSViews.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include "keychain/SecureObjectSync/SOSInternal.h"

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSRingTypes.h"

SOSRingStatus SOSPeerInfoGetRingState(SOSPeerInfoRef pi, CFStringRef ringname, CFErrorRef *error);

#endif /* defined(_sec_SOSPeerInfoRingState_) */
