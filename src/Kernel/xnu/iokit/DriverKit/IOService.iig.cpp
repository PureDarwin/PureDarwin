/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IOService.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IOService.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IOService.h>

/* @iig implementation */
#include <DriverKit/IOUserClient.h>
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
    unsigned long long  registryEntryID;
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
    OSDictionary * properties;
#if !defined(__LP64__)
    uint32_t __propertiesPad;
#endif /* !defined(__LP64__) */
};
#pragma pack(4)
struct IOService_CopyProperties_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_ool_descriptor_t  properties__descriptor;
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
    uint64_t  options;
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
    OSContainer * property;
#if !defined(__LP64__)
    uint32_t __propertyPad;
#endif /* !defined(__LP64__) */
};
#pragma pack(4)
struct IOService_SearchProperty_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_ool_descriptor_t  property__descriptor;
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
    OSDictionary * properties;
#if !defined(__LP64__)
    uint32_t __propertiesPad;
#endif /* !defined(__LP64__) */
};
#pragma pack(4)
struct IOService_SetProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_ool_descriptor_t  properties__descriptor;
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
    OSArray * propertyKeys;
#if !defined(__LP64__)
    uint32_t __propertyKeysPad;
#endif /* !defined(__LP64__) */
};
#pragma pack(4)
struct IOService_CopyProviderProperties_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_ool_descriptor_t  propertyKeys__descriptor;
    IOService_CopyProviderProperties_Msg_Content content;
};
#pragma pack()
#define IOService_CopyProviderProperties_Msg_ObjRefs (2)

struct IOService_CopyProviderProperties_Rpl_Content
{
    IORPCMessage __hdr;
    OSArray * properties;
#if !defined(__LP64__)
    uint32_t __propertiesPad;
#endif /* !defined(__LP64__) */
};
#pragma pack(4)
struct IOService_CopyProviderProperties_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_ool_descriptor_t  properties__descriptor;
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
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gIOMemoryDescriptorMetaClass;
extern OSMetaClass * gIOBufferMemoryDescriptorMetaClass;
extern OSMetaClass * gIOUserClientMetaClass;
extern OSMetaClass * gOSActionMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IOService_QueueNames  ""

#define IOService_MethodNames  ""

#define IOServiceMetaClass_MethodNames  ""

struct OSClassDescription_IOService_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IOService_QueueNames)];
    char               methodNames[sizeof(IOService_MethodNames)];
    char               metaMethodNames[sizeof(IOServiceMetaClass_MethodNames)];
};

const struct OSClassDescription_IOService_t
OSClassDescription_IOService =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IOService_t),
        .name                    = "IOService",
        .superName               = "OSObject",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IOService_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IOService_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IOService_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IOService_t, queueNames),
        .methodNamesSize         = sizeof(IOService_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IOService_t, methodNames),
        .metaMethodNamesSize     = sizeof(IOServiceMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IOService_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IOService_QueueNames,
    .methodNames     = IOService_MethodNames,
    .metaMethodNames = IOServiceMetaClass_MethodNames,
};

OSMetaClass * gIOServiceMetaClass;

static kern_return_t
IOService_New(OSMetaClass * instance);

const OSClassLoadInformation
IOService_Class = 
{
    .description       = &OSClassDescription_IOService.base,
    .metaPointer       = &gIOServiceMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IOService),

    .New               = &IOService_New,
};

extern const void * const
gIOService_Declaration;
const void * const
gIOService_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IOService_Class;

static kern_return_t
IOService_New(OSMetaClass * instance)
{
    if (!new(instance) IOServiceMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IOServiceMetaClass::New(OSObject * instance)
{
    if (!new(instance) IOService) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

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
        case IOService_Start_ID:
        {
            ret = IOService::Start_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Start_Handler, *self, &IOService::Start_Impl));
            break;
        }
        case IOService_Stop_ID:
        {
            ret = IOService::Stop_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Stop_Handler, *self, &IOService::Stop_Impl));
            break;
        }
#if KERNEL
        case IOService_ClientCrashed_ID:
        {
            ret = IOService::ClientCrashed_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ClientCrashed_Handler, *self, &IOService::ClientCrashed_Impl));
            break;
        }
#endif /* !KERNEL */
        case IOService_GetRegistryEntryID_ID:
        {
            ret = IOService::GetRegistryEntryID_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::GetRegistryEntryID_Handler, *self, &IOService::GetRegistryEntryID_Impl));
            break;
        }
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
        case OSObject_SetDispatchQueue_ID:
        {
            ret = OSObject::SetDispatchQueue_Invoke(rpc, self, SimpleMemberFunctionCast(OSObject::SetDispatchQueue_Handler, *self, &IOService::SetDispatchQueue_Impl));
            break;
        }
#if KERNEL
        case OSObject_CopyDispatchQueue_ID:
        {
            ret = OSObject::CopyDispatchQueue_Invoke(rpc, self, SimpleMemberFunctionCast(OSObject::CopyDispatchQueue_Handler, *self, &IOService::CopyDispatchQueue_Impl));
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
        case IOService_SetPowerState_ID:
        {
            ret = IOService::SetPowerState_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::SetPowerState_Handler, *self, &IOService::SetPowerState_Impl));
            break;
        }
#if KERNEL
        case IOService_ChangePowerState_ID:
        {
            ret = IOService::ChangePowerState_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::ChangePowerState_Handler, *self, &IOService::ChangePowerState_Impl));
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
        case IOService_Create_ID:
        {
            ret = IOService::Create_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Create_Handler, *self, &IOService::Create_Impl));
            break;
        }
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
        case IOService_Stop_async_ID:
        {
            ret = IOService::Stop_async_Invoke(rpc, self, SimpleMemberFunctionCast(IOService::Stop_async_Handler, *self, &IOService::Stop_async_Impl));
            break;
        }

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
#else /* KERNEL */
kern_return_t
IOServiceMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
IOService::RegisterService(        OSDispatchMethod supermethod)
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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    msg->content.plane = NULL;

    strlcpy(&msg->content.__plane[0], plane, sizeof(msg->content.__plane));

    msg->content.options = options;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOService_SetProperties_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOService_SetProperties_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->properties__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    msg->properties__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    msg->properties__descriptor.address = (void *) __builtin_offsetof(IOService_SetProperties_Msg_Content, properties);
    msg->content.properties = properties;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    msg->propertyKeys__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    msg->propertyKeys__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    msg->propertyKeys__descriptor.address = (void *) __builtin_offsetof(IOService_CopyProviderProperties_Msg_Content, propertyKeys);
    msg->content.propertyKeys = propertyKeys;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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

void
IOService::Stop_async(
        IOService * provider,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOService_Stop_async_Msg msg;
    } buf;
    struct IOService_Stop_async_Msg * msg = &buf.msg;

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
IOService::CopyProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyProperties_Handler func)
{
    IOService_CopyProperties_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CopyProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopyProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopyProperties_Rpl_ObjRefs;
    rpc.reply->properties__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    rpc.reply->properties__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    rpc.reply->properties__descriptor.address = (void *) __builtin_offsetof(IOService_CopyProperties_Rpl_Content, properties);
    rpc.reply->properties__descriptor.size = 0;

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
        &rpc.reply->content.property);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_SearchProperty_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_SearchProperty_Rpl_ObjRefs;
    rpc.reply->property__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    rpc.reply->property__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    rpc.reply->property__descriptor.address = (void *) __builtin_offsetof(IOService_SearchProperty_Rpl_Content, property);
    rpc.reply->property__descriptor.size = 0;

    return (ret);
}

kern_return_t
IOService::SetProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetProperties_Handler func)
{
    IOService_SetProperties_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_SetProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.properties);

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
IOService::CopyProviderProperties_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyProviderProperties_Handler func)
{
    IOService_CopyProviderProperties_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOService_CopyProviderProperties_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.propertyKeys,
        &rpc.reply->content.properties);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOService_CopyProviderProperties_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOService_CopyProviderProperties_Rpl_ObjRefs;
    rpc.reply->properties__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    rpc.reply->properties__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    rpc.reply->properties__descriptor.address = (void *) __builtin_offsetof(IOService_CopyProviderProperties_Rpl_Content, properties);
    rpc.reply->properties__descriptor.size = 0;

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



