/* iig-lite generated from IOInterruptDispatchSource.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOInterruptDispatchSource.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOInterruptDispatchSource.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndStructors(IOInterruptDispatchSource, IODispatchSource)
#endif /* KERNEL */

struct IOInterruptDispatchSource_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
    OSObjectRef  queue;
    uint32_t  index;
};
#pragma pack(4)
struct IOInterruptDispatchSource_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    mach_msg_port_descriptor_t queue__descriptor;
    IOInterruptDispatchSource_Create_Msg_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_Create_Msg_ObjRefs (3)

struct IOInterruptDispatchSource_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  source;
};
#pragma pack(4)
struct IOInterruptDispatchSource_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t source__descriptor;
    IOInterruptDispatchSource_Create_Rpl_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOInterruptDispatchSource_Create_Msg * message;
        struct IOInterruptDispatchSource_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOInterruptDispatchSource_Create_Invocation;
struct IOInterruptDispatchSource_GetInterruptType_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  provider;
    uint32_t  index;
};
#pragma pack(4)
struct IOInterruptDispatchSource_GetInterruptType_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t provider__descriptor;
    IOInterruptDispatchSource_GetInterruptType_Msg_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_GetInterruptType_Msg_ObjRefs (2)

struct IOInterruptDispatchSource_GetInterruptType_Rpl_Content
{
    IORPCMessage __hdr;
    uint64_t  interruptType;
};
#pragma pack(4)
struct IOInterruptDispatchSource_GetInterruptType_Rpl
{
    IORPCMessageMach           mach;
    IOInterruptDispatchSource_GetInterruptType_Rpl_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_GetInterruptType_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOInterruptDispatchSource_GetInterruptType_Msg * message;
        struct IOInterruptDispatchSource_GetInterruptType_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOInterruptDispatchSource_GetInterruptType_Invocation;
struct IOInterruptDispatchSource_SetHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IOInterruptDispatchSource_SetHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOInterruptDispatchSource_SetHandler_Msg_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_SetHandler_Msg_ObjRefs (2)

struct IOInterruptDispatchSource_SetHandler_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOInterruptDispatchSource_SetHandler_Rpl
{
    IORPCMessageMach           mach;
    IOInterruptDispatchSource_SetHandler_Rpl_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_SetHandler_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOInterruptDispatchSource_SetHandler_Msg * message;
        struct IOInterruptDispatchSource_SetHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOInterruptDispatchSource_SetHandler_Invocation;
struct IOInterruptDispatchSource_GetLastInterrupt_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOInterruptDispatchSource_GetLastInterrupt_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOInterruptDispatchSource_GetLastInterrupt_Msg_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_GetLastInterrupt_Msg_ObjRefs (1)

struct IOInterruptDispatchSource_GetLastInterrupt_Rpl_Content
{
    IORPCMessage __hdr;
    uint64_t  count;
    uint64_t  time;
};
#pragma pack(4)
struct IOInterruptDispatchSource_GetLastInterrupt_Rpl
{
    IORPCMessageMach           mach;
    IOInterruptDispatchSource_GetLastInterrupt_Rpl_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_GetLastInterrupt_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOInterruptDispatchSource_GetLastInterrupt_Msg * message;
        struct IOInterruptDispatchSource_GetLastInterrupt_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOInterruptDispatchSource_GetLastInterrupt_Invocation;
struct IOInterruptDispatchSource_InterruptOccurred_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
    uint64_t  count;
    uint64_t  time;
};
#pragma pack(4)
struct IOInterruptDispatchSource_InterruptOccurred_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOInterruptDispatchSource_InterruptOccurred_Msg_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_InterruptOccurred_Msg_ObjRefs (2)

struct IOInterruptDispatchSource_InterruptOccurred_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOInterruptDispatchSource_InterruptOccurred_Rpl
{
    IORPCMessageMach           mach;
    IOInterruptDispatchSource_InterruptOccurred_Rpl_Content content;
};
#pragma pack()
#define IOInterruptDispatchSource_InterruptOccurred_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOInterruptDispatchSource_InterruptOccurred_Msg * message;
        struct IOInterruptDispatchSource_InterruptOccurred_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOInterruptDispatchSource_InterruptOccurred_Invocation;
kern_return_t
IOInterruptDispatchSource::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOInterruptDispatchSource::_Dispatch(IOInterruptDispatchSource * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOInterruptDispatchSource_SetHandler_ID:
        {
            ret = IOInterruptDispatchSource::SetHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IOInterruptDispatchSource::SetHandler_Handler, *self, &IOInterruptDispatchSource::SetHandler_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_SetEnableWithCompletion_ID:
        {
            ret = IODispatchSource::SetEnableWithCompletion_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::SetEnableWithCompletion_Handler, *self, &IOInterruptDispatchSource::SetEnableWithCompletion_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_Cancel_ID:
        {
            ret = IODispatchSource::Cancel_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::Cancel_Handler, *self, &IOInterruptDispatchSource::Cancel_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOInterruptDispatchSource_GetLastInterrupt_ID:
        {
            ret = IOInterruptDispatchSource::GetLastInterrupt_Invoke(rpc, self, SimpleMemberFunctionCast(IOInterruptDispatchSource::GetLastInterrupt_Handler, *self, &IOInterruptDispatchSource::GetLastInterrupt_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_CheckForWork_ID:
        {
            ret = IODispatchSource::CheckForWork_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::CheckForWork_Handler, *self, &IOInterruptDispatchSource::CheckForWork_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOInterruptDispatchSource_InterruptOccurred_ID:
        {
            ret = IOInterruptDispatchSource::InterruptOccurred_Invoke(rpc, self, SimpleMemberFunctionCast(IOInterruptDispatchSource::InterruptOccurred_Handler, *self, &IOInterruptDispatchSource::InterruptOccurred_Impl));
            break;
        }
#endif /* !KERNEL */

        default:
            ret = IODispatchSource::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IOInterruptDispatchSource::MetaClass::Dispatch(const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
        case IOInterruptDispatchSource_GetInterruptType_ID:
            ret = IOInterruptDispatchSource::GetInterruptType_Invoke(rpc, &IOInterruptDispatchSource::GetInterruptType_Impl);
            break;

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}
#endif /* KERNEL */

kern_return_t
IOInterruptDispatchSource::Create(
        IOService * provider,
        uint32_t index,
        IODispatchQueue * queue,
        IOInterruptDispatchSource ** source)
{
    kern_return_t ret;
    union
    {
        IOInterruptDispatchSource_Create_Msg msg;
        struct
        {
            IOInterruptDispatchSource_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOInterruptDispatchSource_Create_Msg * msg = &buf.msg;
    struct IOInterruptDispatchSource_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOInterruptDispatchSource_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOInterruptDispatchSource_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOInterruptDispatchSource);
    msg->content.__hdr.objectRefs = IOInterruptDispatchSource_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    msg->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.queue = (OSObjectRef) queue;

    msg->content.index = index;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOInterruptDispatchSource)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOInterruptDispatchSource_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOInterruptDispatchSource_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *source = OSDynamicCast(IOInterruptDispatchSource, (OSObject *) rpl->content.source);
        if (rpl->content.source && !*source) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOInterruptDispatchSource_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * provider;
    IODispatchQueue * queue;

    if (IOInterruptDispatchSource_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);
    queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpc.message->content.queue);
    if (!queue && rpc.message->content.queue) return (kIOReturnBadArgument);

    ret = (*func)(provider,
        rpc.message->content.index,
        queue,
        (IOInterruptDispatchSource **)&rpc.reply->content.source);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOInterruptDispatchSource_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOInterruptDispatchSource_Create_Rpl_ObjRefs;
    rpc.reply->source__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::GetInterruptType(
        IOService * provider,
        uint32_t index,
        uint64_t * interruptType)
{
    kern_return_t ret;
    union
    {
        IOInterruptDispatchSource_GetInterruptType_Msg msg;
        struct
        {
            IOInterruptDispatchSource_GetInterruptType_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOInterruptDispatchSource_GetInterruptType_Msg * msg = &buf.msg;
    struct IOInterruptDispatchSource_GetInterruptType_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOInterruptDispatchSource_GetInterruptType_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOInterruptDispatchSource_GetInterruptType_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOInterruptDispatchSource);
    msg->content.__hdr.objectRefs = IOInterruptDispatchSource_GetInterruptType_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->provider__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.provider = (OSObjectRef) provider;

    msg->content.index = index;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOInterruptDispatchSource)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOInterruptDispatchSource_GetInterruptType_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOInterruptDispatchSource_GetInterruptType_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (interruptType) *interruptType = rpl->content.interruptType;
    }

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::GetInterruptType_Invoke(const IORPC _rpc,
        GetInterruptType_Handler func)
{
    IOInterruptDispatchSource_GetInterruptType_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * provider;

    if (IOInterruptDispatchSource_GetInterruptType_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    provider = OSDynamicCast(IOService, (OSObject *) rpc.message->content.provider);
    if (!provider && rpc.message->content.provider) return (kIOReturnBadArgument);

    ret = (*func)(provider,
        rpc.message->content.index,
        &rpc.reply->content.interruptType);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOInterruptDispatchSource_GetInterruptType_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOInterruptDispatchSource_GetInterruptType_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::SetHandler(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOInterruptDispatchSource_SetHandler_Msg msg;
        struct
        {
            IOInterruptDispatchSource_SetHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOInterruptDispatchSource_SetHandler_Msg * msg = &buf.msg;
    struct IOInterruptDispatchSource_SetHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOInterruptDispatchSource_SetHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOInterruptDispatchSource_SetHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOInterruptDispatchSource_SetHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOInterruptDispatchSource_SetHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOInterruptDispatchSource_SetHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::SetHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetHandler_Handler func)
{
    IOInterruptDispatchSource_SetHandler_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSAction * action;

    if (IOInterruptDispatchSource_SetHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    ret = (*func)(target,
        action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOInterruptDispatchSource_SetHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOInterruptDispatchSource_SetHandler_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::GetLastInterrupt(
        uint64_t * count,
        uint64_t * time,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOInterruptDispatchSource_GetLastInterrupt_Msg msg;
        struct
        {
            IOInterruptDispatchSource_GetLastInterrupt_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOInterruptDispatchSource_GetLastInterrupt_Msg * msg = &buf.msg;
    struct IOInterruptDispatchSource_GetLastInterrupt_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOInterruptDispatchSource_GetLastInterrupt_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOInterruptDispatchSource_GetLastInterrupt_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOInterruptDispatchSource_GetLastInterrupt_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOInterruptDispatchSource_GetLastInterrupt_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOInterruptDispatchSource_GetLastInterrupt_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (count) *count = rpl->content.count;
        if (time) *time = rpl->content.time;
    }

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::GetLastInterrupt_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        GetLastInterrupt_Handler func)
{
    IOInterruptDispatchSource_GetLastInterrupt_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOInterruptDispatchSource_GetLastInterrupt_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.count,
        &rpc.reply->content.time);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOInterruptDispatchSource_GetLastInterrupt_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOInterruptDispatchSource_GetLastInterrupt_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::InterruptOccurred(
        IORPC rpc,
        OSAction * action,
        uint64_t count,
        uint64_t time,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    struct IOInterruptDispatchSource_InterruptOccurred_Msg * msg = (typeof(msg)) rpc.reply;

    memset(msg, 0, sizeof(struct IOInterruptDispatchSource_InterruptOccurred_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 1*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOInterruptDispatchSource_InterruptOccurred_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IOInterruptDispatchSource_InterruptOccurred_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    msg->content.count = count;

    msg->content.time = time;


    ret = kIOReturnSuccess;

    return (ret);
}

kern_return_t
IOInterruptDispatchSource::InterruptOccurred_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        InterruptOccurred_Handler func)
{
    return IOInterruptDispatchSource::InterruptOccurred_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IOInterruptDispatchSource::InterruptOccurred_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        InterruptOccurred_Handler func,
        const OSMetaClass * targetActionClass)
{
    IOInterruptDispatchSource_InterruptOccurred_Invocation rpc = { _rpc };
    OSAction * action;

    if (IOInterruptDispatchSource_InterruptOccurred_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (targetActionClass) {
        action = (OSAction *) OSMetaClassBase::safeMetaCast((OSObject *) rpc.message->content.action, targetActionClass);
    } else {
        action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    }
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    (*func)(target,
        action,
        rpc.message->content.count,
        rpc.message->content.time);

    return (kIOReturnSuccess);
}

