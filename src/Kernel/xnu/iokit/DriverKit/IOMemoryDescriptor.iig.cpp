/* iig-lite generated from IOMemoryDescriptor.iig - kernel-side subset; msgids are NOT Apple-ABI */

#undef IIG_IMPLEMENTATION
#define IIG_IMPLEMENTATION 	IOMemoryDescriptor.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <IOKit/IORPC.h>
#include "IOMemoryDescriptor.h"

/* @iig implementation */
#include <DriverKit/IOService.h>
/* @iig end */

#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif

struct IOMemoryDescriptor__CopyState_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOMemoryDescriptor__CopyState_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOMemoryDescriptor__CopyState_Msg_Content content;
};
#pragma pack()
#define IOMemoryDescriptor__CopyState_Msg_ObjRefs (1)

struct IOMemoryDescriptor__CopyState_Rpl_Content
{
    IORPCMessage __hdr;
    _IOMDPrivateState  state;
};
#pragma pack(4)
struct IOMemoryDescriptor__CopyState_Rpl
{
    IORPCMessageMach           mach;
    IOMemoryDescriptor__CopyState_Rpl_Content content;
};
#pragma pack()
#define IOMemoryDescriptor__CopyState_Rpl_ObjRefs (0)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOMemoryDescriptor__CopyState_Msg * message;
        struct IOMemoryDescriptor__CopyState_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOMemoryDescriptor__CopyState_Invocation;
struct IOMemoryDescriptor_CreateMapping_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
    uint64_t  address;
    uint64_t  offset;
    uint64_t  length;
    uint64_t  alignment;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateMapping_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOMemoryDescriptor_CreateMapping_Msg_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateMapping_Msg_ObjRefs (1)

struct IOMemoryDescriptor_CreateMapping_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  map;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateMapping_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t map__descriptor;
    IOMemoryDescriptor_CreateMapping_Rpl_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateMapping_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOMemoryDescriptor_CreateMapping_Msg * message;
        struct IOMemoryDescriptor_CreateMapping_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOMemoryDescriptor_CreateMapping_Invocation;
struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  ofDescriptor;
    uint64_t  memoryDescriptorCreateOptions;
    uint64_t  offset;
    uint64_t  length;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t ofDescriptor__descriptor;
    IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg_ObjRefs (2)

struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg * message;
        struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOMemoryDescriptor_CreateSubMemoryDescriptor_Invocation;
struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef __withDescriptors[32];
    uint64_t  memoryDescriptorCreateOptions;
    uint32_t  withDescriptorsCount;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t withDescriptors__descriptor[32];
    IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg_ObjRefs (33)

struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
};
#pragma pack(4)
struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl_Content content;
};
#pragma pack()
#define IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl_ObjRefs (1)

typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg * message;
        struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOMemoryDescriptor_CreateWithMemoryDescriptors_Invocation;
kern_return_t
IOMemoryDescriptor::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOMemoryDescriptor::_Dispatch(IOMemoryDescriptor * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOMemoryDescriptor__CopyState_ID:
        {
            ret = IOMemoryDescriptor::_CopyState_Invoke(rpc, self, SimpleMemberFunctionCast(IOMemoryDescriptor::_CopyState_Handler, *self, &IOMemoryDescriptor::_CopyState_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IOMemoryDescriptor_CreateMapping_ID:
        {
            ret = IOMemoryDescriptor::CreateMapping_Invoke(rpc, self, SimpleMemberFunctionCast(IOMemoryDescriptor::CreateMapping_Handler, *self, &IOMemoryDescriptor::CreateMapping_Impl));
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
IOMemoryDescriptor::MetaClass::Dispatch(const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
        case IOMemoryDescriptor_CreateSubMemoryDescriptor_ID:
            ret = IOMemoryDescriptor::CreateSubMemoryDescriptor_Invoke(rpc, &IOMemoryDescriptor::CreateSubMemoryDescriptor_Impl);
            break;
        case IOMemoryDescriptor_CreateWithMemoryDescriptors_ID:
            ret = IOMemoryDescriptor::CreateWithMemoryDescriptors_Invoke(rpc, &IOMemoryDescriptor::CreateWithMemoryDescriptors_Impl);
            break;

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}
#endif /* KERNEL */

kern_return_t
IOMemoryDescriptor::_CopyState(
        _IOMDPrivateState * state,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOMemoryDescriptor__CopyState_Msg msg;
        struct
        {
            IOMemoryDescriptor__CopyState_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOMemoryDescriptor__CopyState_Msg * msg = &buf.msg;
    struct IOMemoryDescriptor__CopyState_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOMemoryDescriptor__CopyState_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOMemoryDescriptor__CopyState_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOMemoryDescriptor__CopyState_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOMemoryDescriptor__CopyState_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOMemoryDescriptor__CopyState_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (state) *state = rpl->content.state;
    }

    return (ret);
}

kern_return_t
IOMemoryDescriptor::_CopyState_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _CopyState_Handler func)
{
    IOMemoryDescriptor__CopyState_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOMemoryDescriptor__CopyState_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.state);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOMemoryDescriptor__CopyState_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOMemoryDescriptor__CopyState_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IOMemoryDescriptor::CreateMapping(
        uint64_t options,
        uint64_t address,
        uint64_t offset,
        uint64_t length,
        uint64_t alignment,
        IOMemoryMap ** map,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOMemoryDescriptor_CreateMapping_Msg msg;
        struct
        {
            IOMemoryDescriptor_CreateMapping_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOMemoryDescriptor_CreateMapping_Msg * msg = &buf.msg;
    struct IOMemoryDescriptor_CreateMapping_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOMemoryDescriptor_CreateMapping_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOMemoryDescriptor_CreateMapping_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOMemoryDescriptor_CreateMapping_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->content.address = address;

    msg->content.offset = offset;

    msg->content.length = length;

    msg->content.alignment = alignment;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, _rpc);
    else             ret = ((OSObject *)this)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOMemoryDescriptor_CreateMapping_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOMemoryDescriptor_CreateMapping_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *map = OSDynamicCast(IOMemoryMap, (OSObject *) rpl->content.map);
        if (rpl->content.map && !*map) ret = kIOReturnBadArgument;
    }

    return (ret);
}

kern_return_t
IOMemoryDescriptor::CreateMapping_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CreateMapping_Handler func)
{
    IOMemoryDescriptor_CreateMapping_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOMemoryDescriptor_CreateMapping_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.options,
        rpc.message->content.address,
        rpc.message->content.offset,
        rpc.message->content.length,
        rpc.message->content.alignment,
        (IOMemoryMap **)&rpc.reply->content.map);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOMemoryDescriptor_CreateMapping_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOMemoryDescriptor_CreateMapping_Rpl_ObjRefs;
    rpc.reply->map__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOMemoryDescriptor::CreateSubMemoryDescriptor(
        uint64_t memoryDescriptorCreateOptions,
        uint64_t offset,
        uint64_t length,
        IOMemoryDescriptor * ofDescriptor,
        IOMemoryDescriptor ** memory)
{
    kern_return_t ret;
    union
    {
        IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg msg;
        struct
        {
            IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg * msg = &buf.msg;
    struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOMemoryDescriptor_CreateSubMemoryDescriptor_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOMemoryDescriptor);
    msg->content.__hdr.objectRefs = IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->ofDescriptor__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.ofDescriptor = (OSObjectRef) ofDescriptor;

    msg->content.memoryDescriptorCreateOptions = memoryDescriptorCreateOptions;

    msg->content.offset = offset;

    msg->content.length = length;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOMemoryDescriptor)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOMemoryDescriptor_CreateSubMemoryDescriptor_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
IOMemoryDescriptor::CreateSubMemoryDescriptor_Invoke(const IORPC _rpc,
        CreateSubMemoryDescriptor_Handler func)
{
    IOMemoryDescriptor_CreateSubMemoryDescriptor_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOMemoryDescriptor * ofDescriptor;

    if (IOMemoryDescriptor_CreateSubMemoryDescriptor_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    ofDescriptor = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.ofDescriptor);
    if (!ofDescriptor && rpc.message->content.ofDescriptor) return (kIOReturnBadArgument);

    ret = (*func)(rpc.message->content.memoryDescriptorCreateOptions,
        rpc.message->content.offset,
        rpc.message->content.length,
        ofDescriptor,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOMemoryDescriptor_CreateSubMemoryDescriptor_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOMemoryDescriptor_CreateSubMemoryDescriptor_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOMemoryDescriptor::CreateWithMemoryDescriptors(
        uint64_t memoryDescriptorCreateOptions,
        uint32_t withDescriptorsCount,
        IOMemoryDescriptor ** const withDescriptors,
        IOMemoryDescriptor ** memory)
{
    kern_return_t ret;
    union
    {
        IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg msg;
        struct
        {
            IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg * msg = &buf.msg;
    struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOMemoryDescriptor_CreateWithMemoryDescriptors_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOMemoryDescriptor);
    msg->content.__hdr.objectRefs = IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 33;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    for (unsigned int __i = 0; __i < 32; __i++)
    {
        msg->withDescriptors__descriptor[__i].type = MACH_MSG_PORT_DESCRIPTOR;
        msg->content.__withDescriptors[__i] = (OSObjectRef) withDescriptors[__i];
    }

    msg->content.memoryDescriptorCreateOptions = memoryDescriptorCreateOptions;

    msg->content.withDescriptorsCount = withDescriptorsCount;

    IORPC _rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOMemoryDescriptor)->Invoke(_rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOMemoryDescriptor_CreateWithMemoryDescriptors_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
IOMemoryDescriptor::CreateWithMemoryDescriptors_Invoke(const IORPC _rpc,
        CreateWithMemoryDescriptors_Handler func)
{
    IOMemoryDescriptor_CreateWithMemoryDescriptors_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOMemoryDescriptor ** withDescriptors;

    if (IOMemoryDescriptor_CreateWithMemoryDescriptors_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    withDescriptors = (IOMemoryDescriptor **) &rpc.message->content.__withDescriptors[0];
    for (unsigned int __i = 0; __i < 32; __i++)
    {
        if (withDescriptors[__i] && !OSDynamicCast(IOMemoryDescriptor, withDescriptors[__i])) return (kIOReturnBadArgument);
    }

    ret = (*func)(rpc.message->content.memoryDescriptorCreateOptions,
        rpc.message->content.withDescriptorsCount,
        withDescriptors,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOMemoryDescriptor_CreateWithMemoryDescriptors_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOMemoryDescriptor_CreateWithMemoryDescriptors_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

