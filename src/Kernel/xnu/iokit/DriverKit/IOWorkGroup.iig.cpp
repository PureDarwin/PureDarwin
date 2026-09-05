/* iig-lite generated from IOWorkGroup.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOWorkGroup.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOWorkGroup.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndStructors(IOWorkGroup, OSObject)
#endif /* KERNEL */

struct IOWorkGroup_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  name;
    OSObjectRef  userClient;
};
#pragma pack(4)
struct IOWorkGroup_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t name__descriptor;
    mach_msg_port_descriptor_t userClient__descriptor;
    IOWorkGroup_Create_Msg_Content content;
};
#pragma pack()
#define IOWorkGroup_Create_Msg_ObjRefs (3)

struct IOWorkGroup_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  workgroup;
};
#pragma pack(4)
struct IOWorkGroup_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t workgroup__descriptor;
    IOWorkGroup_Create_Rpl_Content content;
};
#pragma pack()
#define IOWorkGroup_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOWorkGroup_Create_Msg * message;
        struct IOWorkGroup_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOWorkGroup_Create_Invocation;
struct IOWorkGroup_SetWorkGroupPort_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOWorkGroup_SetWorkGroupPort_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t port__descriptor;
    IOWorkGroup_SetWorkGroupPort_Msg_Content content;
};
#pragma pack()
#define IOWorkGroup_SetWorkGroupPort_Msg_ObjRefs (1)

struct IOWorkGroup_SetWorkGroupPort_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOWorkGroup_SetWorkGroupPort_Rpl
{
    IORPCMessageMach           mach;
    IOWorkGroup_SetWorkGroupPort_Rpl_Content content;
};
#pragma pack()
#define IOWorkGroup_SetWorkGroupPort_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOWorkGroup_SetWorkGroupPort_Msg * message;
        struct IOWorkGroup_SetWorkGroupPort_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOWorkGroup_SetWorkGroupPort_Invocation;
struct IOWorkGroup_InvalidateKernel_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  client;
};
#pragma pack(4)
struct IOWorkGroup_InvalidateKernel_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t client__descriptor;
    IOWorkGroup_InvalidateKernel_Msg_Content content;
};
#pragma pack()
#define IOWorkGroup_InvalidateKernel_Msg_ObjRefs (2)

struct IOWorkGroup_InvalidateKernel_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOWorkGroup_InvalidateKernel_Rpl
{
    IORPCMessageMach           mach;
    IOWorkGroup_InvalidateKernel_Rpl_Content content;
};
#pragma pack()
#define IOWorkGroup_InvalidateKernel_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOWorkGroup_InvalidateKernel_Msg * message;
        struct IOWorkGroup_InvalidateKernel_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOWorkGroup_InvalidateKernel_Invocation;
kern_return_t
IOWorkGroup::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOWorkGroup::_Dispatch(IOWorkGroup * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOWorkGroup_SetWorkGroupPort_ID:
        {
            ret = IOWorkGroup::SetWorkGroupPort_Invoke(rpc, self, SimpleMemberFunctionCast(IOWorkGroup::SetWorkGroupPort_Handler, *self, &IOWorkGroup::SetWorkGroupPort_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOWorkGroup_InvalidateKernel_ID:
        {
            ret = IOWorkGroup::InvalidateKernel_Invoke(rpc, self, SimpleMemberFunctionCast(IOWorkGroup::InvalidateKernel_Handler, *self, &IOWorkGroup::InvalidateKernel_Impl));
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
IOWorkGroup::MetaClass::Dispatch(const IORPC rpc)
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
IOWorkGroup::Create(
        OSString * name,
        IOUserClient * userClient,
        IOWorkGroup ** workgroup)
{
    kern_return_t ret;
    union
    {
        IOWorkGroup_Create_Msg msg;
        struct
        {
            IOWorkGroup_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOWorkGroup_Create_Msg * msg = &buf.msg;
    struct IOWorkGroup_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOWorkGroup_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOWorkGroup_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOWorkGroup);
    msg->content.__hdr.objectRefs = IOWorkGroup_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->name__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.name = (OSObjectRef) name;

    msg->userClient__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.userClient = (OSObjectRef) userClient;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl)
#ifdef KERNEL
                 /* OSMetaClassBase::Invoke() reads the RPC header through
                  * kernelContent and fails with kIOReturnIPCError when it is
                  * NULL, so every kernel-side call must set it. */
                 , .kernelContent = (IORPCMessage *) &buf.msg.content
#endif /* KERNEL */
    };
    ret = OSMTypeID(IOWorkGroup)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOWorkGroup_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOWorkGroup_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *workgroup = OSDynamicCast(IOWorkGroup, (OSObject *) rpl->content.workgroup);
        if (rpl->content.workgroup && !*workgroup) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOWorkGroup::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOWorkGroup_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * name;
    IOUserClient * userClient;

    if (IOWorkGroup_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    name = OSDynamicCast(OSString, (OSObject *) rpc.message->content.name);
    if (!name && rpc.message->content.name) return (kIOReturnBadArgument);
    userClient = OSDynamicCast(IOUserClient, (OSObject *) rpc.message->content.userClient);
    if (!userClient && rpc.message->content.userClient) return (kIOReturnBadArgument);

    ret = (*func)(name,
        userClient,
        (IOWorkGroup **)&rpc.reply->content.workgroup);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOWorkGroup_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOWorkGroup_Create_Rpl_ObjRefs;
    rpc.reply->workgroup__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOWorkGroup::SetWorkGroupPort(
        mach_port_t port,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOWorkGroup_SetWorkGroupPort_Msg msg;
        struct
        {
            IOWorkGroup_SetWorkGroupPort_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOWorkGroup_SetWorkGroupPort_Msg * msg = &buf.msg;
    struct IOWorkGroup_SetWorkGroupPort_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOWorkGroup_SetWorkGroupPort_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOWorkGroup_SetWorkGroupPort_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOWorkGroup_SetWorkGroupPort_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->port__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->port__descriptor.disposition = MACH_MSG_TYPE_COPY_SEND;
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
            if (rpl->content.__hdr.msgid                  != IOWorkGroup_SetWorkGroupPort_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOWorkGroup_SetWorkGroupPort_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOWorkGroup::SetWorkGroupPort_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetWorkGroupPort_Handler func)
{
    IOWorkGroup_SetWorkGroupPort_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOWorkGroup_SetWorkGroupPort_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->port__descriptor.name);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOWorkGroup_SetWorkGroupPort_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOWorkGroup_SetWorkGroupPort_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOWorkGroup::InvalidateKernel(
        IOUserClient * client,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOWorkGroup_InvalidateKernel_Msg msg;
        struct
        {
            IOWorkGroup_InvalidateKernel_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOWorkGroup_InvalidateKernel_Msg * msg = &buf.msg;
    struct IOWorkGroup_InvalidateKernel_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOWorkGroup_InvalidateKernel_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOWorkGroup_InvalidateKernel_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOWorkGroup_InvalidateKernel_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->client__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.client = (OSObjectRef) client;

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
            if (rpl->content.__hdr.msgid                  != IOWorkGroup_InvalidateKernel_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOWorkGroup_InvalidateKernel_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOWorkGroup::InvalidateKernel_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        InvalidateKernel_Handler func)
{
    IOWorkGroup_InvalidateKernel_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOUserClient * client;

    if (IOWorkGroup_InvalidateKernel_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    client = OSDynamicCast(IOUserClient, (OSObject *) rpc.message->content.client);
    if (!client && rpc.message->content.client) return (kIOReturnBadArgument);

    ret = (*func)(target,
        client);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOWorkGroup_InvalidateKernel_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOWorkGroup_InvalidateKernel_Rpl_ObjRefs;

    return (ret);
}

