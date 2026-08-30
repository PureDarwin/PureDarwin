/* iig-lite generated from IOEventLink.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOEventLink.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOEventLink.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndStructors(IOEventLink, OSObject)
#endif /* KERNEL */

struct IOEventLink_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  name;
    OSObjectRef  userClient;
};
#pragma pack(4)
struct IOEventLink_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t name__descriptor;
    mach_msg_port_descriptor_t userClient__descriptor;
    IOEventLink_Create_Msg_Content content;
};
#pragma pack()
#define IOEventLink_Create_Msg_ObjRefs (3)

struct IOEventLink_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  eventLink;
};
#pragma pack(4)
struct IOEventLink_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t eventLink__descriptor;
    IOEventLink_Create_Rpl_Content content;
};
#pragma pack()
#define IOEventLink_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOEventLink_Create_Msg * message;
        struct IOEventLink_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOEventLink_Create_Invocation;
struct IOEventLink_SetEventlinkPort_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOEventLink_SetEventlinkPort_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t port__descriptor;
    IOEventLink_SetEventlinkPort_Msg_Content content;
};
#pragma pack()
#define IOEventLink_SetEventlinkPort_Msg_ObjRefs (1)

struct IOEventLink_SetEventlinkPort_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOEventLink_SetEventlinkPort_Rpl
{
    IORPCMessageMach           mach;
    IOEventLink_SetEventlinkPort_Rpl_Content content;
};
#pragma pack()
#define IOEventLink_SetEventlinkPort_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOEventLink_SetEventlinkPort_Msg * message;
        struct IOEventLink_SetEventlinkPort_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOEventLink_SetEventlinkPort_Invocation;
struct IOEventLink_InvalidateKernel_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  client;
};
#pragma pack(4)
struct IOEventLink_InvalidateKernel_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t client__descriptor;
    IOEventLink_InvalidateKernel_Msg_Content content;
};
#pragma pack()
#define IOEventLink_InvalidateKernel_Msg_ObjRefs (2)

struct IOEventLink_InvalidateKernel_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOEventLink_InvalidateKernel_Rpl
{
    IORPCMessageMach           mach;
    IOEventLink_InvalidateKernel_Rpl_Content content;
};
#pragma pack()
#define IOEventLink_InvalidateKernel_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOEventLink_InvalidateKernel_Msg * message;
        struct IOEventLink_InvalidateKernel_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOEventLink_InvalidateKernel_Invocation;
kern_return_t
IOEventLink::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOEventLink::_Dispatch(IOEventLink * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOEventLink_SetEventlinkPort_ID:
        {
            ret = IOEventLink::SetEventlinkPort_Invoke(rpc, self, SimpleMemberFunctionCast(IOEventLink::SetEventlinkPort_Handler, *self, &IOEventLink::SetEventlinkPort_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOEventLink_InvalidateKernel_ID:
        {
            ret = IOEventLink::InvalidateKernel_Invoke(rpc, self, SimpleMemberFunctionCast(IOEventLink::InvalidateKernel_Handler, *self, &IOEventLink::InvalidateKernel_Impl));
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
IOEventLink::MetaClass::Dispatch(const IORPC rpc)
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
IOEventLink::Create(
        OSString * name,
        IOUserClient * userClient,
        IOEventLink ** eventLink)
{
    kern_return_t ret;
    union
    {
        IOEventLink_Create_Msg msg;
        struct
        {
            IOEventLink_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOEventLink_Create_Msg * msg = &buf.msg;
    struct IOEventLink_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOEventLink_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOEventLink_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOEventLink);
    msg->content.__hdr.objectRefs = IOEventLink_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->name__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.name = (OSObjectRef) name;

    msg->userClient__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.userClient = (OSObjectRef) userClient;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOEventLink)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOEventLink_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOEventLink_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *eventLink = OSDynamicCast(IOEventLink, (OSObject *) rpl->content.eventLink);
        if (rpl->content.eventLink && !*eventLink) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOEventLink::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOEventLink_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSString * name;
    IOUserClient * userClient;

    if (IOEventLink_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    name = OSDynamicCast(OSString, (OSObject *) rpc.message->content.name);
    if (!name && rpc.message->content.name) return (kIOReturnBadArgument);
    userClient = OSDynamicCast(IOUserClient, (OSObject *) rpc.message->content.userClient);
    if (!userClient && rpc.message->content.userClient) return (kIOReturnBadArgument);

    ret = (*func)(name,
        userClient,
        (IOEventLink **)&rpc.reply->content.eventLink);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOEventLink_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOEventLink_Create_Rpl_ObjRefs;
    rpc.reply->eventLink__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOEventLink::SetEventlinkPort(
        mach_port_t port,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOEventLink_SetEventlinkPort_Msg msg;
        struct
        {
            IOEventLink_SetEventlinkPort_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOEventLink_SetEventlinkPort_Msg * msg = &buf.msg;
    struct IOEventLink_SetEventlinkPort_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOEventLink_SetEventlinkPort_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOEventLink_SetEventlinkPort_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOEventLink_SetEventlinkPort_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->port__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->port__descriptor.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg->port__descriptor.name = port;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOEventLink_SetEventlinkPort_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOEventLink_SetEventlinkPort_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOEventLink::SetEventlinkPort_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetEventlinkPort_Handler func)
{
    IOEventLink_SetEventlinkPort_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOEventLink_SetEventlinkPort_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->port__descriptor.name);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOEventLink_SetEventlinkPort_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOEventLink_SetEventlinkPort_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOEventLink::InvalidateKernel(
        IOUserClient * client,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOEventLink_InvalidateKernel_Msg msg;
        struct
        {
            IOEventLink_InvalidateKernel_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOEventLink_InvalidateKernel_Msg * msg = &buf.msg;
    struct IOEventLink_InvalidateKernel_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOEventLink_InvalidateKernel_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOEventLink_InvalidateKernel_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOEventLink_InvalidateKernel_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->client__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.client = (OSObjectRef) client;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOEventLink_InvalidateKernel_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOEventLink_InvalidateKernel_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    return (ret);
}

kern_return_t
IOEventLink::InvalidateKernel_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        InvalidateKernel_Handler func)
{
    IOEventLink_InvalidateKernel_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOUserClient * client;

    if (IOEventLink_InvalidateKernel_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    client = OSDynamicCast(IOUserClient, (OSObject *) rpc.message->content.client);
    if (!client && rpc.message->content.client) return (kIOReturnBadArgument);

    ret = (*func)(target,
        client);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOEventLink_InvalidateKernel_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOEventLink_InvalidateKernel_Rpl_ObjRefs;

    return (ret);
}

