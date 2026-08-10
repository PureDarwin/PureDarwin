//
//  SOSPeerOTRTimer.h
//

#ifndef SOSPeerOTRTimer_h
#define SOSPeerOTRTimer_h

void SOSPeerOTRTimerFired(SOSAccount* account, SOSPeerRef peer, SOSEngineRef engine, SOSCoderRef coder);
int SOSPeerOTRTimerTimeoutValue(SOSAccount* account, SOSPeerRef peer);
void SOSPeerOTRTimerSetupAwaitingTimer(SOSAccount* account, SOSPeerRef peer, SOSEngineRef engine, SOSCoderRef coder);


//functions to handle max retry counter
void SOSPeerOTRTimerIncreaseOTRNegotiationRetryCount(SOSAccount* account, NSString* peerid);
bool SOSPeerOTRTimerHaveReachedMaxRetryAllowance(SOSAccount* account, NSString* peerid);
void SOSPeerOTRTimerClearMaxRetryCount(SOSAccount* account, NSString* peerid);
bool SOSPeerOTRTimerHaveAnRTTAvailable(SOSAccount* account, NSString* peerid);
void SOSPeerOTRTimerRemoveRTTTimeoutForPeer(SOSAccount* account, NSString* peerid);
void SOSPeerOTRTimerRemoveLastSentMessageTimestamp(SOSAccount* account, NSString* peerid);

#endif /* SOSPeerOTRTimer_h */
