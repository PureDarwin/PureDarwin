/* iig-lite generated from IOUserClient.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOUserClient.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOUserClient.h"

/* @iig implementation */
#include <DriverKit/IOBufferMemoryDescriptor.h>
/* @iig end */

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

struct IOUserClient_AsyncCompletion_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
    const uint64_t *  asyncData;
#if !defined(__LP64__)
    uint32_t __asyncDataPad;
#endif /* !defined(__LP64__) */
    uint64_t __asyncData[16];
    IOReturn  status;
    uint32_t  asyncDataCount;
};
#pragma pack(4)
struct IOUserClient_AsyncCompletion_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOUserClient_AsyncCompletion_Msg_Content content;
};
#pragma pack()
#define IOUserClient_AsyncCompletion_Msg_ObjRefs (2)

struct IOUserClient_AsyncCompletion_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOUserClient_AsyncCompletion_Rpl
{
    IORPCMessageMach           mach;
    IOUserClient_AsyncCompletion_Rpl_Content content;
};
#pragma pack()
#define IOUserClient_AsyncCompletion_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient_AsyncCompletion_Msg * message;
        struct IOUserClient_AsyncCompletion_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient_AsyncCompletion_Invocation;
struct IOUserClient_CopyClientMemoryForType_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  type;
};
#pragma pack(4)
struct IOUserClient_CopyClientMemoryForType_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOUserClient_CopyClientMemoryForType_Msg_Content content;
};
#pragma pack()
#define IOUserClient_CopyClientMemoryForType_Msg_ObjRefs (1)

struct IOUserClient_CopyClientMemoryForType_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
    uint64_t  options;
};
#pragma pack(4)
struct IOUserClient_CopyClientMemoryForType_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOUserClient_CopyClientMemoryForType_Rpl_Content content;
};
#pragma pack()
#define IOUserClient_CopyClientMemoryForType_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient_CopyClientMemoryForType_Msg * message;
        struct IOUserClient_CopyClientMemoryForType_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient_CopyClientMemoryForType_Invocation;
struct IOUserClient_CreateMemoryDescriptorFromClient_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    const IOAddressSegment *  segments;
#if !defined(__LP64__)
    uint32_t __segmentsPad;
#endif /* !defined(__LP64__) */
    IOAddressSegment __segments[32];
    uint64_t  memoryDescriptorCreateOptions;
    uint32_t  segmentsCount;
};
#pragma pack(4)
struct IOUserClient_CreateMemoryDescriptorFromClient_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOUserClient_CreateMemoryDescriptorFromClient_Msg_Content content;
};
#pragma pack()
#define IOUserClient_CreateMemoryDescriptorFromClient_Msg_ObjRefs (1)

struct IOUserClient_CreateMemoryDescriptorFromClient_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
};
#pragma pack(4)
struct IOUserClient_CreateMemoryDescriptorFromClient_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOUserClient_CreateMemoryDescriptorFromClient_Rpl_Content content;
};
#pragma pack()
#define IOUserClient_CreateMemoryDescriptorFromClient_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient_CreateMemoryDescriptorFromClient_Msg * message;
        struct IOUserClient_CreateMemoryDescriptorFromClient_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient_CreateMemoryDescriptorFromClient_Invocation;
struct IOUserClient_CopyClientEntitlements_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOUserClient_CopyClientEntitlements_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOUserClient_CopyClientEntitlements_Msg_Content content;
};
#pragma pack()
#define IOUserClient_CopyClientEntitlements_Msg_ObjRefs (1)

struct IOUserClient_CopyClientEntitlements_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  entitlements;
};
#pragma pack(4)
struct IOUserClient_CopyClientEntitlements_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t entitlements__descriptor;
    IOUserClient_CopyClientEntitlements_Rpl_Content content;
};
#pragma pack()
#define IOUserClient_CopyClientEntitlements_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient_CopyClientEntitlements_Msg * message;
        struct IOUserClient_CopyClientEntitlements_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient_CopyClientEntitlements_Invocation;
struct IOUserClient__ExternalMethod_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  structureInput;
    OSObjectRef  structureInputDescriptor;
    OSObjectRef  structureOutputDescriptor;
    OSObjectRef  completion;
    const uint64_t *  scalarInput;
#if !defined(__LP64__)
    uint32_t __scalarInputPad;
#endif /* !defined(__LP64__) */
    uint64_t __scalarInput[16];
    uint64_t  selector;
    uint32_t  scalarInputCount;
    uint64_t  structureOutputMaximumSize;
    uint32_t  scalarOutputCount;
};
#pragma pack(4)
struct IOUserClient__ExternalMethod_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t structureInput__descriptor;
    mach_msg_port_descriptor_t structureInputDescriptor__descriptor;
    mach_msg_port_descriptor_t structureOutputDescriptor__descriptor;
    mach_msg_port_descriptor_t completion__descriptor;
    IOUserClient__ExternalMethod_Msg_Content content;
};
#pragma pack()
#define IOUserClient__ExternalMethod_Msg_ObjRefs (5)

struct IOUserClient__ExternalMethod_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  structureOutput;
    uint32_t  scalarOutputCount;
    uint64_t *  scalarOutput;
#if !defined(__LP64__)
    uint32_t __scalarOutputPad;
#endif /* !defined(__LP64__) */
    uint64_t __scalarOutput[16];
};
#pragma pack(4)
struct IOUserClient__ExternalMethod_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t structureOutput__descriptor;
    IOUserClient__ExternalMethod_Rpl_Content content;
};
#pragma pack()
#define IOUserClient__ExternalMethod_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient__ExternalMethod_Msg * message;
        struct IOUserClient__ExternalMethod_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient__ExternalMethod_Invocation;
struct IOUserClient_KernelCompletion_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  action;
    const uint64_t *  asyncData;
#if !defined(__LP64__)
    uint32_t __asyncDataPad;
#endif /* !defined(__LP64__) */
    uint64_t __asyncData[16];
    IOReturn  status;
    uint32_t  asyncDataCount;
};
#pragma pack(4)
struct IOUserClient_KernelCompletion_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t action__descriptor;
    IOUserClient_KernelCompletion_Msg_Content content;
};
#pragma pack()
#define IOUserClient_KernelCompletion_Msg_ObjRefs (2)

struct IOUserClient_KernelCompletion_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOUserClient_KernelCompletion_Rpl
{
    IORPCMessageMach           mach;
    IOUserClient_KernelCompletion_Rpl_Content content;
};
#pragma pack()
#define IOUserClient_KernelCompletion_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOUserClient_KernelCompletion_Msg * message;
        struct IOUserClient_KernelCompletion_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOUserClient_KernelCompletion_Invocation;
kern_return_t
IOUserClient::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOUserClient::_Dispatch(IOUserClient * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOUserClient_CreateMemoryDescriptorFromClient_ID:
        {
            ret = IOUserClient::CreateMemoryDescriptorFromClient_Invoke(rpc, self, SimpleMemberFunctionCast(IOUserClient::CreateMemoryDescriptorFromClient_Handler, *self, &IOUserClient::CreateMemoryDescriptorFromClient_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOUserClient_CopyClientEntitlements_ID:
        {
            ret = IOUserClient::CopyClientEntitlements_Invoke(rpc, self, SimpleMemberFunctionCast(IOUserClient::CopyClientEntitlements_Handler, *self, &IOUserClient::CopyClientEntitlements_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOUserClient__ExternalMethod_ID:
        {
            ret = IOUserClient::_ExternalMethod_Invoke(rpc, self, SimpleMemberFunctionCast(IOUserClient::_ExternalMethod_Handler, *self, &IOUserClient::_ExternalMethod_Impl));
            break;
        }
#endif /* !KERNEL */

        default:
            ret = IOService::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IOUserClient::MetaClass::Dispatch(const IORPC rpc)
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

void
IOUserClient::AsyncCompletion(
        OSAction * action,
        IOReturn status,
        const IOUserClientAsyncArgumentsArray asyncData,
        uint32_t asyncDataCount,
        OSDispatchMethod supermethod)
{
    union
    {
        IOUserClient_AsyncCompletion_Msg msg;
    } buf;
    struct IOUserClient_AsyncCompletion_Msg * msg = &buf.msg;

    memset(msg, 0, sizeof(struct IOUserClient_AsyncCompletion_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 1*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOUserClient_AsyncCompletion_ID;
    msg->content.__object = (OSObjectRef) action;
    msg->content.__hdr.objectRefs = IOUserClient_AsyncCompletion_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->action__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.action = (OSObjectRef) action;

    msg->content.asyncData = NULL;

    if (asyncDataCount > (sizeof(msg->content.__asyncData) / sizeof(msg->content.__asyncData[0]))) return;
    bcopy(asyncData, &msg->content.__asyncData[0], asyncDataCount * sizeof(msg->content.__asyncData[0]));

    msg->content.status = status;

    msg->content.asyncDataCount = asyncDataCount;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    action->Invoke(rpc);
}

kern_return_t
IOUserClient::AsyncCompletion_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        AsyncCompletion_Handler func)
{
    return IOUserClient::AsyncCompletion_Invoke(_rpc, target, func, NULL);
}

kern_return_t
IOUserClient::AsyncCompletion_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        AsyncCompletion_Handler func,
        const OSMetaClass * targetActionClass)
{
    IOUserClient_AsyncCompletion_Invocation rpc = { _rpc };
    OSAction * action;

    if (IOUserClient_AsyncCompletion_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (targetActionClass) {
        action = (OSAction *) OSMetaClassBase::safeMetaCast((OSObject *) rpc.message->content.action, targetActionClass);
    } else {
        action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    }
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);

    (*func)(target,
        action,
        rpc.message->content.status,
        &rpc.message->content.__asyncData[0],
        rpc.message->content.asyncDataCount);

    return (kIOReturnSuccess);
}

kern_return_t
IOUserClient::CopyClientMemoryForType(
        uint64_t type,
        uint64_t * options,
        IOMemoryDescriptor ** memory,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOUserClient_CopyClientMemoryForType_Msg msg;
        struct
        {
            IOUserClient_CopyClientMemoryForType_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOUserClient_CopyClientMemoryForType_Msg * msg = &buf.msg;
    struct IOUserClient_CopyClientMemoryForType_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOUserClient_CopyClientMemoryForType_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOUserClient_CopyClientMemoryForType_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOUserClient_CopyClientMemoryForType_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.type = type;

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
            if (rpl->content.__hdr.msgid                  != IOUserClient_CopyClientMemoryForType_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOUserClient_CopyClientMemoryForType_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
        if (options) *options = rpl->content.options;
    }

    return (ret);
}

kern_return_t
IOUserClient::CopyClientMemoryForType_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyClientMemoryForType_Handler func)
{
    IOUserClient_CopyClientMemoryForType_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOUserClient_CopyClientMemoryForType_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.type,
        &rpc.reply->content.options,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOUserClient_CopyClientMemoryForType_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOUserClient_CopyClientMemoryForType_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOUserClient::CreateMemoryDescriptorFromClient(
        uint64_t memoryDescriptorCreateOptions,
        uint32_t segmentsCount,
        const IOAddressSegment * segments,
        IOMemoryDescriptor ** memory,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOUserClient_CreateMemoryDescriptorFromClient_Msg msg;
        struct
        {
            IOUserClient_CreateMemoryDescriptorFromClient_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOUserClient_CreateMemoryDescriptorFromClient_Msg * msg = &buf.msg;
    struct IOUserClient_CreateMemoryDescriptorFromClient_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOUserClient_CreateMemoryDescriptorFromClient_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOUserClient_CreateMemoryDescriptorFromClient_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOUserClient_CreateMemoryDescriptorFromClient_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.segments = NULL;

    if (segmentsCount > (sizeof(msg->content.__segments) / sizeof(msg->content.__segments[0]))) return kIOReturnOverrun;
    bcopy(segments, &msg->content.__segments[0], segmentsCount * sizeof(msg->content.__segments[0]));

    msg->content.memoryDescriptorCreateOptions = memoryDescriptorCreateOptions;

    msg->content.segmentsCount = segmentsCount;

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
            if (rpl->content.__hdr.msgid                  != IOUserClient_CreateMemoryDescriptorFromClient_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOUserClient_CreateMemoryDescriptorFromClient_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
IOUserClient::CreateMemoryDescriptorFromClient_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CreateMemoryDescriptorFromClient_Handler func)
{
    IOUserClient_CreateMemoryDescriptorFromClient_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOUserClient_CreateMemoryDescriptorFromClient_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.memoryDescriptorCreateOptions,
        rpc.message->content.segmentsCount,
        &rpc.message->content.__segments[0],
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOUserClient_CreateMemoryDescriptorFromClient_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOUserClient_CreateMemoryDescriptorFromClient_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOUserClient::CopyClientEntitlements(
        OSDictionary ** entitlements,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOUserClient_CopyClientEntitlements_Msg msg;
        struct
        {
            IOUserClient_CopyClientEntitlements_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOUserClient_CopyClientEntitlements_Msg * msg = &buf.msg;
    struct IOUserClient_CopyClientEntitlements_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOUserClient_CopyClientEntitlements_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOUserClient_CopyClientEntitlements_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOUserClient_CopyClientEntitlements_Msg_ObjRefs;
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
            if (rpl->content.__hdr.msgid                  != IOUserClient_CopyClientEntitlements_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOUserClient_CopyClientEntitlements_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *entitlements = OSDynamicCast(OSDictionary, (OSObject *) rpl->content.entitlements);
        if (rpl->content.entitlements && !*entitlements) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOUserClient::CopyClientEntitlements_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CopyClientEntitlements_Handler func)
{
    IOUserClient_CopyClientEntitlements_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOUserClient_CopyClientEntitlements_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        (OSDictionary **)&rpc.reply->content.entitlements);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOUserClient_CopyClientEntitlements_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOUserClient_CopyClientEntitlements_Rpl_ObjRefs;
    rpc.reply->entitlements__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOUserClient::_ExternalMethod(
        uint64_t selector,
        const IOUserClientScalarArray scalarInput,
        uint32_t scalarInputCount,
        OSData * structureInput,
        IOMemoryDescriptor * structureInputDescriptor,
        IOUserClientScalarArray scalarOutput,
        uint32_t * scalarOutputCount,
        uint64_t structureOutputMaximumSize,
        OSData ** structureOutput,
        IOMemoryDescriptor * structureOutputDescriptor,
        OSAction * completion,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOUserClient__ExternalMethod_Msg msg;
        struct
        {
            IOUserClient__ExternalMethod_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOUserClient__ExternalMethod_Msg * msg = &buf.msg;
    struct IOUserClient__ExternalMethod_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOUserClient__ExternalMethod_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOUserClient__ExternalMethod_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOUserClient__ExternalMethod_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 5;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->structureInput__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.structureInput = (OSObjectRef) structureInput;

    msg->structureInputDescriptor__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.structureInputDescriptor = (OSObjectRef) structureInputDescriptor;

    msg->structureOutputDescriptor__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.structureOutputDescriptor = (OSObjectRef) structureOutputDescriptor;

    msg->completion__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.completion = (OSObjectRef) completion;

    msg->content.scalarInput = NULL;

    if (scalarInputCount > (sizeof(msg->content.__scalarInput) / sizeof(msg->content.__scalarInput[0]))) return kIOReturnOverrun;
    bcopy(scalarInput, &msg->content.__scalarInput[0], scalarInputCount * sizeof(msg->content.__scalarInput[0]));

    if (*scalarOutputCount > (sizeof(rpl->content.__scalarOutput) / sizeof(rpl->content.__scalarOutput[0]))) return kIOReturnOverrun;
    msg->content.scalarOutputCount = *scalarOutputCount;

    msg->content.selector = selector;

    msg->content.scalarInputCount = scalarInputCount;

    msg->content.structureOutputMaximumSize = structureOutputMaximumSize;

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
            if (rpl->content.__hdr.msgid                  != IOUserClient__ExternalMethod_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOUserClient__ExternalMethod_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *structureOutput = OSDynamicCast(OSData, (OSObject *) rpl->content.structureOutput);
        if (rpl->content.structureOutput && !*structureOutput) ret = kIOReturnBadArgument;
        if (scalarOutputCount) *scalarOutputCount = rpl->content.scalarOutputCount;
        if (scalarOutputCount) {
            if (rpl->content.scalarOutputCount < *scalarOutputCount) *scalarOutputCount = rpl->content.scalarOutputCount;
            bcopy(&rpl->content.__scalarOutput[0], scalarOutput, *scalarOutputCount * sizeof(rpl->content.__scalarOutput[0]));
        }
    }

    return (ret);
}

kern_return_t
IOUserClient::_ExternalMethod_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _ExternalMethod_Handler func)
{
    IOUserClient__ExternalMethod_Invocation rpc = { _rpc };
    kern_return_t ret;
    OSData * structureInput;
    IOMemoryDescriptor * structureInputDescriptor;
    IOMemoryDescriptor * structureOutputDescriptor;
    OSAction * completion;
    uint32_t scalarOutputCount = (sizeof(rpc.reply->content.__scalarOutput) / sizeof(rpc.reply->content.__scalarOutput[0]));

    if (IOUserClient__ExternalMethod_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    structureInput = OSDynamicCast(OSData, (OSObject *) rpc.message->content.structureInput);
    if (!structureInput && rpc.message->content.structureInput) return (kIOReturnBadArgument);
    structureInputDescriptor = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.structureInputDescriptor);
    if (!structureInputDescriptor && rpc.message->content.structureInputDescriptor) return (kIOReturnBadArgument);
    structureOutputDescriptor = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.structureOutputDescriptor);
    if (!structureOutputDescriptor && rpc.message->content.structureOutputDescriptor) return (kIOReturnBadArgument);
    completion = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.completion);
    if (!completion && rpc.message->content.completion) return (kIOReturnBadArgument);
    if (scalarOutputCount > rpc.message->content.scalarOutputCount) scalarOutputCount = rpc.message->content.scalarOutputCount;

    ret = (*func)(target,
        rpc.message->content.selector,
        &rpc.message->content.__scalarInput[0],
        rpc.message->content.scalarInputCount,
        structureInput,
        structureInputDescriptor,
        &rpc.reply->content.__scalarOutput[0],
        &scalarOutputCount,
        rpc.message->content.structureOutputMaximumSize,
        (OSData **)&rpc.reply->content.structureOutput,
        structureOutputDescriptor,
        completion);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOUserClient__ExternalMethod_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOUserClient__ExternalMethod_Rpl_ObjRefs;
    rpc.reply->structureOutput__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    rpc.reply->content.scalarOutputCount = scalarOutputCount;

    return (ret);
}

kern_return_t
IOUserClient::CreateActionKernelCompletion(size_t referenceSize, OSAction ** action)
{
    return OSAction::Create(this, IOUserClient_KernelCompletion_ID, IOUserClient_AsyncCompletion_ID, referenceSize, action);
}

