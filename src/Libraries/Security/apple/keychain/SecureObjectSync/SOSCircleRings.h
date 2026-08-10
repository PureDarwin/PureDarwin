//
//  SOSCircleRings.h
//  sec
//
//  Created by Richard Murphy on 12/4/14.
//
//

#ifndef sec_SOSCircleRings_h
#define sec_SOSCircleRings_h

/* return the ring recorded within the circle */
CFMutableSetRef SOSCircleGetRing(SOSCircleRef circle, CFStringRef ring);

/* return a set of peers referenced by a ring within the circle */
CFMutableSetRef SOSCircleRingCopyPeers(SOSCircleRef circle, CFStringRef ring, CFAllocatorRef allocator);

/* return the number of peers represented within a ring */
int SOSCircleRingCountPeers(SOSCircleRef circle, CFStringRef ring);

/* For each Peer in the circle, evaluate the ones purported to be allowed within a ring and sign them in to the ring */
bool SOSCircleRingAddPeers(SOSCircleRef oldCircle, SOSCircleRef newCircle, CFStringRef ring);



__END_DECLS

#endif

#endif
