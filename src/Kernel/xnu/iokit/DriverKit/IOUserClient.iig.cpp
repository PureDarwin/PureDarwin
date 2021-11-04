/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IOUserClient.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IOUserClient.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IOUserClient.h>

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
    IOReturn  status;
    const unsigned long long *  asyncData;
#if !defined(__LP64__)
    uint32_t __asyncDataPad;
#endif /* !defined(__LP64__) */
    unsigned long long __asyncData[16];
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
    unsigned long long  options;
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
    uint64_t  memoryDescriptorCreateOptions;
    uint32_t  segmentsCount;
    const IOAddressSegment *  segments;
#if !defined(__LP64__)
    uint32_t __segmentsPad;
#endif /* !defined(__LP64__) */
    IOAddressSegment __segments[32];
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
struct IOUserClient__ExternalMethod_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
    OSData * structureInput;
#if !defined(__LP64__)
    uint32_t __structureInputPad;
#endif /* !defined(__LP64__) */
    OSObjectRef  structureInputDescriptor;
    OSObjectRef  structureOutputDescriptor;
    OSObjectRef  completion;
    uint64_t  selector;
    const unsigned long long *  scalarInput;
#if !defined(__LP64__)
    uint32_t __scalarInputPad;
#endif /* !defined(__LP64__) */
    unsigned long long __scalarInput[16];
    uint32_t  scalarInputCount;
    unsigned int  scalarOutputCount;
    uint64_t  structureOutputMaximumSize;
};
#pragma pack(4)
struct IOUserClient__ExternalMethod_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    mach_msg_ool_descriptor_t  structureInput__descriptor;
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
    OSData * structureOutput;
#if !defined(__LP64__)
    uint32_t __structureOutputPad;
#endif /* !defined(__LP64__) */
    unsigned long long *  scalarOutput;
#if !defined(__LP64__)
    uint32_t __scalarOutputPad;
#endif /* !defined(__LP64__) */
    unsigned long long __scalarOutput[16];
    unsigned int  scalarOutputCount;
};
#pragma pack(4)
struct IOUserClient__ExternalMethod_Rpl
{
    IORPCMessageMach           mach;
    mach_msg_ool_descriptor_t  structureOutput__descriptor;
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
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gIOMemoryMapMetaClass;
extern OSMetaClass * gOSAction_IOUserClient_KernelCompletionMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IOUserClient_QueueNames  ""

#define IOUserClient_MethodNames  ""

#define IOUserClientMetaClass_MethodNames  ""

struct OSClassDescription_IOUserClient_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IOUserClient_QueueNames)];
    char               methodNames[sizeof(IOUserClient_MethodNames)];
    char               metaMethodNames[sizeof(IOUserClientMetaClass_MethodNames)];
};

const struct OSClassDescription_IOUserClient_t
OSClassDescription_IOUserClient =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IOUserClient_t),
        .name                    = "IOUserClient",
        .superName               = "IOService",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IOUserClient_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IOUserClient_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IOUserClient_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IOUserClient_t, queueNames),
        .methodNamesSize         = sizeof(IOUserClient_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IOUserClient_t, methodNames),
        .metaMethodNamesSize     = sizeof(IOUserClientMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IOUserClient_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IOUserClient_QueueNames,
    .methodNames     = IOUserClient_MethodNames,
    .metaMethodNames = IOUserClientMetaClass_MethodNames,
};

OSMetaClass * gIOUserClientMetaClass;

static kern_return_t
IOUserClient_New(OSMetaClass * instance);

const OSClassLoadInformation
IOUserClient_Class = 
{
    .description       = &OSClassDescription_IOUserClient.base,
    .metaPointer       = &gIOUserClientMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IOUserClient),

    .New               = &IOUserClient_New,
};

extern const void * const
gIOUserClient_Declaration;
const void * const
gIOUserClient_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IOUserClient_Class;

static kern_return_t
IOUserClient_New(OSMetaClass * instance)
{
    if (!new(instance) IOUserClientMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IOUserClientMetaClass::New(OSObject * instance)
{
    if (!new(instance) IOUserClient) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

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
        case IOUserClient__ExternalMethod_ID:
        {
            ret = IOUserClient::_ExternalMethod_Invoke(rpc, self, SimpleMemberFunctionCast(IOUserClient::_ExternalMethod_Handler, *self, &IOUserClient::_ExternalMethod_Impl));
            break;
        }
#if KERNEL
        case IOUserClient_KernelCompletion_ID:
        {
            ret = IOUserClient::AsyncCompletion_Invoke(rpc, self, SimpleMemberFunctionCast(IOUserClient::AsyncCompletion_Handler, *self, &IOUserClient::KernelCompletion_Impl), OSTypeID(OSAction_IOUserClient_KernelCompletion));
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
#else /* KERNEL */
kern_return_t
IOUserClientMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

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

void
IOUserClient::AsyncCompletion(
        OSAction * action,
        IOReturn status,
        const unsigned long long * asyncData,
        uint32_t asyncDataCount,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
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

    msg->content.status = status;

    msg->content.asyncData = NULL;

    if (asyncDataCount > (sizeof(msg->content.__asyncData) / sizeof(msg->content.__asyncData[0]))) return;
    bcopy(asyncData, &msg->content.__asyncData[0], asyncDataCount * sizeof(msg->content.__asyncData[0]));

    msg->content.asyncDataCount = asyncDataCount;

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    ret = action->Invoke(rpc);

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
        if (options) *options = rpl->content.options;
        *memory = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpl->content.memory);
        if (rpl->content.memory && !*memory) ret = kIOReturnBadArgument;
    }


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

    msg->content.memoryDescriptorCreateOptions = memoryDescriptorCreateOptions;

    msg->content.segmentsCount = segmentsCount;

    msg->content.segments = NULL;

    if (segmentsCount > (sizeof(msg->content.__segments) / sizeof(msg->content.__segments[0]))) return kIOReturnOverrun;
    bcopy(segments, &msg->content.__segments[0], segmentsCount * sizeof(msg->content.__segments[0]));

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
IOUserClient::_ExternalMethod(
        uint64_t selector,
        const unsigned long long * scalarInput,
        uint32_t scalarInputCount,
        OSData * structureInput,
        IOMemoryDescriptor * structureInputDescriptor,
        unsigned long long * scalarOutput,
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

    msg->content.selector = selector;

    msg->content.scalarInput = NULL;

    if (scalarInputCount > (sizeof(msg->content.__scalarInput) / sizeof(msg->content.__scalarInput[0]))) return kIOReturnOverrun;
    bcopy(scalarInput, &msg->content.__scalarInput[0], scalarInputCount * sizeof(msg->content.__scalarInput[0]));

    msg->content.scalarInputCount = scalarInputCount;

    msg->structureInput__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    msg->structureInput__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    msg->structureInput__descriptor.address = (void *) __builtin_offsetof(IOUserClient__ExternalMethod_Msg_Content, structureInput);
    msg->content.structureInput = structureInput;

    msg->structureInputDescriptor__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.structureInputDescriptor = (OSObjectRef) structureInputDescriptor;

    if (*scalarOutputCount > (sizeof(rpl->content.__scalarOutput) / sizeof(rpl->content.__scalarOutput[0]))) return kIOReturnOverrun;
    msg->content.scalarOutputCount = *scalarOutputCount;

    msg->content.structureOutputMaximumSize = structureOutputMaximumSize;

    msg->structureOutputDescriptor__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.structureOutputDescriptor = (OSObjectRef) structureOutputDescriptor;

    msg->completion__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.completion = (OSObjectRef) completion;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
        if (rpl->content.scalarOutputCount < *scalarOutputCount) *scalarOutputCount = rpl->content.scalarOutputCount;
        bcopy(&rpl->content.__scalarOutput[0], scalarOutput, *scalarOutputCount * sizeof(rpl->content.__scalarOutput[0]));
        *structureOutput = OSDynamicCast(OSData, (OSObject *) rpl->content.structureOutput);
        if (rpl->content.structureOutput && !*structureOutput) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
IOUserClient::CreateActionKernelCompletion(size_t referenceSize, OSAction ** action)
{
    kern_return_t ret;

#if defined(IOKIT_ENABLE_SHARED_PTR)
    OSSharedPtr<OSString>
#else /* defined(IOKIT_ENABLE_SHARED_PTR) */
    OSString *
#endif /* !defined(IOKIT_ENABLE_SHARED_PTR) */
    typeName = OSString::withCString("OSAction_IOUserClient_KernelCompletion");
    if (!typeName) {
        return kIOReturnNoMemory;
    }
    ret = OSAction_IOUserClient_KernelCompletion::CreateWithTypeName(this,
                           IOUserClient_KernelCompletion_ID,
                           IOUserClient_AsyncCompletion_ID,
                           referenceSize,
#if defined(IOKIT_ENABLE_SHARED_PTR)
                           typeName.get(),
#else /* defined(IOKIT_ENABLE_SHARED_PTR) */
                           typeName,
#endif /* !defined(IOKIT_ENABLE_SHARED_PTR) */
                           action);

#if !defined(IOKIT_ENABLE_SHARED_PTR)
    typeName->release();
#endif /* !defined(IOKIT_ENABLE_SHARED_PTR) */
    return (ret);
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
    uint32_t asyncDataCount = (sizeof(rpc.message->content.__asyncData) / sizeof(rpc.message->content.__asyncData[0]));

    if (IOUserClient_AsyncCompletion_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (targetActionClass) {
        action = (OSAction *) OSMetaClassBase::safeMetaCast((OSObject *) rpc.message->content.action, targetActionClass);
    } else {
        action = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.action);
    }
    if (!action && rpc.message->content.action) return (kIOReturnBadArgument);
    if (asyncDataCount > rpc.message->content.asyncDataCount) asyncDataCount = rpc.message->content.asyncDataCount;

    (*func)(target,
        action,
        rpc.message->content.status,
        &rpc.message->content.__asyncData[0],
        asyncDataCount);


    return (kIOReturnSuccess);
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
IOUserClient::CreateMemoryDescriptorFromClient_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        CreateMemoryDescriptorFromClient_Handler func)
{
    IOUserClient_CreateMemoryDescriptorFromClient_Invocation rpc = { _rpc };
    kern_return_t ret;
    uint32_t segmentsCount = (sizeof(rpc.message->content.__segments) / sizeof(rpc.message->content.__segments[0]));

    if (IOUserClient_CreateMemoryDescriptorFromClient_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (segmentsCount > rpc.message->content.segmentsCount) segmentsCount = rpc.message->content.segmentsCount;

    ret = (*func)(target,
        rpc.message->content.memoryDescriptorCreateOptions,
        segmentsCount,
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
IOUserClient::_ExternalMethod_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _ExternalMethod_Handler func)
{
    IOUserClient__ExternalMethod_Invocation rpc = { _rpc };
    kern_return_t ret;
    uint32_t scalarInputCount = (sizeof(rpc.message->content.__scalarInput) / sizeof(rpc.message->content.__scalarInput[0]));
    IOMemoryDescriptor * structureInputDescriptor;
    unsigned int scalarOutputCount = (sizeof(rpc.reply->content.__scalarOutput) / sizeof(rpc.reply->content.__scalarOutput[0]));
    IOMemoryDescriptor * structureOutputDescriptor;
    OSAction * completion;

    if (IOUserClient__ExternalMethod_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    structureInputDescriptor = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.structureInputDescriptor);
    if (!structureInputDescriptor && rpc.message->content.structureInputDescriptor) return (kIOReturnBadArgument);
    structureOutputDescriptor = OSDynamicCast(IOMemoryDescriptor, (OSObject *) rpc.message->content.structureOutputDescriptor);
    if (!structureOutputDescriptor && rpc.message->content.structureOutputDescriptor) return (kIOReturnBadArgument);
    completion = OSDynamicCast(OSAction, (OSObject *) rpc.message->content.completion);
    if (!completion && rpc.message->content.completion) return (kIOReturnBadArgument);
    if (scalarInputCount > rpc.message->content.scalarInputCount) scalarInputCount = rpc.message->content.scalarInputCount;
    if (scalarOutputCount > rpc.message->content.scalarOutputCount) scalarOutputCount = rpc.message->content.scalarOutputCount;

    ret = (*func)(target,
        rpc.message->content.selector,
        &rpc.message->content.__scalarInput[0],
        scalarInputCount,
        rpc.message->content.structureInput,
        structureInputDescriptor,
        &rpc.reply->content.__scalarOutput[0],
        &scalarOutputCount,
        rpc.message->content.structureOutputMaximumSize,
        &rpc.reply->content.structureOutput,
        structureOutputDescriptor,
        completion);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOUserClient__ExternalMethod_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 1;
    rpc.reply->content.__hdr.objectRefs = IOUserClient__ExternalMethod_Rpl_ObjRefs;
    rpc.reply->content.scalarOutputCount = scalarOutputCount;
    rpc.reply->structureOutput__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    rpc.reply->structureOutput__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    rpc.reply->structureOutput__descriptor.address = (void *) __builtin_offsetof(IOUserClient__ExternalMethod_Rpl_Content, structureOutput);
    rpc.reply->structureOutput__descriptor.size = 0;

    return (ret);
}

#if KERNEL
OSDefineMetaClassAndStructors(OSAction_IOUserClient_KernelCompletion, OSAction);
#endif /* KERNEL */

#if !KERNEL

#define OSAction_IOUserClient_KernelCompletion_QueueNames  ""

#define OSAction_IOUserClient_KernelCompletion_MethodNames  ""

#define OSAction_IOUserClient_KernelCompletionMetaClass_MethodNames  ""

struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(OSAction_IOUserClient_KernelCompletion_QueueNames)];
    char               methodNames[sizeof(OSAction_IOUserClient_KernelCompletion_MethodNames)];
    char               metaMethodNames[sizeof(OSAction_IOUserClient_KernelCompletionMetaClass_MethodNames)];
};

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
const struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t
OSClassDescription_OSAction_IOUserClient_KernelCompletion =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_OSAction_IOUserClient_KernelCompletion_t),
        .name                    = "OSAction_IOUserClient_KernelCompletion",
        .superName               = "OSAction",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t, metaMethodOptions),
        .queueNamesSize       = sizeof(OSAction_IOUserClient_KernelCompletion_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t, queueNames),
        .methodNamesSize         = sizeof(OSAction_IOUserClient_KernelCompletion_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t, methodNames),
        .metaMethodNamesSize     = sizeof(OSAction_IOUserClient_KernelCompletionMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_OSAction_IOUserClient_KernelCompletion_t, metaMethodNames),
        .flags                   = 0*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = OSAction_IOUserClient_KernelCompletion_QueueNames,
    .methodNames     = OSAction_IOUserClient_KernelCompletion_MethodNames,
    .metaMethodNames = OSAction_IOUserClient_KernelCompletionMetaClass_MethodNames,
};

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
OSMetaClass * gOSAction_IOUserClient_KernelCompletionMetaClass;

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
static kern_return_t
OSAction_IOUserClient_KernelCompletion_New(OSMetaClass * instance);

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
const OSClassLoadInformation
OSAction_IOUserClient_KernelCompletion_Class = 
{
    .description       = &OSClassDescription_OSAction_IOUserClient_KernelCompletion.base,
    .metaPointer       = &gOSAction_IOUserClient_KernelCompletionMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(OSAction_IOUserClient_KernelCompletion),

    .New               = &OSAction_IOUserClient_KernelCompletion_New,
};

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
extern const void * const
gOSAction_IOUserClient_KernelCompletion_Declaration;
 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
const void * const
gOSAction_IOUserClient_KernelCompletion_Declaration
 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &OSAction_IOUserClient_KernelCompletion_Class;

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
static kern_return_t
OSAction_IOUserClient_KernelCompletion_New(OSMetaClass * instance)
{
    if (!new(instance) OSAction_IOUserClient_KernelCompletionMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

 __attribute__((availability(driverkit,introduced=20,message="Type-safe OSAction factory methods are available in DriverKit 20 and newer")))
kern_return_t
OSAction_IOUserClient_KernelCompletionMetaClass::New(OSObject * instance)
{
    if (!new(instance) OSAction_IOUserClient_KernelCompletion) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
OSAction_IOUserClient_KernelCompletion::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
OSAction_IOUserClient_KernelCompletion::_Dispatch(OSAction_IOUserClient_KernelCompletion * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {

        default:
            ret = OSAction::_Dispatch(self, rpc);
            break;
    }

    return (ret);
}

#if KERNEL
kern_return_t
OSAction_IOUserClient_KernelCompletion::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
OSAction_IOUserClient_KernelCompletionMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

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



