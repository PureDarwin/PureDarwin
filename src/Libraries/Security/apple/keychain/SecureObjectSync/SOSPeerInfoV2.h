//
//  SOSPeerInfoV2.h
//  sec
//
//  Created by Richard Murphy on 1/26/15.
//
//

#ifndef _sec_SOSPeerInfoV2_
#define _sec_SOSPeerInfoV2_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSViews.h>

// Description Dictionary Entries Added for V2
extern CFStringRef sV2DictionaryKey;            // CFData wrapper for V2 extensions
extern CFStringRef sViewsKey;                   // Set of Views
extern CFStringRef sSerialNumberKey;            // Device Serial Number
extern CFStringRef sMachineIDKey;               // Account MID
extern CFStringRef kSOSHsaCrKeyDictionary;      // HSA Challenge-Response area
extern CFStringRef sPreferIDS;                    // Whether or not a peer requires to speak over IDS or KVS
extern CFStringRef sPreferIDSFragmentation;     // Whether or not a peer requires to speak over fragmented IDS or not
extern CFStringRef sPreferIDSACKModel;          //If a peer prefers to use the ACK model and not ping

extern CFStringRef sTransportType;              // Dictates the transport type
extern CFStringRef sDeviceID;                   // The IDS device id
extern CFStringRef sRingState;                  // Dictionary of Ring Membership States
extern CFStringRef sBackupKeyKey;
extern CFStringRef sEscrowRecord;

extern CFStringRef sCKKSForAll;

bool SOSPeerInfoUpdateToV2(SOSPeerInfoRef pi, CFErrorRef *error);
void SOSPeerInfoPackV2Data(SOSPeerInfoRef peer);
bool SOSPeerInfoExpandV2Data(SOSPeerInfoRef pi, CFErrorRef *error);
void SOSPeerInfoV2DictionarySetValue(SOSPeerInfoRef peer, const void *key, const void *value);
void SOSPeerInfoV2DictionaryRemoveValue(SOSPeerInfoRef peer, const void *key);

CFMutableDataRef SOSPeerInfoV2DictionaryCopyData(SOSPeerInfoRef pi, const void *key);
CFMutableSetRef SOSPeerInfoV2DictionaryCopySet(SOSPeerInfoRef pi, const void *key);
CFMutableStringRef SOSPeerInfoV2DictionaryCopyString(SOSPeerInfoRef pi, const void *key);
CFBooleanRef SOSPeerInfoV2DictionaryCopyBoolean(SOSPeerInfoRef pi, const void *key);
CFMutableDictionaryRef SOSPeerInfoV2DictionaryCopyDictionary(SOSPeerInfoRef pi, const void *key);
SOSPeerInfoRef SOSPeerInfoCopyWithV2DictionaryUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef newv2dict, SecKeyRef signingKey, CFErrorRef* error);

bool SOSPeerInfoV2DictionaryHasSet(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasData(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasString(SOSPeerInfoRef pi, const void *key);
bool SOSPeerInfoV2DictionaryHasBoolean(SOSPeerInfoRef pi, const void *key);

bool SOSPeerInfoV2DictionaryHasStringValue(SOSPeerInfoRef pi, const void *key, CFStringRef value);

bool SOSPeerInfoV2DictionaryHasSetContaining(SOSPeerInfoRef pi, const void *key, const void* value);
void SOSPeerInfoV2DictionaryForEachSetValue(SOSPeerInfoRef pi, const void *key, void (^action)(const void* value));
void SOSPeerInfoV2DictionaryWithSet(SOSPeerInfoRef pi, const void *key, void(^operation)(CFSetRef set));


bool SOSPeerInfoSerialNumberIsSet(SOSPeerInfoRef pi);
void SOSPeerInfoSetSerialNumber(SOSPeerInfoRef pi);
void SOSPeerInfoSetTestSerialNumber(SOSPeerInfoRef pi, CFStringRef serialNumber);

#endif /* defined(_sec_SOSPeerInfoV2_) */
