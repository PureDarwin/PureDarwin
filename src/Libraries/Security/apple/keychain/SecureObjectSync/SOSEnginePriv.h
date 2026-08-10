//
//  SOSEnginePriv.h
//  sec
//
//

#ifndef SOSEnginePriv_h
#define SOSEnginePriv_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include "keychain/SecureObjectSync/SOSEngine.h"

/* SOSEngine implementation. */
struct __OpaqueSOSEngine {
    CFRuntimeBase _base;
    SOSDataSourceRef dataSource;
    CFStringRef myID;                       // My peerID in the circle
    // We need to address the issues of corrupt keychain items
    SOSManifestRef unreadable;              // Possibly by having a set of unreadable items, to which we
    // add any corrupted items in the db that have yet to be deleted.
    // This happens if we notce corruption during a (read only) query.
    // We would also perma-subtract unreadable from manifest whenever
    // anyone asked for manifest.  This result would be cached in
    // The manifestCache below, so we just need a key into the cache
    CFDataRef localMinusUnreadableDigest;   // or a digest (CFDataRef of the right size).

    CFMutableDictionaryRef manifestCache;       // digest -> ( refcount, manifest )
    CFMutableDictionaryRef peerMap;             // peerId -> SOSPeerRef
    CFDictionaryRef viewNameSet2ChangeTracker;  // CFSetRef of CFStringRef -> SOSChangeTrackerRef
    CFDictionaryRef viewName2ChangeTracker;     // CFStringRef -> SOSChangeTrackerRef
    CFArrayRef peerIDs;
    CFDateRef lastTraceDate;                    // Last time we did a CloudKeychainTrace
    CFMutableDictionaryRef coders;
    bool haveLoadedCoders;

    bool codersNeedSaving;
    dispatch_queue_t queue;                     // Engine queue

    dispatch_source_t save_timer;               // Engine state save timer
    dispatch_queue_t syncCompleteQueue;              // Non-retained queue for async notificaion
    SOSEnginePeerInSyncBlock syncCompleteListener;   // Block to call to notify the listener.

    bool save_timer_pending;                    // Engine state timer running, read/modify on engine queue

};

#endif /* SOSEnginePriv_h */
