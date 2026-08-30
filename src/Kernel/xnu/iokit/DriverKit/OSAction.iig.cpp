/* iig-lite generated from OSAction.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	OSAction.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "OSAction.h"

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

#if KERNEL
OSDefineMetaClassAndStructors(OSAction, OSObject)
#endif /* KERNEL */

struct OSAction_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  target;
    uint64_t  targetmsgid;
    uint64_t  msgid;
    size_t  referenceSize;
};
#pragma pack(4)
struct OSAction_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t target__descriptor;
    OSAction_Create_Msg_Content content;
};
#pragma pack()
#define OSAction_Create_Msg_ObjRefs (2)

struct OSAction_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  action;
};
#pragma pack(4)
struct OSAction_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t action__descriptor;
    OSAction_Create_Rpl_Content content;
};
#pragma pack()
#define OSAction_Create_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct OSAction_Create_Msg * message;
        struct OSAction_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
OSAction_Create_Invocation;
struct OSAction_CreateWithTypeName_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  target;
    OSObjectRef  typeName;
    uint64_t  targetmsgid;
    uint64_t  msgid;
    size_t  referenceSize;
};
#pragma pack(4)
struct OSAction_CreateWithTypeName_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t target__descriptor;
    mach_msg_port_descriptor_t typeName__descriptor;
    OSAction_CreateWithTypeName_Msg_Content content;
};
#pragma pack()
#define OSAction_CreateWithTypeName_Msg_ObjRefs (3)

struct OSAction_CreateWithTypeName_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  action;
};
#pragma pack(4)
struct OSAction_CreateWithTypeName_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t action__descriptor;
    OSAction_CreateWithTypeName_Rpl_Content content;
};
#pragma pack()
#define OSAction_CreateWithTypeName_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct OSAction_CreateWithTypeName_Msg * message;
        struct OSAction_CreateWithTypeName_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
OSAction_CreateWithTypeName_Invocation;
struct OSAction_Aborted_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct OSAction_Aborted_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    OSAction_Aborted_Msg_Content content;
};
#pragma pack()
#define OSAction_Aborted_Msg_ObjRefs (1)

struct OSAction_Aborted_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct OSAction_Aborted_Rpl
{
    IORPCMessageMach           mach;
    OSAction_Aborted_Rpl_Content content;
};
#pragma pack()
#define OSAction_Aborted_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct OSAction_Aborted_Msg * message;
        struct OSAction_Aborted_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
OSAction_Aborted_Invocation;
kern_return_t
OSAction::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
OSAction::_Dispatch(OSAction * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case OSAction_Aborted_ID:
        {
            ret = OSAction::Aborted_Invoke(rpc, self, SimpleMemberFunctionCast(OSAction::Aborted_Handler, *self, &OSAction::Aborted_Impl));
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
OSAction::MetaClass::Dispatch(const IORPC rpc)
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
OSAction::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    OSAction_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSObject * target;

    if (OSAction_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    target = OSDynamicCast(OSObject, (OSObject *) rpc.message->content.target);
    if (!target && rpc.message->content.target) return (kIOReturnBadArgument);

    ret = (*func)(target,
        rpc.message->content.targetmsgid,
        rpc.message->content.msgid,
        rpc.message->content.referenceSize,
        (OSAction **)&rpc.reply->content.action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = OSAction_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = OSAction_Create_Rpl_ObjRefs;
    rpc.reply->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
OSAction::CreateWithTypeName_Invoke(const IORPC _rpc,
        CreateWithTypeName_Handler func)
{
    OSAction_CreateWithTypeName_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSObject * target;
    OSString * typeName;

    if (OSAction_CreateWithTypeName_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    target = OSDynamicCast(OSObject, (OSObject *) rpc.message->content.target);
    if (!target && rpc.message->content.target) return (kIOReturnBadArgument);
    typeName = OSDynamicCast(OSString, (OSObject *) rpc.message->content.typeName);
    if (!typeName && rpc.message->content.typeName) return (kIOReturnBadArgument);

    ret = (*func)(target,
        rpc.message->content.targetmsgid,
        rpc.message->content.msgid,
        rpc.message->content.referenceSize,
        typeName,
        (OSAction **)&rpc.reply->content.action);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = OSAction_CreateWithTypeName_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = OSAction_CreateWithTypeName_Rpl_ObjRefs;
    rpc.reply->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

void
OSAction::Aborted(
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        OSAction_Aborted_Msg msg;
        struct
        {
            OSAction_Aborted_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct OSAction_Aborted_Msg * msg = &buf.msg;
    struct OSAction_Aborted_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct OSAction_Aborted_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = OSAction_Aborted_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = OSAction_Aborted_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != OSAction_Aborted_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (OSAction_Aborted_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }

    (void) ret;
}

kern_return_t
OSAction::Aborted_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        Aborted_Handler func)
{
    OSAction_Aborted_Invocation rpc = { _rpc };

    if (OSAction_Aborted_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    (*func)(target);

    return (kIOReturnSuccess);
}

