//
//  SOSPeerInfoDER.h
//  sec
//
//  Created by Richard Murphy on 2/9/15.
//
//

#ifndef _sec_SOSPeerInfoDER_
#define _sec_SOSPeerInfoDER_

#include <corecrypto/ccder.h>
#include <utilities/der_date.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

#include <stdio.h>
//
// DER Import Export
//
SOSPeerInfoRef SOSPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end);

SOSPeerInfoRef SOSPeerInfoCreateFromData(CFAllocatorRef allocator, CFErrorRef* error,
                                         CFDataRef peerinfo_data);

size_t      SOSPeerInfoGetDEREncodedSize(SOSPeerInfoRef peer, CFErrorRef *error);
uint8_t*    SOSPeerInfoEncodeToDER(SOSPeerInfoRef peer, CFErrorRef* error,
                                   const uint8_t* der, uint8_t* der_end);

CFDataRef SOSPeerInfoCopyEncodedData(SOSPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error);

#endif /* defined(_sec_SOSPeerInfoDER_) */
