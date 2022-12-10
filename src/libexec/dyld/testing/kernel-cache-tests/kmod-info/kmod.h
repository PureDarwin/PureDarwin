
#include <stdint.h>

typedef uint64_t  vm_address_t;
typedef uint64_t  vm_size_t;

// Taken from kmod.h
#define KMOD_MAX_NAME 64

#pragma pack(push, 4)
typedef struct kmod_info {
    struct kmod_info  * next;
    int32_t             info_version;       // version of this structure
    uint32_t            id;
    char                name[KMOD_MAX_NAME];
    char                version[KMOD_MAX_NAME];
    int32_t             reference_count;    // # linkage refs to this
    void  *       reference_list;     // who this refs (links on)
    vm_address_t        address;            // starting address
    vm_size_t           size;               // total size
    vm_size_t           hdr_size;           // unwired hdr size
    void *        start;
    void *        stop;
} kmod_info_t;
#pragma pack(pop)

#define KMOD_INFO_NAME       kmod_info
#define KMOD_INFO_VERSION    1
#define KMOD_DECL(name, version)                                  \
    static kmod_start_func_t name ## _module_start;               \
    static kmod_stop_func_t  name ## _module_stop;                \
    kmod_info_t KMOD_INFO_NAME = { 0, KMOD_INFO_VERSION, -1U,      \
                   { #name }, { version }, -1, 0, 0, 0, 0,    \
                       name ## _module_start,                 \
                       name ## _module_stop };
#define KMOD_EXPLICIT_DECL(name, version, start, stop)            \
    kmod_info_t KMOD_INFO_NAME = { 0, KMOD_INFO_VERSION, -1U,      \
                   { #name }, { version }, -1, 0, 0, 0, 0,    \
                       start, stop };
