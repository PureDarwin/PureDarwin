//
//  depinfo.h
//  cctools libstuff
//
//  Created by Michael Trent on 9/9/19.
//

#ifndef depinfo_h
#define depinfo_h

#include <stdint.h>

enum : uint8_t {
    DEPINFO_TOOL          = 0x00,
    DEPINFO_INPUT_FOUND   = 0x10,
    DEPINFO_INPUT_MISSING = 0x11,
    DEPINFO_OUTPUT        = 0x40,
};

enum : uint32_t {
    DI_READ_NONE          = 0,
    DI_READ_LOG           = (1 << 0),
    DI_READ_NORETVAL      = (1 << 1),
};

struct depinfo;

struct depinfo* depinfo_alloc(void);

void depinfo_free(struct depinfo* depinfo);
void depinfo_add(struct depinfo* depinfo, uint8_t opcode, const char* string);
int  depinfo_count(struct depinfo* depinfo);
int  depinfo_get(struct depinfo* depinfo, int index, uint8_t* opcode,
                const char** string);
void depinfo_sort(struct depinfo* depinfo);

struct depinfo* depinfo_read(const char* path, uint32_t flags);
int  depinfo_write(struct depinfo* depinfo, const char* path);

#endif /* depinfo_h */
