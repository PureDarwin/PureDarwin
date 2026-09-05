/* iig-lite generated from IODispatchQueue.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IODispatchQueue.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IODispatchQueue.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndStructors(IODispatchQueue, OSObject)
#endif /* KERNEL */

struct IODispatchQueue_SetPort_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IODispatchQueue_SetPort_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t port__descriptor;
    IODispatchQueue_SetPort_Msg_Content content;
};
#pragma pack()
#define IODispatchQueue_SetPort_Msg_ObjRefs (1)

struct IODispatchQueue_SetPort_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODispatchQueue_SetPort_Rpl
{
    IORPCMessageMach           mach;
    IODispatchQueue_SetPort_Rpl_Content content;
};
#pragma pack()
#define IODispatchQueue_SetPort_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchQueue_SetPort_Msg * message;
        struct IODispatchQueue_SetPort_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchQueue_SetPort_Invocation;
struct IODispatchQueue_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
    uint64_t  priority;
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[256];
};
#pragma pack(4)
struct IODispatchQueue_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODispatchQueue_Create_Msg_Content content;
};
#pragma pack()
#define IODispatchQueue_Create_Msg_ObjRefs (1)

struct IODispatchQueue_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  queue;
};
#pragma pack(4)
struct IODispatchQueue_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t queue__descriptor;
    IODispatchQueue_Create_Rpl_Content content;
};
#pragma pack()
#define IODispatchQueue_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODispatchQueue_Create_Msg * message;
        struct IODispatchQueue_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODispatchQueue_Create_Invocation;
kern_return_t
IODispatchQueue::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IODispatchQueue::_Dispatch(IODispatchQueue * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODispatchQueue_SetPort_ID:
        {
            ret = IODispatchQueue::SetPort_Invoke(rpc, self, SimpleMemberFunctionCast(IODispatchQueue::SetPort_Handler, *self, &IODispatchQueue::SetPort_Impl));
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
IODispatchQueue::MetaClass::Dispatch(const IORPC rpc)
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
IODispatchQueue::SetPort(
        mach_port_t port,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODispatchQueue_SetPort_Msg msg;
        struct
        {
            IODispatchQueue_SetPort_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchQueue_SetPort_Msg * msg = &buf.msg;
    struct IODispatchQueue_SetPort_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchQueue_SetPort_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchQueue_SetPort_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODispatchQueue_SetPort_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->port__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->port__descriptor.disposition = MACH_MSG_TYPE_MAKE_SEND;
    msg->port__descriptor.name = port;

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
            if (rpl->content.__hdr.msgid                  != IODispatchQueue_SetPort_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODispatchQueue_SetPort_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IODispatchQueue::SetPort_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetPort_Handler func)
{
    IODispatchQueue_SetPort_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchQueue_SetPort_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->port__descriptor.name);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchQueue_SetPort_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODispatchQueue_SetPort_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODispatchQueue::Create(
        const char * name,
        uint64_t options,
        uint64_t priority,
        IODispatchQueue ** queue)
{
    kern_return_t ret;
    union
    {
        IODispatchQueue_Create_Msg msg;
        struct
        {
            IODispatchQueue_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODispatchQueue_Create_Msg * msg = &buf.msg;
    struct IODispatchQueue_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODispatchQueue_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODispatchQueue_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IODispatchQueue);
    msg->content.__hdr.objectRefs = IODispatchQueue_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->content.priority = priority;

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl)
#ifdef KERNEL
                 /* OSMetaClassBase::Invoke() reads the RPC header through
                  * kernelContent and fails with kIOReturnIPCError when it is
                  * NULL, so every kernel-side call must set it. */
                 , .kernelContent = (IORPCMessage *) &buf.msg.content
#endif /* KERNEL */
    };
    ret = OSMTypeID(IODispatchQueue)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODispatchQueue_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODispatchQueue_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
IODispatchQueue::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IODispatchQueue_Create_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchQueue_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;

    ret = (*func)(&rpc.message->content.__name[0],
        rpc.message->content.options,
        rpc.message->content.priority,
        (IODispatchQueue **)&rpc.reply->content.queue);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODispatchQueue_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODispatchQueue_Create_Rpl_ObjRefs;
    rpc.reply->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

