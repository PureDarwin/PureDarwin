//
//  SOSGenCount.h
//  sec
//
//  Created by Richard Murphy on 1/29/15.
//
//

#ifndef _sec_SOSGenCount_
#define _sec_SOSGenCount_

#include <CoreFoundation/CoreFoundation.h>

typedef CFNumberRef SOSGenCountRef;

int64_t SOSGetGenerationSint(SOSGenCountRef gen);
SOSGenCountRef SOSGenerationCreate(void);
SOSGenCountRef SOSGenerationCreateWithValue(int64_t value);
SOSGenCountRef SOSGenerationIncrementAndCreate(SOSGenCountRef gen);
SOSGenCountRef SOSGenerationCopy(SOSGenCountRef gen);
bool SOSGenerationIsOlder(SOSGenCountRef current, SOSGenCountRef proposed);
SOSGenCountRef SOSGenerationCreateWithBaseline(SOSGenCountRef reference);

SOSGenCountRef SOSGenCountCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end);
size_t SOSGenCountGetDEREncodedSize(SOSGenCountRef gencount, CFErrorRef *error);
uint8_t *SOSGenCountEncodeToDER(SOSGenCountRef gencount, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

void SOSGenerationCountWithDescription(SOSGenCountRef gen, void (^operation)(CFStringRef description));
CFStringRef SOSGenerationCountCopyDescription(SOSGenCountRef gen);


#endif /* defined(_sec_SOSGenCount_) */
