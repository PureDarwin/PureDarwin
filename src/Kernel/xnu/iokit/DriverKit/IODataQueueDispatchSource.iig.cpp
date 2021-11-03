/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IODataQueueDispatchSource.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IODataQueueDispatchSource.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IODataQueueDispatchSource.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


struct IODataQueueDispatchSource_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  queue;
    uint64_t  queueByteCount;
};
#pragma pack(4)
struct IODataQueueDispatchSource_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t queue__descriptor;
    IODataQueueDispatchSource_Create_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_Create_Msg_ObjRefs (2)

struct IODataQueueDispatchSource_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  source;
};
#pragma pack(4)
struct IODataQueueDispatchSource_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t source__descriptor;
    IODataQueueDispatchSource_Create_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_Create_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_Create_Msg * message;
        struct IODataQueueDispatchSource_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_Create_Invocation;
struct IODataQueueDispatchSource_SetDataAvailableHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_SetDataAvailableHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_SetDataAvailableHandler_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_SetDataAvailableHandler_Msg_ObjRefs (2)

struct IODataQueueDispatchSource_SetDataAvailableHandler_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODataQueueDispatchSource_SetDataAvailableHandler_Rpl
{
    IORPCMessageMach           mach;
    IODataQueueDispatchSource_SetDataAvailableHandler_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_SetDataAvailableHandler_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_SetDataAvailableHandler_Msg * message;
        struct IODataQueueDispatchSource_SetDataAvailableHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_SetDataAvailableHandler_Invocation;
struct IODataQueueDispatchSource_SetDataServicedHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_SetDataServicedHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_SetDataServicedHandler_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_SetDataServicedHandler_Msg_ObjRefs (2)

struct IODataQueueDispatchSource_SetDataServicedHandler_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODataQueueDispatchSource_SetDataServicedHandler_Rpl
{
    IORPCMessageMach           mach;
    IODataQueueDispatchSource_SetDataServicedHandler_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_SetDataServicedHandler_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_SetDataServicedHandler_Msg * message;
        struct IODataQueueDispatchSource_SetDataServicedHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_SetDataServicedHandler_Invocation;
struct IODataQueueDispatchSource_CopyMemory_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyMemory_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODataQueueDispatchSource_CopyMemory_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyMemory_Msg_ObjRefs (1)

struct IODataQueueDispatchSource_CopyMemory_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyMemory_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IODataQueueDispatchSource_CopyMemory_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyMemory_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_CopyMemory_Msg * message;
        struct IODataQueueDispatchSource_CopyMemory_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_CopyMemory_Invocation;
struct IODataQueueDispatchSource_CopyDataAvailableHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyDataAvailableHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODataQueueDispatchSource_CopyDataAvailableHandler_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyDataAvailableHandler_Msg_ObjRefs (1)

struct IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_CopyDataAvailableHandler_Msg * message;
        struct IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_CopyDataAvailableHandler_Invocation;
struct IODataQueueDispatchSource_CopyDataServicedHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyDataServicedHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODataQueueDispatchSource_CopyDataServicedHandler_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyDataServicedHandler_Msg_ObjRefs (1)

struct IODataQueueDispatchSource_CopyDataServicedHandler_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_CopyDataServicedHandler_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_CopyDataServicedHandler_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_CopyDataServicedHandler_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_CopyDataServicedHandler_Msg * message;
        struct IODataQueueDispatchSource_CopyDataServicedHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_CopyDataServicedHandler_Invocation;
struct IODataQueueDispatchSource_DataAvailable_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_DataAvailable_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_DataAvailable_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_DataAvailable_Msg_ObjRefs (2)

struct IODataQueueDispatchSource_DataAvailable_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODataQueueDispatchSource_DataAvailable_Rpl
{
    IORPCMessageMach           mach;
    IODataQueueDispatchSource_DataAvailable_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_DataAvailable_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_DataAvailable_Msg * message;
        struct IODataQueueDispatchSource_DataAvailable_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_DataAvailable_Invocation;
struct IODataQueueDispatchSource_DataServiced_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IODataQueueDispatchSource_DataServiced_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IODataQueueDispatchSource_DataServiced_Msg_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_DataServiced_Msg_ObjRefs (2)

struct IODataQueueDispatchSource_DataServiced_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODataQueueDispatchSource_DataServiced_Rpl
{
    IORPCMessageMach           mach;
    IODataQueueDispatchSource_DataServiced_Rpl_Content content;
};
#pragma pack()
#define IODataQueueDispatchSource_DataServiced_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODataQueueDispatchSource_DataServiced_Msg * message;
        struct IODataQueueDispatchSource_DataServiced_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODataQueueDispatchSource_DataServiced_Invocation;
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gIOServiceMetaClass;
extern OSMetaClass * gIOMemoryMapMetaClass;
#endif /* !KERNEL */

#if KERNEL
OSDefineMetaClassAndStructors(IODataQueueDispatchSource, IODispatchSource);
#endif /* KERNEL */

#if !KERNEL

#define IODataQueueDispatchSource_QueueNames  ""

#define IODataQueueDispatchSource_MethodNames  ""

#define IODataQueueDispatchSourceMetaClass_MethodNames  ""

struct OSClassDescription_IODataQueueDispatchSource_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IODataQueueDispatchSource_QueueNames)];
    char               methodNames[sizeof(IODataQueueDispatchSource_MethodNames)];
    char               metaMethodNames[sizeof(IODataQueueDispatchSourceMetaClass_MethodNames)];
};

const struct OSClassDescription_IODataQueueDispatchSource_t
OSClassDescription_IODataQueueDispatchSource =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IODataQueueDispatchSource_t),
        .name                    = "IODataQueueDispatchSource",
        .superName               = "IODispatchSource",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IODataQueueDispatchSource_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IODataQueueDispatchSource_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IODataQueueDispatchSource_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IODataQueueDispatchSource_t, queueNames),
        .methodNamesSize         = sizeof(IODataQueueDispatchSource_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IODataQueueDispatchSource_t, methodNames),
        .metaMethodNamesSize     = sizeof(IODataQueueDispatchSourceMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IODataQueueDispatchSource_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IODataQueueDispatchSource_QueueNames,
    .methodNames     = IODataQueueDispatchSource_MethodNames,
    .metaMethodNames = IODataQueueDispatchSourceMetaClass_MethodNames,
};

OSMetaClass * gIODataQueueDispatchSourceMetaClass;

static kern_return_t
IODataQueueDispatchSource_New(OSMetaClass * instance);

const OSClassLoadInformation
IODataQueueDispatchSource_Class = 
{
    .description       = &OSClassDescription_IODataQueueDispatchSource.base,
    .metaPointer       = &gIODataQueueDispatchSourceMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IODataQueueDispatchSource),

    .New               = &IODataQueueDispatchSource_New,
};

extern const void * const
gIODataQueueDispatchSource_Declaration;
const void * const
gIODataQueueDispatchSource_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IODataQueueDispatchSource_Class;

static kern_return_t
IODataQueueDispatchSource_New(OSMetaClass * instance)
{
    if (!new(instance) IODataQueueDispatchSourceMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IODataQueueDispatchSourceMetaClass::New(OSObject * instance)
{
    if (!new(instance) IODataQueueDispatchSource) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
IODataQueueDispatchSource::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IODataQueueDispatchSource::_Dispatch(IODataQueueDispatchSource * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODataQueueDispatchSource_SetDataAvailableHandler_ID:
        {
            ret = IODataQueueDispatchSource::SetDataAvailableHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IODataQueueDispatchSource::SetDataAvailableHandler_Handler, *self, &IODataQueueDispatchSource::SetDataAvailableHandler_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODataQueueDispatchSource_SetDataServicedHandler_ID:
        {
            ret = IODataQueueDispatchSource::SetDataServicedHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IODataQueueDispatchSource::SetDataServicedHandler_Handler, *self, &IODataQueueDispatchSource::SetDataServicedHandler_Impl));
            break;
        }
#endif /* !KERNEL */
        case IODispatchSource_SetEnableWithCompletion_ID:
        {
            ret = IODispatchSource::SetEnableWithCompletion_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::SetEnableWithCompletion_Handler, *self, &IODataQueueDispatchSource::SetEnableWithCompletion_Impl));
            break;
        }
        case IODispatchSource_Cancel_ID:
        {
            ret = IODispatchSource::Cancel_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::Cancel_Handler, *self, &IODataQueueDispatchSource::Cancel_Impl));
            break;
        }
#if KERNEL
        case IODataQueueDispatchSource_CopyMemory_ID:
        {
            ret = IODataQueueDispatchSource::CopyMemory_Invoke(rpc, self, SimpleMemberFunctionCast(IODataQueueDispatchSource::CopyMemory_Handler, *self, &IODataQueueDispatchSource::CopyMemory_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODataQueueDispatchSource_CopyDataAvailableHandler_ID:
        {
            ret = IODataQueueDispatchSource::CopyDataAvailableHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IODataQueueDispatchSource::CopyDataAvailableHandler_Handler, *self, &IODataQueueDispatchSource::CopyDataAvailableHandler_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODataQueueDispatchSource_CopyDataServicedHandler_ID:
        {
            ret = IODataQueueDispatchSource::CopyDataServicedHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IODataQueueDispatchSource::CopyDataServicedHandler_Handler, *self, &IODataQueueDispatchSource::CopyDataServicedHandler_Impl));
            break;
        }
#endif /* !KERNEL */
        case IODispatchSource_CheckForWork_ID:
        {
            ret = IODispatchSource::CheckForWork_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::CheckForWork_Handler, *self, &IODataQueueDispatchSource::CheckForWork_Impl));
            break;
        }

        default:
            ret = IODispatchSource::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IODataQueueDispatchSource::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
IODataQueueDispatchSourceMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODataQueueDispatchSource_Create_ID:
            ret = IODataQueueDispatchSource::Create_Invoke(rpc, &IODataQueueDispatchSource::Create_Impl);
            break;
#endif /* !KERNEL */

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::Create(
        uint64_t queueByteCount,
        IODispatchQueue * queue,
        IODataQueueDispatchSource ** source)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_Create_Msg msg;
        struct
        {
            IODataQueueDispatchSource_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_Create_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IODataQueueDispatchSource);
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.queueByteCount = queueByteCount;

    msg->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.queue = (OSObjectRef) queue;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IODataQueueDispatchSource)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *source = OSDynamicCast(IODataQueueDispatchSource, (OSObject *) rpl->content.source);
        if (rpl->content.source && !*source) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IODataQueueDispatchSource::SetDataAvailableHandler(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_SetDataAvailableHandler_Msg msg;
        struct
        {
            IODataQueueDispatchSource_SetDataAvailableHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_SetDataAvailableHandler_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_SetDataAvailableHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_SetDataAvailableHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_SetDataAvailableHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_SetDataAvailableHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_SetDataAvailableHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_SetDataAvailableHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }


    return (ret);
}

kern_return_t
IODataQueueDispatchSource::SetDataServicedHandler(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_SetDataServicedHandler_Msg msg;
        struct
        {
            IODataQueueDispatchSource_SetDataServicedHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_SetDataServicedHandler_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_SetDataServicedHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_SetDataServicedHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_SetDataServicedHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_SetDataServicedHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_SetDataServicedHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_SetDataServicedHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }


    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyMemory(
        IOMemoryDescriptor ** memory,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_CopyMemory_Msg msg;
        struct
        {
            IODataQueueDispatchSource_CopyMemory_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_CopyMemory_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_CopyMemory_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_CopyMemory_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_CopyMemory_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyMemory_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_CopyMemory_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_CopyMemory_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyDataAvailableHandler(
        OSAction ** action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_CopyDataAvailableHandler_Msg msg;
        struct
        {
            IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_CopyDataAvailableHandler_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_CopyDataAvailableHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_CopyDataAvailableHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyDataAvailableHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_CopyDataAvailableHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *action = OSDynamicCast(OSAction, (OSObject *) rpl->content.action);
        if (rpl->content.action && !*action) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyDataServicedHandler(
        OSAction ** action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_CopyDataServicedHandler_Msg msg;
        struct
        {
            IODataQueueDispatchSource_CopyDataServicedHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODataQueueDispatchSource_CopyDataServicedHandler_Msg * msg = &buf.msg;
    struct IODataQueueDispatchSource_CopyDataServicedHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_CopyDataServicedHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_CopyDataServicedHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyDataServicedHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODataQueueDispatchSource_CopyDataServicedHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODataQueueDispatchSource_CopyDataServicedHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *action = OSDynamicCast(OSAction, (OSObject *) rpl->content.action);
        if (rpl->content.action && !*action) ret = kIOReturnBadArgument;
    }


    return (ret);
}

void
IODataQueueDispatchSource::DataAvailable(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_DataAvailable_Msg msg;
    } buf;
    struct IODataQueueDispatchSource_DataAvailable_Msg * msg = &buf.msg;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_DataAvailable_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_DataAvailable_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_DataAvailable_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    ret = action->Invoke(rpc);

}

void
IODataQueueDispatchSource::DataServiced(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODataQueueDispatchSource_DataServiced_Msg msg;
    } buf;
    struct IODataQueueDispatchSource_DataServiced_Msg * msg = &buf.msg;

    memset(msg, 0, sizeof(struct IODataQueueDispatchSource_DataServiced_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODataQueueDispatchSource_DataServiced_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IODataQueueDispatchSource_DataServiced_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    ret = action->Invoke(rpc);

}

kern_return_t
IODataQueueDispatchSource::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IODataQueueDispatchSource_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    IODispatchQueue * queue;

    if (IODataQueueDispatchSource_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpc.message->content.queue);
    if (!queue && rpc.message->content.queue) return (kIOReturnBadArgument);

    ret = (*func)(        rpc.message->content.queueByteCount,
        queue,
        (IODataQueueDispatchSource **)&rpc.reply->content.source);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_Create_Rpl_ObjRefs;
    rpc.reply->source__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::SetDataAvailableHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetDataAvailableHandler_Handler func)
{
    IODataQueueDispatchSource_SetDataAvailableHandler_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSAction * action;

    if (IODataQueueDispatchSource_SetDataAvailableHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    ret = (*func)(target,
        action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_SetDataAvailableHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_SetDataAvailableHandler_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::SetDataServicedHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetDataServicedHandler_Handler func)
{
    IODataQueueDispatchSource_SetDataServicedHandler_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSAction * action;

    if (IODataQueueDispatchSource_SetDataServicedHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    ret = (*func)(target,
        action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_SetDataServicedHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_SetDataServicedHandler_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyMemory_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyMemory_Handler func)
{
    IODataQueueDispatchSource_CopyMemory_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODataQueueDispatchSource_CopyMemory_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_CopyMemory_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyMemory_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyDataAvailableHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyDataAvailableHandler_Handler func)
{
    IODataQueueDispatchSource_CopyDataAvailableHandler_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODataQueueDispatchSource_CopyDataAvailableHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (OSAction **)&rpc.reply->content.action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_CopyDataAvailableHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyDataAvailableHandler_Rpl_ObjRefs;
    rpc.reply->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::CopyDataServicedHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyDataServicedHandler_Handler func)
{
    IODataQueueDispatchSource_CopyDataServicedHandler_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODataQueueDispatchSource_CopyDataServicedHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (OSAction **)&rpc.reply->content.action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODataQueueDispatchSource_CopyDataServicedHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODataQueueDispatchSource_CopyDataServicedHandler_Rpl_ObjRefs;
    rpc.reply->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODataQueueDispatchSource::DataAvailable_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        DataAvailable_Handler func)
{
    return IODataQueueDispatchSource::DataAvailable_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IODataQueueDispatchSource::DataAvailable_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        DataAvailable_Handler func,
        const OSMetaClass * targetActionClass)
{
    IODataQueueDispatchSource_DataAvailable_Invocation rpc = { _rpc };
    OSAction * action;

    if (IODataQueueDispatchSource_DataAvailable_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (targetActionClass) {
        action = (OSAction *) OSMetaClassBase::safeMetaCast((OSObject *) rpc.message->content.action, targetActionClass);
    } else {
        action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    }
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    (*func)(target,
        action);


    return (kIOReturnSuccess);
}

kern_return_t
IODataQueueDispatchSource::DataServiced_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        DataServiced_Handler func)
{
    return IODataQueueDispatchSource::DataServiced_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IODataQueueDispatchSource::DataServiced_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        DataServiced_Handler func,
        const OSMetaClass * targetActionClass)
{
    IODataQueueDispatchSource_DataServiced_Invocation rpc = { _rpc };
    OSAction * action;

    if (IODataQueueDispatchSource_DataServiced_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (targetActionClass) {
        action = (OSAction *) OSMetaClassBase::safeMetaCast((OSObject *) rpc.message->content.action, targetActionClass);
    } else {
        action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    }
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    (*func)(target,
        action);


    return (kIOReturnSuccess);
}



