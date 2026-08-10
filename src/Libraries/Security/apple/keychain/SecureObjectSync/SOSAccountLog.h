//
//  SOSAccountLog.h
//  sec
//
//  Created by Richard Murphy on 6/1/16.
//
//

#ifndef SOSAccountLog_h
#define SOSAccountLog_h

#include <stdio.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSPeerInfoDER.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"

//#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
@class SOSAccount;

void SOSAccountLog(SOSAccount* account);
SOSAccount* SOSAccountCreateFromStringRef(CFStringRef hexString);

#endif /* SOSAccountLog_h */
