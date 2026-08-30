/* iig-lite generated from IOServiceNotificationDispatchSource.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOServiceNotificationDispatchSource.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOServiceNotificationDispatchSource.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndAbstractStructors(IOServiceNotificationDispatchSource, IODispatchSource)
#endif /* KERNEL */

struct IOServiceNotificationDispatchSource_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  matching;
    OSObjectRef  queue;
    uint64_t  options;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t matching__descriptor;
    mach_msg_port_descriptor_t queue__descriptor;
    IOServiceNotificationDispatchSource_Create_Msg_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_Create_Msg_ObjRefs (3)

struct IOServiceNotificationDispatchSource_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  notification;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t notification__descriptor;
    IOServiceNotificationDispatchSource_Create_Rpl_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceNotificationDispatchSource_Create_Msg * message;
        struct IOServiceNotificationDispatchSource_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceNotificationDispatchSource_Create_Invocation;
struct IOServiceNotificationDispatchSource_SetHandler_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_SetHandler_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOServiceNotificationDispatchSource_SetHandler_Msg_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_SetHandler_Msg_ObjRefs (2)

struct IOServiceNotificationDispatchSource_SetHandler_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_SetHandler_Rpl
{
    IORPCMessageMach           mach;
    IOServiceNotificationDispatchSource_SetHandler_Rpl_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_SetHandler_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceNotificationDispatchSource_SetHandler_Msg * message;
        struct IOServiceNotificationDispatchSource_SetHandler_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceNotificationDispatchSource_SetHandler_Invocation;
struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg_ObjRefs (2)

struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Rpl
{
    IORPCMessageMach           mach;
    IOServiceNotificationDispatchSource_ServiceNotificationReady_Rpl_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_ServiceNotificationReady_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg * message;
        struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceNotificationDispatchSource_ServiceNotificationReady_Invocation;
struct IOServiceNotificationDispatchSource_CopyNextNotification_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_CopyNextNotification_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOServiceNotificationDispatchSource_CopyNextNotification_Msg_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_CopyNextNotification_Msg_ObjRefs (1)

struct IOServiceNotificationDispatchSource_CopyNextNotification_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  service;
    uint64_t  type;
    uint64_t  options;
};
#pragma pack(4)
struct IOServiceNotificationDispatchSource_CopyNextNotification_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t service__descriptor;
    IOServiceNotificationDispatchSource_CopyNextNotification_Rpl_Content content;
};
#pragma pack()
#define IOServiceNotificationDispatchSource_CopyNextNotification_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOServiceNotificationDispatchSource_CopyNextNotification_Msg * message;
        struct IOServiceNotificationDispatchSource_CopyNextNotification_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOServiceNotificationDispatchSource_CopyNextNotification_Invocation;
kern_return_t
IOServiceNotificationDispatchSource::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOServiceNotificationDispatchSource::_Dispatch(IOServiceNotificationDispatchSource * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODispatchSource_SetEnableWithCompletion_ID:
        {
            ret = IODispatchSource::SetEnableWithCompletion_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::SetEnableWithCompletion_Handler, *self, &IOServiceNotificationDispatchSource::SetEnableWithCompletion_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_Cancel_ID:
        {
            ret = IODispatchSource::Cancel_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::Cancel_Handler, *self, &IOServiceNotificationDispatchSource::Cancel_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOServiceNotificationDispatchSource_SetHandler_ID:
        {
            ret = IOServiceNotificationDispatchSource::SetHandler_Invoke(rpc, self, SimpleMemberFunctionCast(IOServiceNotificationDispatchSource::SetHandler_Handler, *self, &IOServiceNotificationDispatchSource::SetHandler_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODispatchSource_CheckForWork_ID:
        {
            ret = IODispatchSource::CheckForWork_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::CheckForWork_Handler, *self, &IOServiceNotificationDispatchSource::CheckForWork_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOServiceNotificationDispatchSource_CopyNextNotification_ID:
        {
            ret = IOServiceNotificationDispatchSource::CopyNextNotification_Invoke(rpc, self, SimpleMemberFunctionCast(IOServiceNotificationDispatchSource::CopyNextNotification_Handler, *self, &IOServiceNotificationDispatchSource::CopyNextNotification_Impl));
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
IOServiceNotificationDispatchSource::MetaClass::Dispatch(const IORPC rpc)
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
IOServiceNotificationDispatchSource::Create(
        OSDictionary * matching,
        uint64_t options,
        IODispatchQueue * queue,
        IOServiceNotificationDispatchSource ** notification)
{
    kern_return_t ret;
    union
    {
        IOServiceNotificationDispatchSource_Create_Msg msg;
        struct
        {
            IOServiceNotificationDispatchSource_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceNotificationDispatchSource_Create_Msg * msg = &buf.msg;
    struct IOServiceNotificationDispatchSource_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceNotificationDispatchSource_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceNotificationDispatchSource_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOServiceNotificationDispatchSource);
    msg->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->matching__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.matching = (OSObjectRef) matching;

    msg->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.queue = (OSObjectRef) queue;

    msg->content.options = options;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOServiceNotificationDispatchSource)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOServiceNotificationDispatchSource_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOServiceNotificationDispatchSource_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *notification = OSDynamicCast(IOServiceNotificationDispatchSource, (OSObject *) rpl->content.notification);
        if (rpl->content.notification && !*notification) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOServiceNotificationDispatchSource::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOServiceNotificationDispatchSource_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSDictionary * matching;
    IODispatchQueue * queue;

    if (IOServiceNotificationDispatchSource_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    matching = OSDynamicCast(OSDictionary, (OSObject *) rpc.message->content.matching);
    if (!matching && rpc.message->content.matching) return (kIOReturnBadArgument);
    queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpc.message->content.queue);
    if (!queue && rpc.message->content.queue) return (kIOReturnBadArgument);

    ret = (*func)(matching,
        rpc.message->content.options,
        queue,
        (IOServiceNotificationDispatchSource **)&rpc.reply->content.notification);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceNotificationDispatchSource_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_Create_Rpl_ObjRefs;
    rpc.reply->notification__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOServiceNotificationDispatchSource::SetHandler(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOServiceNotificationDispatchSource_SetHandler_Msg msg;
        struct
        {
            IOServiceNotificationDispatchSource_SetHandler_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceNotificationDispatchSource_SetHandler_Msg * msg = &buf.msg;
    struct IOServiceNotificationDispatchSource_SetHandler_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceNotificationDispatchSource_SetHandler_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceNotificationDispatchSource_SetHandler_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_SetHandler_Msg_ObjRefs;
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
            if (rpl->content.__hdr.msgid                  != IOServiceNotificationDispatchSource_SetHandler_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOServiceNotificationDispatchSource_SetHandler_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOServiceNotificationDispatchSource::SetHandler_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetHandler_Handler func)
{
    IOServiceNotificationDispatchSource_SetHandler_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSAction * action;

    if (IOServiceNotificationDispatchSource_SetHandler_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    ret = (*func)(target,
        action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceNotificationDispatchSource_SetHandler_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_SetHandler_Rpl_ObjRefs;

    return (ret);
}

void
IOServiceNotificationDispatchSource::ServiceNotificationReady(
        OSAction * action,
        OSDispatchMethod supermethod)
{
    union
    {
        IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg msg;
    } buf;
    struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg * msg = &buf.msg;

    memset(msg, 0, sizeof(struct IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceNotificationDispatchSource_ServiceNotificationReady_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    action->Invoke(rpc);
}

kern_return_t
IOServiceNotificationDispatchSource::ServiceNotificationReady_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ServiceNotificationReady_Handler func)
{
    return IOServiceNotificationDispatchSource::ServiceNotificationReady_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IOServiceNotificationDispatchSource::ServiceNotificationReady_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        ServiceNotificationReady_Handler func,
        const OSMetaClass * targetActionClass)
{
    IOServiceNotificationDispatchSource_ServiceNotificationReady_Invocation rpc = { _rpc };
    OSAction * action;

    if (IOServiceNotificationDispatchSource_ServiceNotificationReady_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
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
IOServiceNotificationDispatchSource::CopyNextNotification(
        uint64_t * type,
        IOService ** service,
        uint64_t * options,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOServiceNotificationDispatchSource_CopyNextNotification_Msg msg;
        struct
        {
            IOServiceNotificationDispatchSource_CopyNextNotification_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOServiceNotificationDispatchSource_CopyNextNotification_Msg * msg = &buf.msg;
    struct IOServiceNotificationDispatchSource_CopyNextNotification_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOServiceNotificationDispatchSource_CopyNextNotification_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOServiceNotificationDispatchSource_CopyNextNotification_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_CopyNextNotification_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOServiceNotificationDispatchSource_CopyNextNotification_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOServiceNotificationDispatchSource_CopyNextNotification_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *service = OSDynamicCast(IOService, (OSObject *) rpl->content.service);
        if (rpl->content.service && !*service) ret = kIOReturnBadArgument;
        if (type) *type = rpl->content.type;
        if (options) *options = rpl->content.options;
    }

    return (ret);
}

kern_return_t
IOServiceNotificationDispatchSource::CopyNextNotification_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyNextNotification_Handler func)
{
    IOServiceNotificationDispatchSource_CopyNextNotification_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOServiceNotificationDispatchSource_CopyNextNotification_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.type,
        (IOService **)&rpc.reply->content.service,
        &rpc.reply->content.options);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOServiceNotificationDispatchSource_CopyNextNotification_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOServiceNotificationDispatchSource_CopyNextNotification_Rpl_ObjRefs;
    rpc.reply->service__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

