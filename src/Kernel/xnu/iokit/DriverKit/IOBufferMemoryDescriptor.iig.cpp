/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IOBufferMemoryDescriptor.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IOBufferMemoryDescriptor.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


struct IOBufferMemoryDescriptor_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
    uint64_t  capacity;
    uint64_t  alignment;
};
#pragma pack(4)
struct IOBufferMemoryDescriptor_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOBufferMemoryDescriptor_Create_Msg_Content content;
};
#pragma pack()
#define IOBufferMemoryDescriptor_Create_Msg_ObjRefs (1)

struct IOBufferMemoryDescriptor_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
};
#pragma pack(4)
struct IOBufferMemoryDescriptor_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IOBufferMemoryDescriptor_Create_Rpl_Content content;
};
#pragma pack()
#define IOBufferMemoryDescriptor_Create_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOBufferMemoryDescriptor_Create_Msg * message;
        struct IOBufferMemoryDescriptor_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOBufferMemoryDescriptor_Create_Invocation;
struct IOBufferMemoryDescriptor_SetLength_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  length;
};
#pragma pack(4)
struct IOBufferMemoryDescriptor_SetLength_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOBufferMemoryDescriptor_SetLength_Msg_Content content;
};
#pragma pack()
#define IOBufferMemoryDescriptor_SetLength_Msg_ObjRefs (1)

struct IOBufferMemoryDescriptor_SetLength_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IOBufferMemoryDescriptor_SetLength_Rpl
{
    IORPCMessageMach           mach;
    IOBufferMemoryDescriptor_SetLength_Rpl_Content content;
};
#pragma pack()
#define IOBufferMemoryDescriptor_SetLength_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOBufferMemoryDescriptor_SetLength_Msg * message;
        struct IOBufferMemoryDescriptor_SetLength_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOBufferMemoryDescriptor_SetLength_Invocation;
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gIOServiceMetaClass;
extern OSMetaClass * gIOMemoryMapMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IOBufferMemoryDescriptor_QueueNames  ""

#define IOBufferMemoryDescriptor_MethodNames  ""

#define IOBufferMemoryDescriptorMetaClass_MethodNames  ""

struct OSClassDescription_IOBufferMemoryDescriptor_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IOBufferMemoryDescriptor_QueueNames)];
    char               methodNames[sizeof(IOBufferMemoryDescriptor_MethodNames)];
    char               metaMethodNames[sizeof(IOBufferMemoryDescriptorMetaClass_MethodNames)];
};

const struct OSClassDescription_IOBufferMemoryDescriptor_t
OSClassDescription_IOBufferMemoryDescriptor =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IOBufferMemoryDescriptor_t),
        .name                    = "IOBufferMemoryDescriptor",
        .superName               = "IOMemoryDescriptor",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IOBufferMemoryDescriptor_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IOBufferMemoryDescriptor_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IOBufferMemoryDescriptor_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IOBufferMemoryDescriptor_t, queueNames),
        .methodNamesSize         = sizeof(IOBufferMemoryDescriptor_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IOBufferMemoryDescriptor_t, methodNames),
        .metaMethodNamesSize     = sizeof(IOBufferMemoryDescriptorMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IOBufferMemoryDescriptor_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IOBufferMemoryDescriptor_QueueNames,
    .methodNames     = IOBufferMemoryDescriptor_MethodNames,
    .metaMethodNames = IOBufferMemoryDescriptorMetaClass_MethodNames,
};

OSMetaClass * gIOBufferMemoryDescriptorMetaClass;

static kern_return_t
IOBufferMemoryDescriptor_New(OSMetaClass * instance);

const OSClassLoadInformation
IOBufferMemoryDescriptor_Class = 
{
    .description       = &OSClassDescription_IOBufferMemoryDescriptor.base,
    .metaPointer       = &gIOBufferMemoryDescriptorMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IOBufferMemoryDescriptor),

    .New               = &IOBufferMemoryDescriptor_New,
};

extern const void * const
gIOBufferMemoryDescriptor_Declaration;
const void * const
gIOBufferMemoryDescriptor_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IOBufferMemoryDescriptor_Class;

static kern_return_t
IOBufferMemoryDescriptor_New(OSMetaClass * instance)
{
    if (!new(instance) IOBufferMemoryDescriptorMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IOBufferMemoryDescriptorMetaClass::New(OSObject * instance)
{
    if (!new(instance) IOBufferMemoryDescriptor) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
IOBufferMemoryDescriptor::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOBufferMemoryDescriptor::_Dispatch(IOBufferMemoryDescriptor * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOBufferMemoryDescriptor_SetLength_ID:
        {
            ret = IOBufferMemoryDescriptor::SetLength_Invoke(rpc, self, SimpleMemberFunctionCast(IOBufferMemoryDescriptor::SetLength_Handler, *self, &IOBufferMemoryDescriptor::SetLength_Impl));
            break;
        }
#endif /* !KERNEL */

        default:
            ret = IOMemoryDescriptor::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
IOBufferMemoryDescriptor::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
IOBufferMemoryDescriptorMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOBufferMemoryDescriptor_Create_ID:
            ret = IOBufferMemoryDescriptor::Create_Invoke(rpc, &IOBufferMemoryDescriptor::Create_Impl);
            break;
#endif /* !KERNEL */

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

kern_return_t
IOBufferMemoryDescriptor::Create(
        uint64_t options,
        uint64_t capacity,
        uint64_t alignment,
        IOBufferMemoryDescriptor ** memory)
{
    kern_return_t ret;
    union
    {
        IOBufferMemoryDescriptor_Create_Msg msg;
        struct
        {
            IOBufferMemoryDescriptor_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOBufferMemoryDescriptor_Create_Msg * msg = &buf.msg;
    struct IOBufferMemoryDescriptor_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOBufferMemoryDescriptor_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOBufferMemoryDescriptor_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IOBufferMemoryDescriptor);
    msg->content.__hdr.objectRefs = IOBufferMemoryDescriptor_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->content.capacity = capacity;

    msg->content.alignment = alignment;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IOBufferMemoryDescriptor)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOBufferMemoryDescriptor_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IOBufferMemoryDescriptor_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *memory = OSDynamicCast(IOBufferMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IOBufferMemoryDescriptor::SetLength(
        uint64_t length,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOBufferMemoryDescriptor_SetLength_Msg msg;
        struct
        {
            IOBufferMemoryDescriptor_SetLength_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOBufferMemoryDescriptor_SetLength_Msg * msg = &buf.msg;
    struct IOBufferMemoryDescriptor_SetLength_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOBufferMemoryDescriptor_SetLength_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOBufferMemoryDescriptor_SetLength_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOBufferMemoryDescriptor_SetLength_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.length = length;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOBufferMemoryDescriptor_SetLength_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOBufferMemoryDescriptor_SetLength_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }


    return (ret);
}

kern_return_t
IOBufferMemoryDescriptor::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IOBufferMemoryDescriptor_Create_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOBufferMemoryDescriptor_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(        rpc.message->content.options,
        rpc.message->content.capacity,
        rpc.message->content.alignment,
        (IOBufferMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOBufferMemoryDescriptor_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOBufferMemoryDescriptor_Create_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IOBufferMemoryDescriptor::SetLength_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        SetLength_Handler func)
{
    IOBufferMemoryDescriptor_SetLength_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOBufferMemoryDescriptor_SetLength_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.length);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOBufferMemoryDescriptor_SetLength_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOBufferMemoryDescriptor_SetLength_Rpl_ObjRefs;

    return (ret);
}



