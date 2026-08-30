/* iig-lite generated from IOService.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOService.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOService.h"

/* @iig implementation */
#include <DriverKit/IODispatchQueue.h>
#include <DriverKit/IOUserClient.h>
#include <DriverKit/IOServiceStateNotificationDispatchSource.h>
/* @iig end */

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

struct IOService_Start_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
};
#pragma pack(4)
struct IOService_Start_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    IOService_Start_Msg_Content content;
};
#pragma pack()
#define IOService_Start_Msg_ObjRefs (2)

struct IOService_Start_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_Start_Rpl
{
    IORPCMessageMach           mach;
    IOService_Start_Rpl_Content content;
};
#pragma pack()
#define IOService_Start_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Start_Msg * message;
        struct IOService_Start_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_Start_Invocation;
struct IOService_Stop_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
};
#pragma pack(4)
struct IOService_Stop_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    IOService_Stop_Msg_Content content;
};
#pragma pack()
#define IOService_Stop_Msg_ObjRefs (2)

struct IOService_Stop_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_Stop_Rpl
{
    IORPCMessageMach           mach;
    IOService_Stop_Rpl_Content content;
};
#pragma pack()
#define IOService_Stop_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Stop_Msg * message;
        struct IOService_Stop_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_Stop_Invocation;
struct IOService_ClientCrashed_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  client;
    uint64_t  options;
};
#pragma pack(4)
struct IOService_ClientCrashed_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t client__descriptor;
    IOService_ClientCrashed_Msg_Content content;
};
#pragma pack()
#define IOService_ClientCrashed_Msg_ObjRefs (2)

struct IOService_ClientCrashed_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_ClientCrashed_Rpl
{
    IORPCMessageMach           mach;
    IOService_ClientCrashed_Rpl_Content content;
};
#pragma pack()
#define IOService_ClientCrashed_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_ClientCrashed_Msg * message;
        struct IOService_ClientCrashed_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_ClientCrashed_Invocation;
struct IOService_GetRegistryEntryID_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_GetRegistryEntryID_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_GetRegistryEntryID_Msg_Content content;
};
#pragma pack()
#define IOService_GetRegistryEntryID_Msg_ObjRefs (1)

struct IOService_GetRegistryEntryID_Rpl_Content
{
    IORPCMessage __hdr;
    uint64_t  registryEntryID;
};
#pragma pack(4)
struct IOService_GetRegistryEntryID_Rpl
{
    IORPCMessageMach           mach;
    IOService_GetRegistryEntryID_Rpl_Content content;
};
#pragma pack()
#define IOService_GetRegistryEntryID_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_GetRegistryEntryID_Msg * message;
        struct IOService_GetRegistryEntryID_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_GetRegistryEntryID_Invocation;
struct IOService_SetName_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[128];
};
#pragma pack(4)
struct IOService_SetName_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_SetName_Msg_Content content;
};
#pragma pack()
#define IOService_SetName_Msg_ObjRefs (1)

struct IOService_SetName_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SetName_Rpl
{
    IORPCMessageMach           mach;
    IOService_SetName_Rpl_Content content;
};
#pragma pack()
#define IOService_SetName_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SetName_Msg * message;
        struct IOService_SetName_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SetName_Invocation;
struct IOService_RegisterService_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_RegisterService_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_RegisterService_Msg_Content content;
};
#pragma pack()
#define IOService_RegisterService_Msg_ObjRefs (1)

struct IOService_RegisterService_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_RegisterService_Rpl
{
    IORPCMessageMach           mach;
    IOService_RegisterService_Rpl_Content content;
};
#pragma pack()
#define IOService_RegisterService_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_RegisterService_Msg * message;
        struct IOService_RegisterService_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_RegisterService_Invocation;
struct IOService_CreateDefaultDispatchQueue_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_CreateDefaultDispatchQueue_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_CreateDefaultDispatchQueue_Msg_Content content;
};
#pragma pack()
#define IOService_CreateDefaultDispatchQueue_Msg_ObjRefs (1)

struct IOService_CreateDefaultDispatchQueue_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  queue;
};
#pragma pack(4)
struct IOService_CreateDefaultDispatchQueue_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t queue__descriptor;
    IOService_CreateDefaultDispatchQueue_Rpl_Content content;
};
#pragma pack()
#define IOService_CreateDefaultDispatchQueue_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CreateDefaultDispatchQueue_Msg * message;
        struct IOService_CreateDefaultDispatchQueue_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CreateDefaultDispatchQueue_Invocation;
struct IOService_CopyProperties_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_CopyProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_CopyProperties_Msg_Content content;
};
#pragma pack()
#define IOService_CopyProperties_Msg_ObjRefs (1)

struct IOService_CopyProperties_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  properties;
};
#pragma pack(4)
struct IOService_CopyProperties_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t properties__descriptor;
    IOService_CopyProperties_Rpl_Content content;
};
#pragma pack()
#define IOService_CopyProperties_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CopyProperties_Msg * message;
        struct IOService_CopyProperties_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CopyProperties_Invocation;
struct IOService_SearchProperty_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[128];
    const char *  plane;
#if !defined(__LP64__)
    uint32_t __planePad;
#endif /* !defined(__LP64__) */
    char __plane[128];
};
#pragma pack(4)
struct IOService_SearchProperty_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_SearchProperty_Msg_Content content;
};
#pragma pack()
#define IOService_SearchProperty_Msg_ObjRefs (1)

struct IOService_SearchProperty_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  property;
};
#pragma pack(4)
struct IOService_SearchProperty_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t property__descriptor;
    IOService_SearchProperty_Rpl_Content content;
};
#pragma pack()
#define IOService_SearchProperty_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SearchProperty_Msg * message;
        struct IOService_SearchProperty_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SearchProperty_Invocation;
struct IOService_SetProperties_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  properties;
};
#pragma pack(4)
struct IOService_SetProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t properties__descriptor;
    IOService_SetProperties_Msg_Content content;
};
#pragma pack()
#define IOService_SetProperties_Msg_ObjRefs (2)

struct IOService_SetProperties_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SetProperties_Rpl
{
    IORPCMessageMach           mach;
    IOService_SetProperties_Rpl_Content content;
};
#pragma pack()
#define IOService_SetProperties_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SetProperties_Msg * message;
        struct IOService_SetProperties_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SetProperties_Invocation;
struct IOService_JoinPMTree_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_JoinPMTree_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_JoinPMTree_Msg_Content content;
};
#pragma pack()
#define IOService_JoinPMTree_Msg_ObjRefs (1)

struct IOService_JoinPMTree_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_JoinPMTree_Rpl
{
    IORPCMessageMach           mach;
    IOService_JoinPMTree_Rpl_Content content;
};
#pragma pack()
#define IOService_JoinPMTree_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_JoinPMTree_Msg * message;
        struct IOService_JoinPMTree_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_JoinPMTree_Invocation;
struct IOService_SetPowerState_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint32_t  powerFlags;
};
#pragma pack(4)
struct IOService_SetPowerState_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_SetPowerState_Msg_Content content;
};
#pragma pack()
#define IOService_SetPowerState_Msg_ObjRefs (1)

struct IOService_SetPowerState_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SetPowerState_Rpl
{
    IORPCMessageMach           mach;
    IOService_SetPowerState_Rpl_Content content;
};
#pragma pack()
#define IOService_SetPowerState_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SetPowerState_Msg * message;
        struct IOService_SetPowerState_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SetPowerState_Invocation;
struct IOService_ChangePowerState_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint32_t  powerFlags;
};
#pragma pack(4)
struct IOService_ChangePowerState_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_ChangePowerState_Msg_Content content;
};
#pragma pack()
#define IOService_ChangePowerState_Msg_ObjRefs (1)

struct IOService_ChangePowerState_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_ChangePowerState_Rpl
{
    IORPCMessageMach           mach;
    IOService_ChangePowerState_Rpl_Content content;
};
#pragma pack()
#define IOService_ChangePowerState_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_ChangePowerState_Msg * message;
        struct IOService_ChangePowerState_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_ChangePowerState_Invocation;
struct IOService_SetPowerOverride_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    bool  enable;
};
#pragma pack(4)
struct IOService_SetPowerOverride_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_SetPowerOverride_Msg_Content content;
};
#pragma pack()
#define IOService_SetPowerOverride_Msg_ObjRefs (1)

struct IOService_SetPowerOverride_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SetPowerOverride_Rpl
{
    IORPCMessageMach           mach;
    IOService_SetPowerOverride_Rpl_Content content;
};
#pragma pack()
#define IOService_SetPowerOverride_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SetPowerOverride_Msg * message;
        struct IOService_SetPowerOverride_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SetPowerOverride_Invocation;
struct IOService_NewUserClient_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint32_t  type;
};
#pragma pack(4)
struct IOService_NewUserClient_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_NewUserClient_Msg_Content content;
};
#pragma pack()
#define IOService_NewUserClient_Msg_ObjRefs (1)

struct IOService_NewUserClient_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  userClient;
};
#pragma pack(4)
struct IOService_NewUserClient_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t userClient__descriptor;
    IOService_NewUserClient_Rpl_Content content;
};
#pragma pack()
#define IOService_NewUserClient_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_NewUserClient_Msg * message;
        struct IOService_NewUserClient_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_NewUserClient_Invocation;
struct IOService_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
    const char *  propertiesKey;
#if !defined(__LP64__)
    uint32_t __propertiesKeyPad;
#endif /* !defined(__LP64__) */
    char __propertiesKey[128];
};
#pragma pack(4)
struct IOService_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    IOService_Create_Msg_Content content;
};
#pragma pack()
#define IOService_Create_Msg_ObjRefs (2)

struct IOService_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  result;
};
#pragma pack(4)
struct IOService_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t result__descriptor;
    IOService_Create_Rpl_Content content;
};
#pragma pack()
#define IOService_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Create_Msg * message;
        struct IOService_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_Create_Invocation;
struct IOService_Terminate_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
};
#pragma pack(4)
struct IOService_Terminate_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_Terminate_Msg_Content content;
};
#pragma pack()
#define IOService_Terminate_Msg_ObjRefs (1)

struct IOService_Terminate_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_Terminate_Rpl
{
    IORPCMessageMach           mach;
    IOService_Terminate_Rpl_Content content;
};
#pragma pack()
#define IOService_Terminate_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Terminate_Msg * message;
        struct IOService_Terminate_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_Terminate_Invocation;
struct IOService_CopyProviderProperties_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  propertyKeys;
};
#pragma pack(4)
struct IOService_CopyProviderProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t propertyKeys__descriptor;
    IOService_CopyProviderProperties_Msg_Content content;
};
#pragma pack()
#define IOService_CopyProviderProperties_Msg_ObjRefs (2)

struct IOService_CopyProviderProperties_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  properties;
};
#pragma pack(4)
struct IOService_CopyProviderProperties_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t properties__descriptor;
    IOService_CopyProviderProperties_Rpl_Content content;
};
#pragma pack()
#define IOService_CopyProviderProperties_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CopyProviderProperties_Msg * message;
        struct IOService_CopyProviderProperties_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CopyProviderProperties_Invocation;
struct IOService_RequireMaxBusStall_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  maxBusStall;
};
#pragma pack(4)
struct IOService_RequireMaxBusStall_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_RequireMaxBusStall_Msg_Content content;
};
#pragma pack()
#define IOService_RequireMaxBusStall_Msg_ObjRefs (1)

struct IOService_RequireMaxBusStall_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_RequireMaxBusStall_Rpl
{
    IORPCMessageMach           mach;
    IOService_RequireMaxBusStall_Rpl_Content content;
};
#pragma pack()
#define IOService_RequireMaxBusStall_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_RequireMaxBusStall_Msg * message;
        struct IOService_RequireMaxBusStall_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_RequireMaxBusStall_Invocation;
struct IOService_AdjustBusy_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    int32_t  delta;
};
#pragma pack(4)
struct IOService_AdjustBusy_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_AdjustBusy_Msg_Content content;
};
#pragma pack()
#define IOService_AdjustBusy_Msg_ObjRefs (1)

struct IOService_AdjustBusy_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_AdjustBusy_Rpl
{
    IORPCMessageMach           mach;
    IOService_AdjustBusy_Rpl_Content content;
};
#pragma pack()
#define IOService_AdjustBusy_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_AdjustBusy_Msg * message;
        struct IOService_AdjustBusy_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_AdjustBusy_Invocation;
struct IOService_GetBusyState_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_GetBusyState_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_GetBusyState_Msg_Content content;
};
#pragma pack()
#define IOService_GetBusyState_Msg_ObjRefs (1)

struct IOService_GetBusyState_Rpl_Content
{
    IORPCMessage __hdr;
    uint32_t  busyState;
};
#pragma pack(4)
struct IOService_GetBusyState_Rpl
{
    IORPCMessageMach           mach;
    IOService_GetBusyState_Rpl_Content content;
};
#pragma pack()
#define IOService_GetBusyState_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_GetBusyState_Msg * message;
        struct IOService_GetBusyState_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_GetBusyState_Invocation;
struct IOService_CoreAnalyticsSendEvent_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  eventName;
    OSObjectRef  eventPayload;
    uint64_t  options;
};
#pragma pack(4)
struct IOService_CoreAnalyticsSendEvent_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t eventName__descriptor;
    mach_msg_port_descriptor_t eventPayload__descriptor;
    IOService_CoreAnalyticsSendEvent_Msg_Content content;
};
#pragma pack()
#define IOService_CoreAnalyticsSendEvent_Msg_ObjRefs (3)

struct IOService_CoreAnalyticsSendEvent_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_CoreAnalyticsSendEvent_Rpl
{
    IORPCMessageMach           mach;
    IOService_CoreAnalyticsSendEvent_Rpl_Content content;
};
#pragma pack()
#define IOService_CoreAnalyticsSendEvent_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CoreAnalyticsSendEvent_Msg * message;
        struct IOService_CoreAnalyticsSendEvent_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CoreAnalyticsSendEvent_Invocation;
struct IOService_UpdateReport_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  channels;
    OSObjectRef  buffer;
    uint32_t  action;
    uint64_t  offset;
    uint64_t  capacity;
};
#pragma pack(4)
struct IOService_UpdateReport_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t channels__descriptor;
    mach_msg_port_descriptor_t buffer__descriptor;
    IOService_UpdateReport_Msg_Content content;
};
#pragma pack()
#define IOService_UpdateReport_Msg_ObjRefs (3)

struct IOService_UpdateReport_Rpl_Content
{
    IORPCMessage __hdr;
    uint32_t  outElementCount;
};
#pragma pack(4)
struct IOService_UpdateReport_Rpl
{
    IORPCMessageMach           mach;
    IOService_UpdateReport_Rpl_Content content;
};
#pragma pack()
#define IOService_UpdateReport_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_UpdateReport_Msg * message;
        struct IOService_UpdateReport_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_UpdateReport_Invocation;
struct IOService_ConfigureReport_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  channels;
    uint32_t  action;
};
#pragma pack(4)
struct IOService_ConfigureReport_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t channels__descriptor;
    IOService_ConfigureReport_Msg_Content content;
};
#pragma pack()
#define IOService_ConfigureReport_Msg_ObjRefs (2)

struct IOService_ConfigureReport_Rpl_Content
{
    IORPCMessage __hdr;
    uint32_t  outCount;
};
#pragma pack(4)
struct IOService_ConfigureReport_Rpl
{
    IORPCMessageMach           mach;
    IOService_ConfigureReport_Rpl_Content content;
};
#pragma pack()
#define IOService_ConfigureReport_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_ConfigureReport_Msg * message;
        struct IOService_ConfigureReport_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_ConfigureReport_Invocation;
struct IOService_SetLegend_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  legend;
    bool  is_public;
};
#pragma pack(4)
struct IOService_SetLegend_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t legend__descriptor;
    IOService_SetLegend_Msg_Content content;
};
#pragma pack()
#define IOService_SetLegend_Msg_ObjRefs (2)

struct IOService_SetLegend_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SetLegend_Rpl
{
    IORPCMessageMach           mach;
    IOService_SetLegend_Rpl_Content content;
};
#pragma pack()
#define IOService_SetLegend_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SetLegend_Msg * message;
        struct IOService_SetLegend_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SetLegend_Invocation;
struct IOService_CopyName_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_CopyName_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_CopyName_Msg_Content content;
};
#pragma pack()
#define IOService_CopyName_Msg_ObjRefs (1)

struct IOService_CopyName_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  name;
};
#pragma pack(4)
struct IOService_CopyName_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t name__descriptor;
    IOService_CopyName_Rpl_Content content;
};
#pragma pack()
#define IOService_CopyName_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CopyName_Msg * message;
        struct IOService_CopyName_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CopyName_Invocation;
struct IOService_StringFromReturn_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    IOReturn  retval;
};
#pragma pack(4)
struct IOService_StringFromReturn_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_StringFromReturn_Msg_Content content;
};
#pragma pack()
#define IOService_StringFromReturn_Msg_ObjRefs (1)

struct IOService_StringFromReturn_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  str;
};
#pragma pack(4)
struct IOService_StringFromReturn_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t str__descriptor;
    IOService_StringFromReturn_Rpl_Content content;
};
#pragma pack()
#define IOService_StringFromReturn_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_StringFromReturn_Msg * message;
        struct IOService_StringFromReturn_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_StringFromReturn_Invocation;
struct IOService__ClaimSystemWakeEvent_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  device;
    OSObjectRef  details;
    uint64_t  flags;
    const char *  reason;
#if !defined(__LP64__)
    uint32_t __reasonPad;
#endif /* !defined(__LP64__) */
    char __reason[128];
};
#pragma pack(4)
struct IOService__ClaimSystemWakeEvent_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t device__descriptor;
    mach_msg_port_descriptor_t details__descriptor;
    IOService__ClaimSystemWakeEvent_Msg_Content content;
};
#pragma pack()
#define IOService__ClaimSystemWakeEvent_Msg_ObjRefs (3)

struct IOService__ClaimSystemWakeEvent_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService__ClaimSystemWakeEvent_Rpl
{
    IORPCMessageMach           mach;
    IOService__ClaimSystemWakeEvent_Rpl_Content content;
};
#pragma pack()
#define IOService__ClaimSystemWakeEvent_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService__ClaimSystemWakeEvent_Msg * message;
        struct IOService__ClaimSystemWakeEvent_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService__ClaimSystemWakeEvent_Invocation;
struct IOService_UserSetProperties_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  properties;
};
#pragma pack(4)
struct IOService_UserSetProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t properties__descriptor;
    IOService_UserSetProperties_Msg_Content content;
};
#pragma pack()
#define IOService_UserSetProperties_Msg_ObjRefs (2)

struct IOService_UserSetProperties_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_UserSetProperties_Rpl
{
    IORPCMessageMach           mach;
    IOService_UserSetProperties_Rpl_Content content;
};
#pragma pack()
#define IOService_UserSetProperties_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_UserSetProperties_Msg * message;
        struct IOService_UserSetProperties_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_UserSetProperties_Invocation;
struct IOService_SendIOMessageServicePropertyChange_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_SendIOMessageServicePropertyChange_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_SendIOMessageServicePropertyChange_Msg_Content content;
};
#pragma pack()
#define IOService_SendIOMessageServicePropertyChange_Msg_ObjRefs (1)

struct IOService_SendIOMessageServicePropertyChange_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_SendIOMessageServicePropertyChange_Rpl
{
    IORPCMessageMach           mach;
    IOService_SendIOMessageServicePropertyChange_Rpl_Content content;
};
#pragma pack()
#define IOService_SendIOMessageServicePropertyChange_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_SendIOMessageServicePropertyChange_Msg * message;
        struct IOService_SendIOMessageServicePropertyChange_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_SendIOMessageServicePropertyChange_Invocation;
struct IOService_RemoveProperty_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  propertyName;
};
#pragma pack(4)
struct IOService_RemoveProperty_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t propertyName__descriptor;
    IOService_RemoveProperty_Msg_Content content;
};
#pragma pack()
#define IOService_RemoveProperty_Msg_ObjRefs (2)

struct IOService_RemoveProperty_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_RemoveProperty_Rpl
{
    IORPCMessageMach           mach;
    IOService_RemoveProperty_Rpl_Content content;
};
#pragma pack()
#define IOService_RemoveProperty_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_RemoveProperty_Msg * message;
        struct IOService_RemoveProperty_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_RemoveProperty_Invocation;
struct IOService_CopySystemStateNotificationService_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOService_CopySystemStateNotificationService_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_CopySystemStateNotificationService_Msg_Content content;
};
#pragma pack()
#define IOService_CopySystemStateNotificationService_Msg_ObjRefs (1)

struct IOService_CopySystemStateNotificationService_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  service;
};
#pragma pack(4)
struct IOService_CopySystemStateNotificationService_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t service__descriptor;
    IOService_CopySystemStateNotificationService_Rpl_Content content;
};
#pragma pack()
#define IOService_CopySystemStateNotificationService_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CopySystemStateNotificationService_Msg * message;
        struct IOService_CopySystemStateNotificationService_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CopySystemStateNotificationService_Invocation;
struct IOService_StateNotificationItemCreate_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  itemName;
    OSObjectRef  value;
};
#pragma pack(4)
struct IOService_StateNotificationItemCreate_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t itemName__descriptor;
    mach_msg_port_descriptor_t value__descriptor;
    IOService_StateNotificationItemCreate_Msg_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemCreate_Msg_ObjRefs (3)

struct IOService_StateNotificationItemCreate_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_StateNotificationItemCreate_Rpl
{
    IORPCMessageMach           mach;
    IOService_StateNotificationItemCreate_Rpl_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemCreate_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_StateNotificationItemCreate_Msg * message;
        struct IOService_StateNotificationItemCreate_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_StateNotificationItemCreate_Invocation;
struct IOService_StateNotificationItemSet_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  itemName;
    OSObjectRef  value;
};
#pragma pack(4)
struct IOService_StateNotificationItemSet_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t itemName__descriptor;
    mach_msg_port_descriptor_t value__descriptor;
    IOService_StateNotificationItemSet_Msg_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemSet_Msg_ObjRefs (3)

struct IOService_StateNotificationItemSet_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_StateNotificationItemSet_Rpl
{
    IORPCMessageMach           mach;
    IOService_StateNotificationItemSet_Rpl_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemSet_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_StateNotificationItemSet_Msg * message;
        struct IOService_StateNotificationItemSet_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_StateNotificationItemSet_Invocation;
struct IOService_StateNotificationItemCopy_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  itemName;
};
#pragma pack(4)
struct IOService_StateNotificationItemCopy_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t itemName__descriptor;
    IOService_StateNotificationItemCopy_Msg_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemCopy_Msg_ObjRefs (2)

struct IOService_StateNotificationItemCopy_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  value;
};
#pragma pack(4)
struct IOService_StateNotificationItemCopy_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t value__descriptor;
    IOService_StateNotificationItemCopy_Rpl_Content content;
};
#pragma pack()
#define IOService_StateNotificationItemCopy_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_StateNotificationItemCopy_Msg * message;
        struct IOService_StateNotificationItemCopy_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_StateNotificationItemCopy_Invocation;
struct IOService_CreatePMAssertion_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint32_t  assertionBits;
    bool  synced;
};
#pragma pack(4)
struct IOService_CreatePMAssertion_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_CreatePMAssertion_Msg_Content content;
};
#pragma pack()
#define IOService_CreatePMAssertion_Msg_ObjRefs (1)

struct IOService_CreatePMAssertion_Rpl_Content
{
    IORPCMessage __hdr;
    uint64_t  assertionID;
};
#pragma pack(4)
struct IOService_CreatePMAssertion_Rpl
{
    IORPCMessageMach           mach;
    IOService_CreatePMAssertion_Rpl_Content content;
};
#pragma pack()
#define IOService_CreatePMAssertion_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_CreatePMAssertion_Msg * message;
        struct IOService_CreatePMAssertion_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_CreatePMAssertion_Invocation;
struct IOService_ReleasePMAssertion_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  assertionID;
};
#pragma pack(4)
struct IOService_ReleasePMAssertion_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOService_ReleasePMAssertion_Msg_Content content;
};
#pragma pack()
#define IOService_ReleasePMAssertion_Msg_ObjRefs (1)

struct IOService_ReleasePMAssertion_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_ReleasePMAssertion_Rpl
{
    IORPCMessageMach           mach;
    IOService_ReleasePMAssertion_Rpl_Content content;
};
#pragma pack()
#define IOService_ReleasePMAssertion_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_ReleasePMAssertion_Msg * message;
        struct IOService_ReleasePMAssertion_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_ReleasePMAssertion_Invocation;
struct IOService_Stop_async_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
};
#pragma pack(4)
struct IOService_Stop_async_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    IOService_Stop_async_Msg_Content content;
};
#pragma pack()
#define IOService_Stop_async_Msg_ObjRefs (2)

struct IOService_Stop_async_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOService_Stop_async_Rpl
{
    IORPCMessageMach           mach;
    IOService_Stop_async_Rpl_Content content;
};
#pragma pack()
#define IOService_Stop_async_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService_Stop_async_Msg * message;
        struct IOService_Stop_async_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService_Stop_async_Invocation;
struct IOService__NewUserClient_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  entitlements;
    uint32_t  type;
};
#pragma pack(4)
struct IOService__NewUserClient_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t entitlements__descriptor;
    IOService__NewUserClient_Msg_Content content;
};
#pragma pack()
#define IOService__NewUserClient_Msg_ObjRefs (2)

struct IOService__NewUserClient_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  userClient;
};
#pragma pack(4)
struct IOService__NewUserClient_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t userClient__descriptor;
    IOService__NewUserClient_Rpl_Content content;
};
#pragma pack()
#define IOService__NewUserClient_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOService__NewUserClient_Msg * message;
        struct IOService__NewUserClient_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOService__NewUserClient_Invocation;
kern_return_t
IOService::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOService::_Dispatch(IOService * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOService_Start_ID:
        {
            ret = IOService::Start_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Start_Handler, *self, &IOService::Start_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_Stop_ID:
        {
            ret = IOService::Stop_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Stop_Handler, *self, &IOService::Stop_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_ClientCrashed_ID:
        {
            ret = IOService::ClientCrashed_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ClientCrashed_Handler, *self, &IOService::ClientCrashed_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_GetRegistryEntryID_ID:
        {
            ret = IOService::GetRegistryEntryID_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::GetRegistryEntryID_Handler, *self, &IOService::GetRegistryEntryID_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SetName_ID:
        {
            ret = IOService::SetName_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetName_Handler, *self, &IOService::SetName_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_RegisterService_ID:
        {
            ret = IOService::RegisterService_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::RegisterService_Handler, *self, &IOService::RegisterService_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case OSObject_SetDispatchQueue_ID:
        {
            ret = OSObject::SetDispatchQueue_Invoke(rpc, self, SimpleMemberFunctionCast(OSObject::SetDispatchQueue_Handler, *self, &IOService::SetDispatchQueue_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case OSObject_CopyDispatchQueue_ID:
        {
            ret = OSObject::CopyDispatchQueue_Invoke(rpc, self, SimpleMemberFunctionCast(OSObject::CopyDispatchQueue_Handler, *self, &IOService::CopyDispatchQueue_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CreateDefaultDispatchQueue_ID:
        {
            ret = IOService::CreateDefaultDispatchQueue_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CreateDefaultDispatchQueue_Handler, *self, &IOService::CreateDefaultDispatchQueue_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CopyProperties_ID:
        {
            ret = IOService::CopyProperties_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CopyProperties_Handler, *self, &IOService::CopyProperties_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SearchProperty_ID:
        {
            ret = IOService::SearchProperty_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SearchProperty_Handler, *self, &IOService::SearchProperty_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SetProperties_ID:
        {
            ret = IOService::SetProperties_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetProperties_Handler, *self, &IOService::SetProperties_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_JoinPMTree_ID:
        {
            ret = IOService::JoinPMTree_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::JoinPMTree_Handler, *self, &IOService::JoinPMTree_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SetPowerState_ID:
        {
            ret = IOService::SetPowerState_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetPowerState_Handler, *self, &IOService::SetPowerState_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_ChangePowerState_ID:
        {
            ret = IOService::ChangePowerState_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ChangePowerState_Handler, *self, &IOService::ChangePowerState_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SetPowerOverride_ID:
        {
            ret = IOService::SetPowerOverride_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetPowerOverride_Handler, *self, &IOService::SetPowerOverride_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_NewUserClient_ID:
        {
            ret = IOService::NewUserClient_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::NewUserClient_Handler, *self, &IOService::NewUserClient_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_Create_ID:
        {
            ret = IOService::Create_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Create_Handler, *self, &IOService::Create_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_Terminate_ID:
        {
            ret = IOService::Terminate_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Terminate_Handler, *self, &IOService::Terminate_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CopyProviderProperties_ID:
        {
            ret = IOService::CopyProviderProperties_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CopyProviderProperties_Handler, *self, &IOService::CopyProviderProperties_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_RequireMaxBusStall_ID:
        {
            ret = IOService::RequireMaxBusStall_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::RequireMaxBusStall_Handler, *self, &IOService::RequireMaxBusStall_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_AdjustBusy_ID:
        {
            ret = IOService::AdjustBusy_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::AdjustBusy_Handler, *self, &IOService::AdjustBusy_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_GetBusyState_ID:
        {
            ret = IOService::GetBusyState_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::GetBusyState_Handler, *self, &IOService::GetBusyState_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CoreAnalyticsSendEvent_ID:
        {
            ret = IOService::CoreAnalyticsSendEvent_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CoreAnalyticsSendEvent_Handler, *self, &IOService::CoreAnalyticsSendEvent_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_UpdateReport_ID:
        {
            ret = IOService::UpdateReport_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::UpdateReport_Handler, *self, &IOService::UpdateReport_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_ConfigureReport_ID:
        {
            ret = IOService::ConfigureReport_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ConfigureReport_Handler, *self, &IOService::ConfigureReport_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SetLegend_ID:
        {
            ret = IOService::SetLegend_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetLegend_Handler, *self, &IOService::SetLegend_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CopyName_ID:
        {
            ret = IOService::CopyName_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CopyName_Handler, *self, &IOService::CopyName_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_StringFromReturn_ID:
        {
            ret = IOService::StringFromReturn_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::StringFromReturn_Handler, *self, &IOService::StringFromReturn_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService__ClaimSystemWakeEvent_ID:
        {
            ret = IOService::_ClaimSystemWakeEvent_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::_ClaimSystemWakeEvent_Handler, *self, &IOService::_ClaimSystemWakeEvent_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_UserSetProperties_ID:
        {
            ret = IOService::UserSetProperties_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::UserSetProperties_Handler, *self, &IOService::UserSetProperties_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_SendIOMessageServicePropertyChange_ID:
        {
            ret = IOService::SendIOMessageServicePropertyChange_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SendIOMessageServicePropertyChange_Handler, *self, &IOService::SendIOMessageServicePropertyChange_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_RemoveProperty_ID:
        {
            ret = IOService::RemoveProperty_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::RemoveProperty_Handler, *self, &IOService::RemoveProperty_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CopySystemStateNotificationService_ID:
        {
            ret = IOService::CopySystemStateNotificationService_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CopySystemStateNotificationService_Handler, *self, &IOService::CopySystemStateNotificationService_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_StateNotificationItemCreate_ID:
        {
            ret = IOService::StateNotificationItemCreate_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::StateNotificationItemCreate_Handler, *self, &IOService::StateNotificationItemCreate_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_StateNotificationItemSet_ID:
        {
            ret = IOService::StateNotificationItemSet_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::StateNotificationItemSet_Handler, *self, &IOService::StateNotificationItemSet_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_StateNotificationItemCopy_ID:
        {
            ret = IOService::StateNotificationItemCopy_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::StateNotificationItemCopy_Handler, *self, &IOService::StateNotificationItemCopy_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_CreatePMAssertion_ID:
        {
            ret = IOService::CreatePMAssertion_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::CreatePMAssertion_Handler, *self, &IOService::CreatePMAssertion_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_ReleasePMAssertion_ID:
        {
            ret = IOService::ReleasePMAssertion_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ReleasePMAssertion_Handler, *self, &IOService::ReleasePMAssertion_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService_Stop_async_ID:
        {
            ret = IOService::Stop_async_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Stop_async_Handler, *self, &IOService::Stop_async_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOService__NewUserClient_ID:
        {
            ret = IOService::_NewUserClient_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::_NewUserClient_Handler, *self, &IOService::_NewUserClient_Impl));
            break;
        }
#endif /* !KERNEL */

        default:
            ret = OSObject::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IOService::MetaClass::Dispatch(const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}
#endif /* KERNEL */

kern_return_t
IOService::Start(
        IOService * provider,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Start_Msg msg;
        struct
        {
            IOService_Start_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_Start_Msg * msg = &buf.msg;
    struct IOService_Start_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_Start_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_Start_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_Start_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_Start_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_Start_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::Start_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Start_Handler func)
{
    IOService_Start_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * provider;

    if (IOService_Start_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);

    ret = (*func)(target,
        provider);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_Start_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_Start_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::Stop(
        IOService * provider,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Stop_Msg msg;
        struct
        {
            IOService_Stop_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_Stop_Msg * msg = &buf.msg;
    struct IOService_Stop_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_Stop_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_Stop_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_Stop_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_Stop_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_Stop_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::Stop_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Stop_Handler func)
{
    IOService_Stop_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * provider;

    if (IOService_Stop_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);

    ret = (*func)(target,
        provider);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_Stop_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_Stop_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::ClientCrashed(
        IOService * client,
        uint64_t options,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_ClientCrashed_Msg msg;
        struct
        {
            IOService_ClientCrashed_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_ClientCrashed_Msg * msg = &buf.msg;
    struct IOService_ClientCrashed_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_ClientCrashed_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_ClientCrashed_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_ClientCrashed_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->client__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.client = (OSObjectRef) client;

    msg->content.options = options;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_ClientCrashed_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_ClientCrashed_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::ClientCrashed_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ClientCrashed_Handler func)
{
    IOService_ClientCrashed_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * client;

    if (IOService_ClientCrashed_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    client = OSDynamicCast(IOService, (OSObject *) rpc.message->content.client);
    if (!client && rpc.message->content.client) return (kIOReturnBadArgument);

    ret = (*func)(target,
        client,
        rpc.message->content.options);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_ClientCrashed_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_ClientCrashed_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::GetRegistryEntryID(
        uint64_t * registryEntryID,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_GetRegistryEntryID_Msg msg;
        struct
        {
            IOService_GetRegistryEntryID_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_GetRegistryEntryID_Msg * msg = &buf.msg;
    struct IOService_GetRegistryEntryID_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_GetRegistryEntryID_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_GetRegistryEntryID_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_GetRegistryEntryID_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_GetRegistryEntryID_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_GetRegistryEntryID_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (registryEntryID) *registryEntryID = rpl->content.registryEntryID;
    }

    return (ret);
}

kern_return_t
IOService::GetRegistryEntryID_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        GetRegistryEntryID_Handler func)
{
    IOService_GetRegistryEntryID_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_GetRegistryEntryID_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.registryEntryID);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_GetRegistryEntryID_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_GetRegistryEntryID_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::SetName(
        const char * name,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SetName_Msg msg;
        struct
        {
            IOService_SetName_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SetName_Msg * msg = &buf.msg;
    struct IOService_SetName_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SetName_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetName_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetName_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SetName_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SetName_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SetName_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetName_Handler func)
{
    IOService_SetName_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SetName_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;

    ret = (*func)(target,
        &rpc.message->content.__name[0]);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SetName_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SetName_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::RegisterService(
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_RegisterService_Msg msg;
        struct
        {
            IOService_RegisterService_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_RegisterService_Msg * msg = &buf.msg;
    struct IOService_RegisterService_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_RegisterService_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_RegisterService_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_RegisterService_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_RegisterService_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_RegisterService_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::RegisterService_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        RegisterService_Handler func)
{
    IOService_RegisterService_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_RegisterService_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_RegisterService_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_RegisterService_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::CreateDefaultDispatchQueue(
        IODispatchQueue ** queue,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CreateDefaultDispatchQueue_Msg msg;
        struct
        {
            IOService_CreateDefaultDispatchQueue_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CreateDefaultDispatchQueue_Msg * msg = &buf.msg;
    struct IOService_CreateDefaultDispatchQueue_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CreateDefaultDispatchQueue_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CreateDefaultDispatchQueue_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CreateDefaultDispatchQueue_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CreateDefaultDispatchQueue_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_CreateDefaultDispatchQueue_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpl->content.queue);
        if (rpl->content.queue && !*queue) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::CreateDefaultDispatchQueue_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CreateDefaultDispatchQueue_Handler func)
{
    IOService_CreateDefaultDispatchQueue_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CreateDefaultDispatchQueue_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (IODispatchQueue **)&rpc.reply->content.queue);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CreateDefaultDispatchQueue_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CreateDefaultDispatchQueue_Rpl_ObjRefs;
    rpc.reply->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::CopyProperties(
        OSDictionary ** properties,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CopyProperties_Msg msg;
        struct
        {
            IOService_CopyProperties_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CopyProperties_Msg * msg = &buf.msg;
    struct IOService_CopyProperties_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CopyProperties_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CopyProperties_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CopyProperties_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CopyProperties_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_CopyProperties_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *properties = OSDynamicCast(OSDictionary, (OSObject *) rpl->content.properties);
        if (rpl->content.properties && !*properties) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::CopyProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyProperties_Handler func)
{
    IOService_CopyProperties_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CopyProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (OSDictionary **)&rpc.reply->content.properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopyProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopyProperties_Rpl_ObjRefs;
    rpc.reply->properties__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::SearchProperty(
        const char * name,
        const char * plane,
        uint64_t options,
        OSContainer ** property,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SearchProperty_Msg msg;
        struct
        {
            IOService_SearchProperty_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SearchProperty_Msg * msg = &buf.msg;
    struct IOService_SearchProperty_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SearchProperty_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SearchProperty_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SearchProperty_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    msg->content.plane = NULL;

    strlcpy(&msg->content.__plane[0], plane, sizeof(msg->content.__plane));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SearchProperty_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_SearchProperty_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *property = OSDynamicCast(OSContainer, (OSObject *) rpl->content.property);
        if (rpl->content.property && !*property) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::SearchProperty_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SearchProperty_Handler func)
{
    IOService_SearchProperty_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SearchProperty_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;
    if (strnlen(&rpc.message->content.__plane[0], sizeof(rpc.message->content.__plane)) >= sizeof(rpc.message->content.__plane)) return kIOReturnBadArgument;

    ret = (*func)(target,
        &rpc.message->content.__name[0],
        &rpc.message->content.__plane[0],
        rpc.message->content.options,
        (OSContainer **)&rpc.reply->content.property);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SearchProperty_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_SearchProperty_Rpl_ObjRefs;
    rpc.reply->property__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::SetProperties(
        OSDictionary * properties,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SetProperties_Msg msg;
        struct
        {
            IOService_SetProperties_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SetProperties_Msg * msg = &buf.msg;
    struct IOService_SetProperties_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SetProperties_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetProperties_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetProperties_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->properties__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.properties = (OSObjectRef) properties;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SetProperties_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SetProperties_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SetProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetProperties_Handler func)
{
    IOService_SetProperties_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSDictionary * properties;

    if (IOService_SetProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    properties = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.properties);
    if (!properties && rpc.message->content.properties) return (kIOReturnBadArgument);

    ret = (*func)(target,
        properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SetProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SetProperties_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::JoinPMTree(
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_JoinPMTree_Msg msg;
        struct
        {
            IOService_JoinPMTree_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_JoinPMTree_Msg * msg = &buf.msg;
    struct IOService_JoinPMTree_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_JoinPMTree_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_JoinPMTree_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_JoinPMTree_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_JoinPMTree_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_JoinPMTree_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::JoinPMTree_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        JoinPMTree_Handler func)
{
    IOService_JoinPMTree_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_JoinPMTree_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_JoinPMTree_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_JoinPMTree_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::SetPowerState(
        uint32_t powerFlags,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SetPowerState_Msg msg;
        struct
        {
            IOService_SetPowerState_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SetPowerState_Msg * msg = &buf.msg;
    struct IOService_SetPowerState_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SetPowerState_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetPowerState_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetPowerState_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.powerFlags = powerFlags;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SetPowerState_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SetPowerState_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SetPowerState_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetPowerState_Handler func)
{
    IOService_SetPowerState_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SetPowerState_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.powerFlags);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SetPowerState_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SetPowerState_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::ChangePowerState(
        uint32_t powerFlags,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_ChangePowerState_Msg msg;
        struct
        {
            IOService_ChangePowerState_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_ChangePowerState_Msg * msg = &buf.msg;
    struct IOService_ChangePowerState_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_ChangePowerState_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_ChangePowerState_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_ChangePowerState_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.powerFlags = powerFlags;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_ChangePowerState_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_ChangePowerState_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::ChangePowerState_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ChangePowerState_Handler func)
{
    IOService_ChangePowerState_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_ChangePowerState_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.powerFlags);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_ChangePowerState_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_ChangePowerState_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::SetPowerOverride(
        bool enable,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SetPowerOverride_Msg msg;
        struct
        {
            IOService_SetPowerOverride_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SetPowerOverride_Msg * msg = &buf.msg;
    struct IOService_SetPowerOverride_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SetPowerOverride_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetPowerOverride_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetPowerOverride_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.enable = enable;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SetPowerOverride_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SetPowerOverride_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SetPowerOverride_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetPowerOverride_Handler func)
{
    IOService_SetPowerOverride_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SetPowerOverride_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.enable);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SetPowerOverride_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SetPowerOverride_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::NewUserClient(
        uint32_t type,
        IOUserClient ** userClient,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_NewUserClient_Msg msg;
        struct
        {
            IOService_NewUserClient_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_NewUserClient_Msg * msg = &buf.msg;
    struct IOService_NewUserClient_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_NewUserClient_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_NewUserClient_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_NewUserClient_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.type = type;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_NewUserClient_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_NewUserClient_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *userClient = OSDynamicCast(IOUserClient, (OSObject *) rpl->content.userClient);
        if (rpl->content.userClient && !*userClient) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::NewUserClient_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        NewUserClient_Handler func)
{
    IOService_NewUserClient_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_NewUserClient_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.type,
        (IOUserClient **)&rpc.reply->content.userClient);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_NewUserClient_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_NewUserClient_Rpl_ObjRefs;
    rpc.reply->userClient__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::Create(
        IOService * provider,
        const char * propertiesKey,
        IOService ** result,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Create_Msg msg;
        struct
        {
            IOService_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_Create_Msg * msg = &buf.msg;
    struct IOService_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_Create_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    msg->content.propertiesKey = NULL;

    strlcpy(&msg->content.__propertiesKey[0], propertiesKey, sizeof(msg->content.__propertiesKey));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *result = OSDynamicCast(IOService, (OSObject *) rpl->content.result);
        if (rpl->content.result && !*result) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::Create_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Create_Handler func)
{
    IOService_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * provider;

    if (IOService_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);
    if (strnlen(&rpc.message->content.__propertiesKey[0], sizeof(rpc.message->content.__propertiesKey)) >= sizeof(rpc.message->content.__propertiesKey)) return kIOReturnBadArgument;

    ret = (*func)(target,
        provider,
        &rpc.message->content.__propertiesKey[0],
        (IOService **)&rpc.reply->content.result);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_Create_Rpl_ObjRefs;
    rpc.reply->result__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::Terminate(
        uint64_t options,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Terminate_Msg msg;
        struct
        {
            IOService_Terminate_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_Terminate_Msg * msg = &buf.msg;
    struct IOService_Terminate_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_Terminate_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_Terminate_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_Terminate_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_Terminate_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_Terminate_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::Terminate_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Terminate_Handler func)
{
    IOService_Terminate_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_Terminate_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.options);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_Terminate_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_Terminate_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::CopyProviderProperties(
        OSArray * propertyKeys,
        OSArray ** properties,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CopyProviderProperties_Msg msg;
        struct
        {
            IOService_CopyProviderProperties_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CopyProviderProperties_Msg * msg = &buf.msg;
    struct IOService_CopyProviderProperties_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CopyProviderProperties_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CopyProviderProperties_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CopyProviderProperties_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->propertyKeys__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.propertyKeys = (OSObjectRef) propertyKeys;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CopyProviderProperties_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_CopyProviderProperties_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *properties = OSDynamicCast(OSArray, (OSObject *) rpl->content.properties);
        if (rpl->content.properties && !*properties) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::CopyProviderProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyProviderProperties_Handler func)
{
    IOService_CopyProviderProperties_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSArray * propertyKeys;

    if (IOService_CopyProviderProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    propertyKeys = OSDynamicCast(OSArray, (OSObject *) rpc.message->content.propertyKeys);
    if (!propertyKeys && rpc.message->content.propertyKeys) return (kIOReturnBadArgument);

    ret = (*func)(target,
        propertyKeys,
        (OSArray **)&rpc.reply->content.properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopyProviderProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopyProviderProperties_Rpl_ObjRefs;
    rpc.reply->properties__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::RequireMaxBusStall(
        uint64_t maxBusStall,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_RequireMaxBusStall_Msg msg;
        struct
        {
            IOService_RequireMaxBusStall_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_RequireMaxBusStall_Msg * msg = &buf.msg;
    struct IOService_RequireMaxBusStall_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_RequireMaxBusStall_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_RequireMaxBusStall_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_RequireMaxBusStall_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.maxBusStall = maxBusStall;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_RequireMaxBusStall_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_RequireMaxBusStall_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::RequireMaxBusStall_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        RequireMaxBusStall_Handler func)
{
    IOService_RequireMaxBusStall_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_RequireMaxBusStall_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.maxBusStall);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_RequireMaxBusStall_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_RequireMaxBusStall_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::AdjustBusy(
        int32_t delta,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_AdjustBusy_Msg msg;
        struct
        {
            IOService_AdjustBusy_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_AdjustBusy_Msg * msg = &buf.msg;
    struct IOService_AdjustBusy_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_AdjustBusy_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_AdjustBusy_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_AdjustBusy_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.delta = delta;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_AdjustBusy_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_AdjustBusy_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::AdjustBusy_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        AdjustBusy_Handler func)
{
    IOService_AdjustBusy_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_AdjustBusy_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.delta);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_AdjustBusy_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_AdjustBusy_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::GetBusyState(
        uint32_t * busyState,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_GetBusyState_Msg msg;
        struct
        {
            IOService_GetBusyState_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_GetBusyState_Msg * msg = &buf.msg;
    struct IOService_GetBusyState_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_GetBusyState_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_GetBusyState_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_GetBusyState_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_GetBusyState_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_GetBusyState_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (busyState) *busyState = rpl->content.busyState;
    }

    return (ret);
}

kern_return_t
IOService::GetBusyState_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        GetBusyState_Handler func)
{
    IOService_GetBusyState_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_GetBusyState_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.busyState);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_GetBusyState_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_GetBusyState_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::CoreAnalyticsSendEvent(
        uint64_t options,
        OSString * eventName,
        OSDictionary * eventPayload,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CoreAnalyticsSendEvent_Msg msg;
        struct
        {
            IOService_CoreAnalyticsSendEvent_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CoreAnalyticsSendEvent_Msg * msg = &buf.msg;
    struct IOService_CoreAnalyticsSendEvent_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CoreAnalyticsSendEvent_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CoreAnalyticsSendEvent_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CoreAnalyticsSendEvent_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->eventName__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.eventName = (OSObjectRef) eventName;

    msg->eventPayload__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.eventPayload = (OSObjectRef) eventPayload;

    msg->content.options = options;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CoreAnalyticsSendEvent_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_CoreAnalyticsSendEvent_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::CoreAnalyticsSendEvent_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CoreAnalyticsSendEvent_Handler func)
{
    IOService_CoreAnalyticsSendEvent_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * eventName;
    OSDictionary * eventPayload;

    if (IOService_CoreAnalyticsSendEvent_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    eventName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.eventName);
    if (!eventName && rpc.message->content.eventName) return (kIOReturnBadArgument);
    eventPayload = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.eventPayload);
    if (!eventPayload && rpc.message->content.eventPayload) return (kIOReturnBadArgument);

    ret = (*func)(target,
        rpc.message->content.options,
        eventName,
        eventPayload);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CoreAnalyticsSendEvent_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_CoreAnalyticsSendEvent_Rpl_ObjRefs;

    return (ret);
}

IOReturn
IOService::UpdateReport(
        OSData * channels,
        uint32_t action,
        uint32_t * outElementCount,
        uint64_t offset,
        uint64_t capacity,
        IOMemoryDescriptor * buffer,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_UpdateReport_Msg msg;
        struct
        {
            IOService_UpdateReport_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_UpdateReport_Msg * msg = &buf.msg;
    struct IOService_UpdateReport_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_UpdateReport_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_UpdateReport_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_UpdateReport_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->channels__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.channels = (OSObjectRef) channels;

    msg->buffer__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.buffer = (OSObjectRef) buffer;

    msg->content.action = action;

    msg->content.offset = offset;

    msg->content.capacity = capacity;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_UpdateReport_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_UpdateReport_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (outElementCount) *outElementCount = rpl->content.outElementCount;
    }

    return (ret);
}

kern_return_t
IOService::UpdateReport_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        UpdateReport_Handler func)
{
    IOService_UpdateReport_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSData * channels;
    IOMemoryDescriptor * buffer;

    if (IOService_UpdateReport_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    channels = OSDynamicCast(OSData, (OSObject *) rpc.message->content.channels);
    if (!channels && rpc.message->content.channels) return (kIOReturnBadArgument);
    buffer = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.buffer);
    if (!buffer && rpc.message->content.buffer) return (kIOReturnBadArgument);

    ret = (*func)(target,
        channels,
        rpc.message->content.action,
        &rpc.reply->content.outElementCount,
        rpc.message->content.offset,
        rpc.message->content.capacity,
        buffer);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_UpdateReport_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_UpdateReport_Rpl_ObjRefs;

    return (ret);
}

IOReturn
IOService::ConfigureReport(
        OSData * channels,
        uint32_t action,
        uint32_t * outCount,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_ConfigureReport_Msg msg;
        struct
        {
            IOService_ConfigureReport_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_ConfigureReport_Msg * msg = &buf.msg;
    struct IOService_ConfigureReport_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_ConfigureReport_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_ConfigureReport_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_ConfigureReport_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->channels__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.channels = (OSObjectRef) channels;

    msg->content.action = action;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_ConfigureReport_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_ConfigureReport_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (outCount) *outCount = rpl->content.outCount;
    }

    return (ret);
}

kern_return_t
IOService::ConfigureReport_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ConfigureReport_Handler func)
{
    IOService_ConfigureReport_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSData * channels;

    if (IOService_ConfigureReport_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    channels = OSDynamicCast(OSData, (OSObject *) rpc.message->content.channels);
    if (!channels && rpc.message->content.channels) return (kIOReturnBadArgument);

    ret = (*func)(target,
        channels,
        rpc.message->content.action,
        &rpc.reply->content.outCount);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_ConfigureReport_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_ConfigureReport_Rpl_ObjRefs;

    return (ret);
}

IOReturn
IOService::SetLegend(
        OSArray * legend,
        bool is_public,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SetLegend_Msg msg;
        struct
        {
            IOService_SetLegend_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SetLegend_Msg * msg = &buf.msg;
    struct IOService_SetLegend_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SetLegend_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetLegend_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetLegend_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->legend__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.legend = (OSObjectRef) legend;

    msg->content.is_public = is_public;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SetLegend_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SetLegend_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SetLegend_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetLegend_Handler func)
{
    IOService_SetLegend_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSArray * legend;

    if (IOService_SetLegend_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    legend = OSDynamicCast(OSArray, (OSObject *) rpc.message->content.legend);
    if (!legend && rpc.message->content.legend) return (kIOReturnBadArgument);

    ret = (*func)(target,
        legend,
        rpc.message->content.is_public);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SetLegend_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SetLegend_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::CopyName(
        OSString ** name,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CopyName_Msg msg;
        struct
        {
            IOService_CopyName_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CopyName_Msg * msg = &buf.msg;
    struct IOService_CopyName_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CopyName_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CopyName_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CopyName_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CopyName_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_CopyName_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *name = OSDynamicCast(OSString, (OSObject *) rpl->content.name);
        if (rpl->content.name && !*name) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::CopyName_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyName_Handler func)
{
    IOService_CopyName_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CopyName_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (OSString **)&rpc.reply->content.name);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopyName_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopyName_Rpl_ObjRefs;
    rpc.reply->name__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::StringFromReturn(
        IOReturn retval,
        OSString ** str,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_StringFromReturn_Msg msg;
        struct
        {
            IOService_StringFromReturn_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_StringFromReturn_Msg * msg = &buf.msg;
    struct IOService_StringFromReturn_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_StringFromReturn_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_StringFromReturn_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_StringFromReturn_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.retval = retval;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_StringFromReturn_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_StringFromReturn_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *str = OSDynamicCast(OSString, (OSObject *) rpl->content.str);
        if (rpl->content.str && !*str) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::StringFromReturn_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StringFromReturn_Handler func)
{
    IOService_StringFromReturn_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_StringFromReturn_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.retval,
        (OSString **)&rpc.reply->content.str);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_StringFromReturn_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_StringFromReturn_Rpl_ObjRefs;
    rpc.reply->str__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::_ClaimSystemWakeEvent(
        IOService * device,
        uint64_t flags,
        const char * reason,
        OSContainer * details,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService__ClaimSystemWakeEvent_Msg msg;
        struct
        {
            IOService__ClaimSystemWakeEvent_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService__ClaimSystemWakeEvent_Msg * msg = &buf.msg;
    struct IOService__ClaimSystemWakeEvent_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService__ClaimSystemWakeEvent_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService__ClaimSystemWakeEvent_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService__ClaimSystemWakeEvent_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->device__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.device = (OSObjectRef) device;

    msg->details__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.details = (OSObjectRef) details;

    msg->content.flags = flags;

    msg->content.reason = NULL;

    strlcpy(&msg->content.__reason[0], reason, sizeof(msg->content.__reason));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService__ClaimSystemWakeEvent_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService__ClaimSystemWakeEvent_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::_ClaimSystemWakeEvent_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _ClaimSystemWakeEvent_Handler func)
{
    IOService__ClaimSystemWakeEvent_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * device;
    OSContainer * details;

    if (IOService__ClaimSystemWakeEvent_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    device = OSDynamicCast(IOService, (OSObject *) rpc.message->content.device);
    if (!device && rpc.message->content.device) return (kIOReturnBadArgument);
    details = OSDynamicCast(OSContainer, (OSObject *) rpc.message->content.details);
    if (!details && rpc.message->content.details) return (kIOReturnBadArgument);
    if (strnlen(&rpc.message->content.__reason[0], sizeof(rpc.message->content.__reason)) >= sizeof(rpc.message->content.__reason)) return kIOReturnBadArgument;

    ret = (*func)(target,
        device,
        rpc.message->content.flags,
        &rpc.message->content.__reason[0],
        details);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService__ClaimSystemWakeEvent_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService__ClaimSystemWakeEvent_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::UserSetProperties(
        OSContainer * properties,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_UserSetProperties_Msg msg;
        struct
        {
            IOService_UserSetProperties_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_UserSetProperties_Msg * msg = &buf.msg;
    struct IOService_UserSetProperties_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_UserSetProperties_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_UserSetProperties_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_UserSetProperties_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->properties__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.properties = (OSObjectRef) properties;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_UserSetProperties_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_UserSetProperties_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::UserSetProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        UserSetProperties_Handler func)
{
    IOService_UserSetProperties_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSContainer * properties;

    if (IOService_UserSetProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    properties = OSDynamicCast(OSContainer, (OSObject *) rpc.message->content.properties);
    if (!properties && rpc.message->content.properties) return (kIOReturnBadArgument);

    ret = (*func)(target,
        properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_UserSetProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_UserSetProperties_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::SendIOMessageServicePropertyChange(
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_SendIOMessageServicePropertyChange_Msg msg;
        struct
        {
            IOService_SendIOMessageServicePropertyChange_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_SendIOMessageServicePropertyChange_Msg * msg = &buf.msg;
    struct IOService_SendIOMessageServicePropertyChange_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_SendIOMessageServicePropertyChange_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SendIOMessageServicePropertyChange_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SendIOMessageServicePropertyChange_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_SendIOMessageServicePropertyChange_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_SendIOMessageServicePropertyChange_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::SendIOMessageServicePropertyChange_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SendIOMessageServicePropertyChange_Handler func)
{
    IOService_SendIOMessageServicePropertyChange_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SendIOMessageServicePropertyChange_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SendIOMessageServicePropertyChange_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_SendIOMessageServicePropertyChange_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::RemoveProperty(
        OSString * propertyName,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_RemoveProperty_Msg msg;
        struct
        {
            IOService_RemoveProperty_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_RemoveProperty_Msg * msg = &buf.msg;
    struct IOService_RemoveProperty_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_RemoveProperty_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_RemoveProperty_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_RemoveProperty_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->propertyName__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.propertyName = (OSObjectRef) propertyName;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_RemoveProperty_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_RemoveProperty_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::RemoveProperty_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        RemoveProperty_Handler func)
{
    IOService_RemoveProperty_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * propertyName;

    if (IOService_RemoveProperty_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    propertyName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.propertyName);
    if (!propertyName && rpc.message->content.propertyName) return (kIOReturnBadArgument);

    ret = (*func)(target,
        propertyName);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_RemoveProperty_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_RemoveProperty_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::CopySystemStateNotificationService(
        IOService ** service,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CopySystemStateNotificationService_Msg msg;
        struct
        {
            IOService_CopySystemStateNotificationService_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CopySystemStateNotificationService_Msg * msg = &buf.msg;
    struct IOService_CopySystemStateNotificationService_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CopySystemStateNotificationService_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CopySystemStateNotificationService_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CopySystemStateNotificationService_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CopySystemStateNotificationService_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_CopySystemStateNotificationService_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *service = OSDynamicCast(IOService, (OSObject *) rpl->content.service);
        if (rpl->content.service && !*service) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::CopySystemStateNotificationService_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopySystemStateNotificationService_Handler func)
{
    IOService_CopySystemStateNotificationService_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CopySystemStateNotificationService_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (IOService **)&rpc.reply->content.service);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopySystemStateNotificationService_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopySystemStateNotificationService_Rpl_ObjRefs;
    rpc.reply->service__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::StateNotificationItemCreate(
        OSString * itemName,
        OSDictionary * value,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_StateNotificationItemCreate_Msg msg;
        struct
        {
            IOService_StateNotificationItemCreate_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_StateNotificationItemCreate_Msg * msg = &buf.msg;
    struct IOService_StateNotificationItemCreate_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_StateNotificationItemCreate_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_StateNotificationItemCreate_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_StateNotificationItemCreate_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->itemName__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.itemName = (OSObjectRef) itemName;

    msg->value__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.value = (OSObjectRef) value;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_StateNotificationItemCreate_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_StateNotificationItemCreate_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::StateNotificationItemCreate_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationItemCreate_Handler func)
{
    IOService_StateNotificationItemCreate_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * itemName;
    OSDictionary * value;

    if (IOService_StateNotificationItemCreate_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    itemName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.itemName);
    if (!itemName && rpc.message->content.itemName) return (kIOReturnBadArgument);
    value = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.value);
    if (!value && rpc.message->content.value) return (kIOReturnBadArgument);

    ret = (*func)(target,
        itemName,
        value);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_StateNotificationItemCreate_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_StateNotificationItemCreate_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::StateNotificationItemSet(
        OSString * itemName,
        OSDictionary * value,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_StateNotificationItemSet_Msg msg;
        struct
        {
            IOService_StateNotificationItemSet_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_StateNotificationItemSet_Msg * msg = &buf.msg;
    struct IOService_StateNotificationItemSet_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_StateNotificationItemSet_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_StateNotificationItemSet_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_StateNotificationItemSet_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->itemName__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.itemName = (OSObjectRef) itemName;

    msg->value__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.value = (OSObjectRef) value;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_StateNotificationItemSet_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_StateNotificationItemSet_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::StateNotificationItemSet_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationItemSet_Handler func)
{
    IOService_StateNotificationItemSet_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * itemName;
    OSDictionary * value;

    if (IOService_StateNotificationItemSet_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    itemName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.itemName);
    if (!itemName && rpc.message->content.itemName) return (kIOReturnBadArgument);
    value = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.value);
    if (!value && rpc.message->content.value) return (kIOReturnBadArgument);

    ret = (*func)(target,
        itemName,
        value);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_StateNotificationItemSet_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_StateNotificationItemSet_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::StateNotificationItemCopy(
        OSString * itemName,
        OSDictionary ** value,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_StateNotificationItemCopy_Msg msg;
        struct
        {
            IOService_StateNotificationItemCopy_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_StateNotificationItemCopy_Msg * msg = &buf.msg;
    struct IOService_StateNotificationItemCopy_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_StateNotificationItemCopy_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_StateNotificationItemCopy_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_StateNotificationItemCopy_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->itemName__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.itemName = (OSObjectRef) itemName;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_StateNotificationItemCopy_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService_StateNotificationItemCopy_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *value = OSDynamicCast(OSDictionary, (OSObject *) rpl->content.value);
        if (rpl->content.value && !*value) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::StateNotificationItemCopy_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationItemCopy_Handler func)
{
    IOService_StateNotificationItemCopy_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * itemName;

    if (IOService_StateNotificationItemCopy_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    itemName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.itemName);
    if (!itemName && rpc.message->content.itemName) return (kIOReturnBadArgument);

    ret = (*func)(target,
        itemName,
        (OSDictionary **)&rpc.reply->content.value);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_StateNotificationItemCopy_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_StateNotificationItemCopy_Rpl_ObjRefs;
    rpc.reply->value__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOService::CreatePMAssertion(
        uint32_t assertionBits,
        uint64_t * assertionID,
        bool synced,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_CreatePMAssertion_Msg msg;
        struct
        {
            IOService_CreatePMAssertion_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_CreatePMAssertion_Msg * msg = &buf.msg;
    struct IOService_CreatePMAssertion_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_CreatePMAssertion_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_CreatePMAssertion_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_CreatePMAssertion_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.assertionBits = assertionBits;

    msg->content.synced = synced;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_CreatePMAssertion_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_CreatePMAssertion_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (assertionID) *assertionID = rpl->content.assertionID;
    }

    return (ret);
}

kern_return_t
IOService::CreatePMAssertion_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CreatePMAssertion_Handler func)
{
    IOService_CreatePMAssertion_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CreatePMAssertion_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.assertionBits,
        &rpc.reply->content.assertionID,
        rpc.message->content.synced);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CreatePMAssertion_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_CreatePMAssertion_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOService::ReleasePMAssertion(
        uint64_t assertionID,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_ReleasePMAssertion_Msg msg;
        struct
        {
            IOService_ReleasePMAssertion_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_ReleasePMAssertion_Msg * msg = &buf.msg;
    struct IOService_ReleasePMAssertion_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_ReleasePMAssertion_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_ReleasePMAssertion_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_ReleasePMAssertion_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.assertionID = assertionID;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_ReleasePMAssertion_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_ReleasePMAssertion_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOService::ReleasePMAssertion_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ReleasePMAssertion_Handler func)
{
    IOService_ReleasePMAssertion_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_ReleasePMAssertion_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.assertionID);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_ReleasePMAssertion_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOService_ReleasePMAssertion_Rpl_ObjRefs;

    return (ret);
}

void
IOService::Stop_async(
        IOService * provider,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Stop_async_Msg msg;
        struct
        {
            IOService_Stop_async_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService_Stop_async_Msg * msg = &buf.msg;
    struct IOService_Stop_async_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService_Stop_async_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_Stop_async_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_Stop_async_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService_Stop_async_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOService_Stop_async_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    (void) ret;
}

kern_return_t
IOService::Stop_async_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Stop_async_Handler func)
{
    IOService_Stop_async_Invocation rpc = { _rpc };
    IOService * provider;

    if (IOService_Stop_async_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);

    (*func)(target,
        provider);

    return (kIOReturnSuccess);
}

kern_return_t
IOService::_NewUserClient(
        uint32_t type,
        OSDictionary * entitlements,
        IOUserClient ** userClient,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService__NewUserClient_Msg msg;
        struct
        {
            IOService__NewUserClient_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOService__NewUserClient_Msg * msg = &buf.msg;
    struct IOService__NewUserClient_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOService__NewUserClient_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService__NewUserClient_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService__NewUserClient_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->entitlements__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.entitlements = (OSObjectRef) entitlements;

    msg->content.type = type;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOService__NewUserClient_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOService__NewUserClient_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *userClient = OSDynamicCast(IOUserClient, (OSObject *) rpl->content.userClient);
        if (rpl->content.userClient && !*userClient) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOService::_NewUserClient_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _NewUserClient_Handler func)
{
    IOService__NewUserClient_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSDictionary * entitlements;

    if (IOService__NewUserClient_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    entitlements = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.entitlements);
    if (!entitlements && rpc.message->content.entitlements) return (kIOReturnBadArgument);

    ret = (*func)(target,
        rpc.message->content.type,
        entitlements,
        (IOUserClient **)&rpc.reply->content.userClient);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService__NewUserClient_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService__NewUserClient_Rpl_ObjRefs;
    rpc.reply->userClient__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

