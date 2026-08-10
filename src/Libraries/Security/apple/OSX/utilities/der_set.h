//
//  der_set.h
//  utilities
//
//  Created by Richard Murphy on 1/22/15.
//  Copyright © 2015 Apple Inc. All rights reserved.
//

#ifndef _utilities_der_set_
#define _utilities_der_set_

#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <corecrypto/ccder.h>

// If you provide a set in *set, we will add elements to it and return the union.
const uint8_t* der_decode_set(CFAllocatorRef allocator,
                              CFSetRef* set, CFErrorRef *error,
                              const uint8_t* der, const uint8_t *der_end);

size_t der_sizeof_set(CFSetRef dict, CFErrorRef *error);

uint8_t* der_encode_set(CFSetRef set, CFErrorRef *error,
                        const uint8_t *der, uint8_t *der_end);


#endif /* defined(_utilities_der_set_) */
