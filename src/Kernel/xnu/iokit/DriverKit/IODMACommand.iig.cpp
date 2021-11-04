/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IODMACommand.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IODMACommand.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IODMACommand.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


struct IODMACommand_Create_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  device;
    uint64_t  options;
    IODMACommandSpecification  specification;
};
#pragma pack(4)
struct IODMACommand_Create_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t device__descriptor;
    IODMACommand_Create_Msg_Content content;
};
#pragma pack()
#define IODMACommand_Create_Msg_ObjRefs (2)

struct IODMACommand_Create_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  command;
};
#pragma pack(4)
struct IODMACommand_Create_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t command__descriptor;
    IODMACommand_Create_Rpl_Content content;
};
#pragma pack()
#define IODMACommand_Create_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODMACommand_Create_Msg * message;
        struct IODMACommand_Create_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODMACommand_Create_Invocation;
struct IODMACommand_PrepareForDMA_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  memory;
    uint64_t  options;
    uint64_t  offset;
    uint64_t  length;
    unsigned int  segmentsCount;
};
#pragma pack(4)
struct IODMACommand_PrepareForDMA_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t memory__descriptor;
    IODMACommand_PrepareForDMA_Msg_Content content;
};
#pragma pack()
#define IODMACommand_PrepareForDMA_Msg_ObjRefs (2)

struct IODMACommand_PrepareForDMA_Rpl_Content
{
    IORPCMessage __hdr;
    unsigned long long  flags;
    unsigned int  segmentsCount;
    IOAddressSegment *  segments;
#if !defined(__LP64__)
    uint32_t __segmentsPad;
#endif /* !defined(__LP64__) */
    IOAddressSegment __segments[32];
};
#pragma pack(4)
struct IODMACommand_PrepareForDMA_Rpl
{
    IORPCMessageMach           mach;
    IODMACommand_PrepareForDMA_Rpl_Content content;
};
#pragma pack()
#define IODMACommand_PrepareForDMA_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODMACommand_PrepareForDMA_Msg * message;
        struct IODMACommand_PrepareForDMA_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODMACommand_PrepareForDMA_Invocation;
struct IODMACommand_CompleteDMA_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    uint64_t  options;
};
#pragma pack(4)
struct IODMACommand_CompleteDMA_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODMACommand_CompleteDMA_Msg_Content content;
};
#pragma pack()
#define IODMACommand_CompleteDMA_Msg_ObjRefs (1)

struct IODMACommand_CompleteDMA_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODMACommand_CompleteDMA_Rpl
{
    IORPCMessageMach           mach;
    IODMACommand_CompleteDMA_Rpl_Content content;
};
#pragma pack()
#define IODMACommand_CompleteDMA_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODMACommand_CompleteDMA_Msg * message;
        struct IODMACommand_CompleteDMA_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODMACommand_CompleteDMA_Invocation;
struct IODMACommand_GetPreparation_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IODMACommand_GetPreparation_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IODMACommand_GetPreparation_Msg_Content content;
};
#pragma pack()
#define IODMACommand_GetPreparation_Msg_ObjRefs (1)

struct IODMACommand_GetPreparation_Rpl_Content
{
    IORPCMessage __hdr;
    OSObjectRef  memory;
    unsigned long long  offset;
    unsigned long long  length;
};
#pragma pack(4)
struct IODMACommand_GetPreparation_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t memory__descriptor;
    IODMACommand_GetPreparation_Rpl_Content content;
};
#pragma pack()
#define IODMACommand_GetPreparation_Rpl_ObjRefs (1)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODMACommand_GetPreparation_Msg * message;
        struct IODMACommand_GetPreparation_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODMACommand_GetPreparation_Invocation;
struct IODMACommand_PerformOperation_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSObjectRef  data;
    uint64_t  options;
    uint64_t  dmaOffset;
    uint64_t  length;
    uint64_t  dataOffset;
};
#pragma pack(4)
struct IODMACommand_PerformOperation_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_port_descriptor_t data__descriptor;
    IODMACommand_PerformOperation_Msg_Content content;
};
#pragma pack()
#define IODMACommand_PerformOperation_Msg_ObjRefs (2)

struct IODMACommand_PerformOperation_Rpl_Content
{
    IORPCMessage __hdr;
};
#pragma pack(4)
struct IODMACommand_PerformOperation_Rpl
{
    IORPCMessageMach           mach;
    IODMACommand_PerformOperation_Rpl_Content content;
};
#pragma pack()
#define IODMACommand_PerformOperation_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IODMACommand_PerformOperation_Msg * message;
        struct IODMACommand_PerformOperation_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IODMACommand_PerformOperation_Invocation;
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gIOMemoryMapMetaClass;
extern OSMetaClass * gIOBufferMemoryDescriptorMetaClass;
extern OSMetaClass * gIOUserClientMetaClass;
extern OSMetaClass * gOSActionMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IODMACommand_QueueNames  ""

#define IODMACommand_MethodNames  ""

#define IODMACommandMetaClass_MethodNames  ""

struct OSClassDescription_IODMACommand_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IODMACommand_QueueNames)];
    char               methodNames[sizeof(IODMACommand_MethodNames)];
    char               metaMethodNames[sizeof(IODMACommandMetaClass_MethodNames)];
};

const struct OSClassDescription_IODMACommand_t
OSClassDescription_IODMACommand =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IODMACommand_t),
        .name                    = "IODMACommand",
        .superName               = "OSObject",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IODMACommand_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IODMACommand_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IODMACommand_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IODMACommand_t, queueNames),
        .methodNamesSize         = sizeof(IODMACommand_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IODMACommand_t, methodNames),
        .metaMethodNamesSize     = sizeof(IODMACommandMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IODMACommand_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IODMACommand_QueueNames,
    .methodNames     = IODMACommand_MethodNames,
    .metaMethodNames = IODMACommandMetaClass_MethodNames,
};

OSMetaClass * gIODMACommandMetaClass;

static kern_return_t
IODMACommand_New(OSMetaClass * instance);

const OSClassLoadInformation
IODMACommand_Class = 
{
    .description       = &OSClassDescription_IODMACommand.base,
    .metaPointer       = &gIODMACommandMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IODMACommand),

    .New               = &IODMACommand_New,
};

extern const void * const
gIODMACommand_Declaration;
const void * const
gIODMACommand_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IODMACommand_Class;

static kern_return_t
IODMACommand_New(OSMetaClass * instance)
{
    if (!new(instance) IODMACommandMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IODMACommandMetaClass::New(OSObject * instance)
{
    if (!new(instance) IODMACommand) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
IODMACommand::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IODMACommand::_Dispatch(IODMACommand * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODMACommand_PrepareForDMA_ID:
        {
            ret = IODMACommand::PrepareForDMA_Invoke(rpc, self, SimpleMemberFunctionCast(IODMACommand::PrepareForDMA_Handler, *self, &IODMACommand::PrepareForDMA_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODMACommand_CompleteDMA_ID:
        {
            ret = IODMACommand::CompleteDMA_Invoke(rpc, self, SimpleMemberFunctionCast(IODMACommand::CompleteDMA_Handler, *self, &IODMACommand::CompleteDMA_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODMACommand_GetPreparation_ID:
        {
            ret = IODMACommand::GetPreparation_Invoke(rpc, self, SimpleMemberFunctionCast(IODMACommand::GetPreparation_Handler, *self, &IODMACommand::GetPreparation_Impl));
            break;
        }
#endif /* !KERNEL */
#if KERNEL
        case IODMACommand_PerformOperation_ID:
        {
            ret = IODMACommand::PerformOperation_Invoke(rpc, self, SimpleMemberFunctionCast(IODMACommand::PerformOperation_Handler, *self, &IODMACommand::PerformOperation_Impl));
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
IODMACommand::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
IODMACommandMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODMACommand_Create_ID:
            ret = IODMACommand::Create_Invoke(rpc, &IODMACommand::Create_Impl);
            break;
#endif /* !KERNEL */

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

kern_return_t
IODMACommand::Create(
        IOService * device,
        uint64_t options,
        const IODMACommandSpecification * specification,
        IODMACommand ** command)
{
    kern_return_t ret;
    union
    {
        IODMACommand_Create_Msg msg;
        struct
        {
            IODMACommand_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODMACommand_Create_Msg * msg = &buf.msg;
    struct IODMACommand_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODMACommand_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODMACommand_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(IODMACommand);
    msg->content.__hdr.objectRefs = IODMACommand_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->device__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.device = (OSObjectRef) device;

    msg->content.options = options;

    msg->content.specification = *specification;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IODMACommand)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODMACommand_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODMACommand_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *command = OSDynamicCast(IODMACommand, (OSObject *) rpl->content.command);
        if (rpl->content.command && !*command) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IODMACommand::PrepareForDMA(
        uint64_t options,
        IOMemoryDescriptor * memory,
        uint64_t offset,
        uint64_t length,
        uint64_t * flags,
        uint32_t * segmentsCount,
        IOAddressSegment * segments,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODMACommand_PrepareForDMA_Msg msg;
        struct
        {
            IODMACommand_PrepareForDMA_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODMACommand_PrepareForDMA_Msg * msg = &buf.msg;
    struct IODMACommand_PrepareForDMA_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODMACommand_PrepareForDMA_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODMACommand_PrepareForDMA_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODMACommand_PrepareForDMA_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.memory = (OSObjectRef) memory;

    msg->content.offset = offset;

    msg->content.length = length;

    if (*segmentsCount > (sizeof(rpl->content.__segments) / sizeof(rpl->content.__segments[0]))) return kIOReturnOverrun;
    msg->content.segmentsCount = *segmentsCount;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODMACommand_PrepareForDMA_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODMACommand_PrepareForDMA_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (flags) *flags = rpl->content.flags;
        if (rpl->content.segmentsCount < *segmentsCount) *segmentsCount = rpl->content.segmentsCount;
        bcopy(&rpl->content.__segments[0], segments, *segmentsCount * sizeof(rpl->content.__segments[0]));
    }


    return (ret);
}

kern_return_t
IODMACommand::CompleteDMA(
        uint64_t options,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODMACommand_CompleteDMA_Msg msg;
        struct
        {
            IODMACommand_CompleteDMA_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODMACommand_CompleteDMA_Msg * msg = &buf.msg;
    struct IODMACommand_CompleteDMA_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODMACommand_CompleteDMA_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODMACommand_CompleteDMA_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODMACommand_CompleteDMA_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODMACommand_CompleteDMA_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODMACommand_CompleteDMA_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }


    return (ret);
}

kern_return_t
IODMACommand::GetPreparation(
        uint64_t * offset,
        uint64_t * length,
        IOMemoryDescriptor ** memory,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODMACommand_GetPreparation_Msg msg;
        struct
        {
            IODMACommand_GetPreparation_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODMACommand_GetPreparation_Msg * msg = &buf.msg;
    struct IODMACommand_GetPreparation_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODMACommand_GetPreparation_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODMACommand_GetPreparation_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODMACommand_GetPreparation_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODMACommand_GetPreparation_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (IODMACommand_GetPreparation_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        if (offset) *offset = rpl->content.offset;
        if (length) *length = rpl->content.length;
        *memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IODMACommand::PerformOperation(
        uint64_t options,
        uint64_t dmaOffset,
        uint64_t length,
        uint64_t dataOffset,
        IOMemoryDescriptor * data,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IODMACommand_PerformOperation_Msg msg;
        struct
        {
            IODMACommand_PerformOperation_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IODMACommand_PerformOperation_Msg * msg = &buf.msg;
    struct IODMACommand_PerformOperation_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IODMACommand_PerformOperation_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IODMACommand_PerformOperation_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IODMACommand_PerformOperation_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->content.options = options;

    msg->content.dmaOffset = dmaOffset;

    msg->content.length = length;

    msg->content.dataOffset = dataOffset;

    msg->data__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.data = (OSObjectRef) data;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IODMACommand_PerformOperation_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IODMACommand_PerformOperation_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
    }


    return (ret);
}

kern_return_t
IODMACommand::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IODMACommand_Create_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOService * device;

    if (IODMACommand_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    device = OSDynamicCast(IOService, (OSObject *) rpc.message->content.device);
    if (!device && rpc.message->content.device) return (kIOReturnBadArgument);

    ret = (*func)(        device,
        rpc.message->content.options,
        &rpc.message->content.specification,
        (IODMACommand **)&rpc.reply->content.command);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODMACommand_Create_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODMACommand_Create_Rpl_ObjRefs;
    rpc.reply->command__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODMACommand::PrepareForDMA_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        PrepareForDMA_Handler func)
{
    IODMACommand_PrepareForDMA_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOMemoryDescriptor * memory;
    unsigned int segmentsCount = (sizeof(rpc.reply->content.__segments) / sizeof(rpc.reply->content.__segments[0]));

    if (IODMACommand_PrepareForDMA_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.memory);
    if (!memory && rpc.message->content.memory) return (kIOReturnBadArgument);
    if (segmentsCount > rpc.message->content.segmentsCount) segmentsCount = rpc.message->content.segmentsCount;

    ret = (*func)(target,
        rpc.message->content.options,
        memory,
        rpc.message->content.offset,
        rpc.message->content.length,
        &rpc.reply->content.flags,
        &segmentsCount,
        &rpc.reply->content.__segments[0]);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODMACommand_PrepareForDMA_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODMACommand_PrepareForDMA_Rpl_ObjRefs;
    rpc.reply->content.segmentsCount = segmentsCount;

    return (ret);
}

kern_return_t
IODMACommand::CompleteDMA_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CompleteDMA_Handler func)
{
    IODMACommand_CompleteDMA_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODMACommand_CompleteDMA_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        rpc.message->content.options);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODMACommand_CompleteDMA_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODMACommand_CompleteDMA_Rpl_ObjRefs;

    return (ret);
}

kern_return_t
IODMACommand::GetPreparation_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        GetPreparation_Handler func)
{
    IODMACommand_GetPreparation_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODMACommand_GetPreparation_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.offset,
        &rpc.reply->content.length,
        (IOMemoryDescriptor **)&rpc.reply->content.memory);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODMACommand_GetPreparation_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IODMACommand_GetPreparation_Rpl_ObjRefs;
    rpc.reply->memory__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    return (ret);
}

kern_return_t
IODMACommand::PerformOperation_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        PerformOperation_Handler func)
{
    IODMACommand_PerformOperation_Invocation rpc = { _rpc };
    kern_return_t ret;
    IOMemoryDescriptor * data;

    if (IODMACommand_PerformOperation_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    data = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.data);
    if (!data && rpc.message->content.data) return (kIOReturnBadArgument);

    ret = (*func)(target,
        rpc.message->content.options,
        rpc.message->content.dmaOffset,
        rpc.message->content.length,
        rpc.message->content.dataOffset,
        data);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IODMACommand_PerformOperation_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IODMACommand_PerformOperation_Rpl_ObjRefs;

    return (ret);
}



