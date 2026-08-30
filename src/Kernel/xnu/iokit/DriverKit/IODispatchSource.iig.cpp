/* iig-lite generated from IODispatchSource.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IODispatchSource.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IODispatchSource.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndAbstractStructors(IODispatchSource, OSObject)
#endif /* KERNEL */

struct IODispatchSource_Cancel_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    IODispatchSourceCancelHandler  handler;
};
#pragma pack(4)
struct IODispatchSource_Cancel_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODispatchSource_Cancel_Msg_Content content;
};
#pragma pack()
#define IODispatchSource_Cancel_Msg_ObjRefs (1)

struct IODispatchSource_Cancel_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODispatchSource_Cancel_Rpl
{
    IORPCMessageMach           mach;
    IODispatchSource_Cancel_Rpl_Content content;
};
#pragma pack()
#define IODispatchSource_Cancel_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchSource_Cancel_Msg * message;
        struct IODispatchSource_Cancel_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchSource_Cancel_Invocation;
struct IODispatchSource_SetEnableWithCompletion_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    bool  enable;
    IODispatchSourceCancelHandler  handler;
};
#pragma pack(4)
struct IODispatchSource_SetEnableWithCompletion_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODispatchSource_SetEnableWithCompletion_Msg_Content content;
};
#pragma pack()
#define IODispatchSource_SetEnableWithCompletion_Msg_ObjRefs (1)

struct IODispatchSource_SetEnableWithCompletion_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODispatchSource_SetEnableWithCompletion_Rpl
{
    IORPCMessageMach           mach;
    IODispatchSource_SetEnableWithCompletion_Rpl_Content content;
};
#pragma pack()
#define IODispatchSource_SetEnableWithCompletion_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchSource_SetEnableWithCompletion_Msg * message;
        struct IODispatchSource_SetEnableWithCompletion_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchSource_SetEnableWithCompletion_Invocation;
struct IODispatchSource_CheckForWork_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    bool  synchronous;
};
#pragma pack(4)
struct IODispatchSource_CheckForWork_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODispatchSource_CheckForWork_Msg_Content content;
};
#pragma pack()
#define IODispatchSource_CheckForWork_Msg_ObjRefs (1)

struct IODispatchSource_CheckForWork_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODispatchSource_CheckForWork_Rpl
{
    IORPCMessageMach           mach;
    IODispatchSource_CheckForWork_Rpl_Content content;
};
#pragma pack()
#define IODispatchSource_CheckForWork_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchSource_CheckForWork_Msg * message;
        struct IODispatchSource_CheckForWork_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchSource_CheckForWork_Invocation;
struct IODispatchSource_SetEnable_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    bool  enable;
};
#pragma pack(4)
struct IODispatchSource_SetEnable_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODispatchSource_SetEnable_Msg_Content content;
};
#pragma pack()
#define IODispatchSource_SetEnable_Msg_ObjRefs (1)

struct IODispatchSource_SetEnable_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODispatchSource_SetEnable_Rpl
{
    IORPCMessageMach           mach;
    IODispatchSource_SetEnable_Rpl_Content content;
};
#pragma pack()
#define IODispatchSource_SetEnable_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchSource_SetEnable_Msg * message;
        struct IODispatchSource_SetEnable_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchSource_SetEnable_Invocation;
kern_return_t
IODispatchSource::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IODispatchSource::_Dispatch(IODispatchSource * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODispatchSource_SetEnable_ID:
        {
            ret = IODispatchSource::SetEnable_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchSource::SetEnable_Handler, *self, &IODispatchSource::SetEnable_Impl));
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
IODispatchSource::MetaClass::Dispatch(const IORPC rpc)
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
IODispatchSource::Cancel(
        IODispatchSourceCancelHandler handler,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODispatchSource_Cancel_Msg msg;
        struct
        {
            IODispatchSource_Cancel_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchSource_Cancel_Msg * msg = &buf.msg;
    struct IODispatchSource_Cancel_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchSource_Cancel_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchSource_Cancel_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODispatchSource_Cancel_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.handler = handler;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODispatchSource_Cancel_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODispatchSource_Cancel_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IODispatchSource::Cancel_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Cancel_Handler func)
{
    IODispatchSource_Cancel_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchSource_Cancel_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.handler);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchSource_Cancel_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODispatchSource_Cancel_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODispatchSource::SetEnableWithCompletion(
        bool enable,
        IODispatchSourceCancelHandler handler,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODispatchSource_SetEnableWithCompletion_Msg msg;
        struct
        {
            IODispatchSource_SetEnableWithCompletion_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchSource_SetEnableWithCompletion_Msg * msg = &buf.msg;
    struct IODispatchSource_SetEnableWithCompletion_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchSource_SetEnableWithCompletion_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchSource_SetEnableWithCompletion_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODispatchSource_SetEnableWithCompletion_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.enable = enable;

    msg->content.handler = handler;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODispatchSource_SetEnableWithCompletion_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODispatchSource_SetEnableWithCompletion_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IODispatchSource::SetEnableWithCompletion_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetEnableWithCompletion_Handler func)
{
    IODispatchSource_SetEnableWithCompletion_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchSource_SetEnableWithCompletion_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.enable,
        rpc.message->content.handler);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchSource_SetEnableWithCompletion_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODispatchSource_SetEnableWithCompletion_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODispatchSource::CheckForWork(
        bool synchronous,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODispatchSource_CheckForWork_Msg msg;
        struct
        {
            IODispatchSource_CheckForWork_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchSource_CheckForWork_Msg * msg = &buf.msg;
    struct IODispatchSource_CheckForWork_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchSource_CheckForWork_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchSource_CheckForWork_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODispatchSource_CheckForWork_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.synchronous = synchronous;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODispatchSource_CheckForWork_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODispatchSource_CheckForWork_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IODispatchSource::CheckForWork_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CheckForWork_Handler func)
{
    IODispatchSource_CheckForWork_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchSource_CheckForWork_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.rpc,
        rpc.message->content.synchronous);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchSource_CheckForWork_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODispatchSource_CheckForWork_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODispatchSource::SetEnable(
        bool enable,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODispatchSource_SetEnable_Msg msg;
        struct
        {
            IODispatchSource_SetEnable_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchSource_SetEnable_Msg * msg = &buf.msg;
    struct IODispatchSource_SetEnable_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchSource_SetEnable_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchSource_SetEnable_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODispatchSource_SetEnable_Msg_ObjRefs;
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
            if (rpl->content.__hdr.msgid                  != IODispatchSource_SetEnable_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODispatchSource_SetEnable_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IODispatchSource::SetEnable_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetEnable_Handler func)
{
    IODispatchSource_SetEnable_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchSource_SetEnable_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.enable);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchSource_SetEnable_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODispatchSource_SetEnable_Rpl_ObjRefs;

    return (ret);
}

