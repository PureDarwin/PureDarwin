//
//  SecBuffer.c
//  utilities
//
//  Created by Mitch Adler on 3/6/15.
//  Copyright © 2015 Apple Inc. All rights reserved.
//

#include <utilities/SecBuffer.h>

#include <strings.h>

#define stackBufferSizeLimit 2048

void PerformWithBuffer(size_t size, void (^operation)(size_t size, uint8_t *buffer)) {
    if (size == 0) {
        operation(0, NULL);
    } else if (size <= stackBufferSizeLimit) {
        uint8_t buffer[size];
        operation(size, buffer);
    } else {
        uint8_t *buffer = malloc(size);
        
        operation(size, buffer);
        
        if (buffer)
            free(buffer);
    }
}

void PerformWithBufferAndClear(size_t size, void (^operation)(size_t size, uint8_t *buffer)) {
    PerformWithBuffer(size, ^(size_t buf_size, uint8_t *buffer) {
        operation(buf_size, buffer);
        
        bzero(buffer, buf_size);
    });
}
