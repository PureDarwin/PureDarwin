/* iig-lite generated from OSObject.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	OSObject.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "OSObject.h"

/* @iig implementation */
#include <DriverKit/IODispatchQueue.h>
/* @iig end */

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

struct OSObject_SetDispatchQueue_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  queue;
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[256];
};
#pragma pack(4)
struct OSObject_SetDispatchQueue_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t queue__descriptor;
    OSObject_SetDispatchQueue_Msg_Content content;
};
#pragma pack()
#define OSObject_SetDispatchQueue_Msg_ObjRefs (2)

struct OSObject_SetDispatchQueue_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct OSObject_SetDispatchQueue_Rpl
{
    IORPCMessageMach           mach;
    OSObject_SetDispatchQueue_Rpl_Content content;
};
#pragma pack()
#define OSObject_SetDispatchQueue_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct OSObject_SetDispatchQueue_Msg * message;
        struct OSObject_SetDispatchQueue_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
OSObject_SetDispatchQueue_Invocation;
struct OSObject_CopyDispatchQueue_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[256];
};
#pragma pack(4)
struct OSObject_CopyDispatchQueue_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    OSObject_CopyDispatchQueue_Msg_Content content;
};
#pragma pack()
#define OSObject_CopyDispatchQueue_Msg_ObjRefs (1)

struct OSObject_CopyDispatchQueue_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  queue;
};
#pragma pack(4)
struct OSObject_CopyDispatchQueue_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t queue__descriptor;
    OSObject_CopyDispatchQueue_Rpl_Content content;
};
#pragma pack()
#define OSObject_CopyDispatchQueue_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct OSObject_CopyDispatchQueue_Msg * message;
        struct OSObject_CopyDispatchQueue_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
OSObject_CopyDispatchQueue_Invocation;
kern_return_t
OSObject::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
OSObject::_Dispatch(OSObject * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {

        default:
            ret = self->OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
OSObject::MetaClass::Dispatch(const IORPC rpc)
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
OSObject::SetDispatchQueue(
        const char * name,
        IODispatchQueue * queue,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        OSObject_SetDispatchQueue_Msg msg;
        struct
        {
            OSObject_SetDispatchQueue_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct OSObject_SetDispatchQueue_Msg * msg = &buf.msg;
    struct OSObject_SetDispatchQueue_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct OSObject_SetDispatchQueue_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = OSObject_SetDispatchQueue_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = OSObject_SetDispatchQueue_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.queue = (OSObjectRef) queue;

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != OSObject_SetDispatchQueue_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (OSObject_SetDispatchQueue_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
OSObject::SetDispatchQueue_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetDispatchQueue_Handler func)
{
    OSObject_SetDispatchQueue_Invocation rpc = { _rpc };
    kern_return_t ret;
    IODispatchQueue * queue;

    if (OSObject_SetDispatchQueue_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    queue = OSDynamicCast(IODispatchQueue, (OSObject *) rpc.message->content.queue);
    if (!queue && rpc.message->content.queue) return (kIOReturnBadArgument);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;

    ret = (*func)(target,
        &rpc.message->content.__name[0],
        queue);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = OSObject_SetDispatchQueue_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = OSObject_SetDispatchQueue_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
OSObject::CopyDispatchQueue(
        const char * name,
        IODispatchQueue ** queue,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        OSObject_CopyDispatchQueue_Msg msg;
        struct
        {
            OSObject_CopyDispatchQueue_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct OSObject_CopyDispatchQueue_Msg * msg = &buf.msg;
    struct OSObject_CopyDispatchQueue_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct OSObject_CopyDispatchQueue_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = OSObject_CopyDispatchQueue_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = OSObject_CopyDispatchQueue_Msg_ObjRefs;
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
            if (rpl->content.__hdr.msgid                  != OSObject_CopyDispatchQueue_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (OSObject_CopyDispatchQueue_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
OSObject::CopyDispatchQueue_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyDispatchQueue_Handler func)
{
    OSObject_CopyDispatchQueue_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (OSObject_CopyDispatchQueue_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;

    ret = (*func)(target,
        &rpc.message->content.__name[0],
        (IODispatchQueue **)&rpc.reply->content.queue);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = OSObject_CopyDispatchQueue_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = OSObject_CopyDispatchQueue_Rpl_ObjRefs;
    rpc.reply->queue__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

