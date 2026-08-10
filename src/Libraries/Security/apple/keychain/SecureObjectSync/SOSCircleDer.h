//
//  SOSCircleDer.h
//  sec
//
//  Created by Richard Murphy on 1/22/15.
//
//

#ifndef _sec_SOSCircleDer_
#define _sec_SOSCircleDer_

#include <stdio.h>

size_t der_sizeof_data_or_null(CFDataRef data, CFErrorRef* error);

uint8_t* der_encode_data_or_null(CFDataRef data, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

const uint8_t* der_decode_data_or_null(CFAllocatorRef allocator, CFDataRef* data,
                                       CFErrorRef* error,
                                       const uint8_t* der, const uint8_t* der_end);

#endif /* defined(_sec_SOSCircleDer_) */
