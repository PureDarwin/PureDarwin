/* iig-lite generated from IOServiceStateNotificationDispatchSource.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOServiceStateNotificationDispatchSource.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOServiceStateNotificationDispatchSource.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndAbstractStructors(IOServiceStateNotificationDispatchSource, IODispatchSource)
#endif /* KERNEL */

struct IOServiceStateNotificationDispatchSource_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  service;
    OSObjectRef  items;
    OSObjectRef  queue;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t service__descriptor;
    mach_msg_port_descriptor_t items__descriptor;
    mach_msg_port_descriptor_t queue__descriptor;
    IOServiceStateNotificationDispatchSource_Create_Msg_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_Create_Msg_ObjRefs (4)

struct IOServiceStateNotificationDispatchSource_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  source;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t source__descriptor;
    IOServiceStateNotificationDispatchSource_Create_Rpl_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceStateNotificationDispatchSource_Create_Msg * message;
        struct IOServiceStateNotificationDispatchSource_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceStateNotificationDispatchSource_Create_Invocation;
struct IOServiceStateNotificationDispatchSource_SetHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_SetHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOServiceStateNotificationDispatchSource_SetHandler_Msg_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_SetHandler_Msg_ObjRefs (2)

struct IOServiceStateNotificationDispatchSource_SetHandler_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_SetHandler_Rpl
{
    IORPCMessageMach           mach;
    IOServiceStateNotificationDispatchSource_SetHandler_Rpl_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_SetHandler_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceStateNotificationDispatchSource_SetHandler_Msg * message;
        struct IOServiceStateNotificationDispatchSource_SetHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceStateNotificationDispatchSource_SetHandler_Invocation;
struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg_ObjRefs (1)

struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl
{
    IORPCMessageMach           mach;
    IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg * message;
        struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceStateNotificationDispatchSource_StateNotificationBegin_Invocation;
struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg_ObjRefs (2)

struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Rpl
{
    IORPCMessageMach           mach;
    IOServiceStateNotificationDispatchSource_StateNotificationReady_Rpl_Content content;
};
#pragma pack()
#define IOServiceStateNotificationDispatchSource_StateNotificationReady_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg * message;
        struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceStateNotificationDispatchSource_StateNotificationReady_Invocation;
kern_return_t
IOServiceStateNotificationDispatchSource::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOServiceStateNotificationDispatchSource::_Dispatch(IOServiceStateNotificationDispatchSource * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODispatchSource_SetEnableWithCompletion_ID:
        {
            ret = IODispatchSource::SetEnableWithCompletion_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::SetEnableWithCompletion_Handler, *self, &IOServiceStateNotificationDispatchSource::SetEnableWithCompletion_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_Cancel_ID:
        {
            ret = IODispatchSource::Cancel_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::Cancel_Handler, *self, &IOServiceStateNotificationDispatchSource::Cancel_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOServiceStateNotificationDispatchSource_SetHandler_ID:
        {
            ret = IOServiceStateNotificationDispatchSource::SetHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IOServiceStateNotificationDispatchSource::SetHandler_Handler, *self, &IOServiceStateNotificationDispatchSource::SetHandler_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOServiceStateNotificationDispatchSource_StateNotificationBegin_ID:
        {
            ret = IOServiceStateNotificationDispatchSource::StateNotificationBegin_Invoke(rpc, self, SimpleMemberFunctionCast(IOServiceStateNotificationDispatchSource::StateNotificationBegin_Handler, *self, &IOServiceStateNotificationDispatchSource::StateNotificationBegin_Impl));
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
IOServiceStateNotificationDispatchSource::MetaClass::Dispatch(const IORPC rpc)
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
IOServiceStateNotificationDispatchSource::Create(
        IOService * service,
        OSArray * items,
        IODispatchQueue * queue,
        IOServiceStateNotificationDispatchSource ** source)
{
    kern_return_t ret;
    union
    {
        IOServiceStateNotificationDispatchSource_Create_Msg msg;
        struct
        {
            IOServiceStateNotificationDispatchSource_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceStateNotificationDispatchSource_Create_Msg * msg = &buf.msg;
    struct IOServiceStateNotificationDispatchSource_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceStateNotificationDispatchSource_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOServiceStateNotificationDispatchSource);
    msg->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 4;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->service__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.service = (OSObjectRef) service;

    msg->items__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.items = (OSObjectRef) items;

    msg->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.queue = (OSObjectRef) queue;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl)
#ifdef KERNEL
                 /* OSMetaClassBase::Invoke() reads the RPC header through
                  * kernelContent and fails with kIOReturnIPCError when it is
                  * NULL, so every kernel-side call must set it. */
                 , .kernelContent = (IORPCMessage *) &buf.msg.content
#endif /* KERNEL */
    };
    ret = OSMTypeID(IOServiceStateNotificationDispatchSource)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOServiceStateNotificationDispatchSource_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOServiceStateNotificationDispatchSource_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *source = OSDynamicCast(IOServiceStateNotificationDispatchSource, (OSObject *) rpl->content.source);
        if (rpl->content.source && !*source) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOServiceStateNotificationDispatchSource::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOServiceStateNotificationDispatchSource_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * service;
    OSArray * items;
    IODispatchQueue * queue;

    if (IOServiceStateNotificationDispatchSource_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    service = OSDynamicCast(IOService, (OSObject *) rpc.message->content.service);
    if (!service && rpc.message->content.service) return (kIOReturnBadArgument);
    items = OSDynamicCast(OSArray, (OSObject *) rpc.message->content.items);
    if (!items && rpc.message->content.items) return (kIOReturnBadArgument);
    queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpc.message->content.queue);
    if (!queue && rpc.message->content.queue) return (kIOReturnBadArgument);

    ret = (*func)(service,
        items,
        queue,
        (IOServiceStateNotificationDispatchSource **)&rpc.reply->content.source);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_Create_Rpl_ObjRefs;
    rpc.reply->source__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOServiceStateNotificationDispatchSource::SetHandler(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOServiceStateNotificationDispatchSource_SetHandler_Msg msg;
        struct
        {
            IOServiceStateNotificationDispatchSource_SetHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceStateNotificationDispatchSource_SetHandler_Msg * msg = &buf.msg;
    struct IOServiceStateNotificationDispatchSource_SetHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceStateNotificationDispatchSource_SetHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_SetHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_SetHandler_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl)
#ifdef KERNEL
                 /* OSMetaClassBase::Invoke() reads the RPC header through
                  * kernelContent and fails with kIOReturnIPCError when it is
                  * NULL, so every kernel-side call must set it. */
                 , .kernelContent = (IORPCMessage *) &buf.msg.content
#endif /* KERNEL */
    };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOServiceStateNotificationDispatchSource_SetHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOServiceStateNotificationDispatchSource_SetHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOServiceStateNotificationDispatchSource::SetHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetHandler_Handler func)
{
    IOServiceStateNotificationDispatchSource_SetHandler_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSAction * action;

    if (IOServiceStateNotificationDispatchSource_SetHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    ret = (*func)(target,
        action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_SetHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_SetHandler_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOServiceStateNotificationDispatchSource::StateNotificationBegin(
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg msg;
        struct
        {
            IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg * msg = &buf.msg;
    struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_StateNotificationBegin_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl)
#ifdef KERNEL
                 /* OSMetaClassBase::Invoke() reads the RPC header through
                  * kernelContent and fails with kIOReturnIPCError when it is
                  * NULL, so every kernel-side call must set it. */
                 , .kernelContent = (IORPCMessage *) &buf.msg.content
#endif /* KERNEL */
    };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOServiceStateNotificationDispatchSource_StateNotificationBegin_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOServiceStateNotificationDispatchSource::StateNotificationBegin_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationBegin_Handler func)
{
    IOServiceStateNotificationDispatchSource_StateNotificationBegin_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOServiceStateNotificationDispatchSource_StateNotificationBegin_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_StateNotificationBegin_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_StateNotificationBegin_Rpl_ObjRefs;

    return (ret);
}

void
IOServiceStateNotificationDispatchSource::StateNotificationReady(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    union
    {
        IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg msg;
    } buf;
    struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg * msg = &buf.msg;

    memset(msg, 0, sizeof(struct IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceStateNotificationDispatchSource_StateNotificationReady_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    action->Invoke(rpc);
}

kern_return_t
IOServiceStateNotificationDispatchSource::StateNotificationReady_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationReady_Handler func)
{
    return IOServiceStateNotificationDispatchSource::StateNotificationReady_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IOServiceStateNotificationDispatchSource::StateNotificationReady_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        StateNotificationReady_Handler func,
        const OSMetaClass * targetActionClass)
{
    IOServiceStateNotificationDispatchSource_StateNotificationReady_Invocation rpc = { _rpc };
    OSAction * action;

    if (IOServiceStateNotificationDispatchSource_StateNotificationReady_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
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

