/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IODispatchQueue.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IODispatchQueue.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IODispatchQueue.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


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
    const char *  name;
#if !defined(__LP64__)
    uint32_t __namePad;
#endif /* !defined(__LP64__) */
    char __name[256];
    uint64_t  options;
    uint64_t  priority;
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
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gOSStringMetaClass;
#endif /* !KERNEL */

#if KERNEL
OSDefineMetaClassAndStructors(IODispatchQueue, OSObject);
#endif /* KERNEL */

#if !KERNEL

#define IODispatchQueue_QueueNames  ""

#define IODispatchQueue_MethodNames  ""

#define IODispatchQueueMetaClass_MethodNames  ""

struct OSClassDescription_IODispatchQueue_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IODispatchQueue_QueueNames)];
    char               methodNames[sizeof(IODispatchQueue_MethodNames)];
    char               metaMethodNames[sizeof(IODispatchQueueMetaClass_MethodNames)];
};

const struct OSClassDescription_IODispatchQueue_t
OSClassDescription_IODispatchQueue =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IODispatchQueue_t),
        .name                    = "IODispatchQueue",
        .superName               = "OSObject",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IODispatchQueue_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IODispatchQueue_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IODispatchQueue_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IODispatchQueue_t, queueNames),
        .methodNamesSize         = sizeof(IODispatchQueue_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IODispatchQueue_t, methodNames),
        .metaMethodNamesSize     = sizeof(IODispatchQueueMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IODispatchQueue_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IODispatchQueue_QueueNames,
    .methodNames     = IODispatchQueue_MethodNames,
    .metaMethodNames = IODispatchQueueMetaClass_MethodNames,
};

OSMetaClass * gIODispatchQueueMetaClass;

static kern_return_t
IODispatchQueue_New(OSMetaClass * instance);

const OSClassLoadInformation
IODispatchQueue_Class = 
{
    .description       = &OSClassDescription_IODispatchQueue.base,
    .metaPointer       = &gIODispatchQueueMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IODispatchQueue),

    .New               = &IODispatchQueue_New,
};

extern const void * const
gIODispatchQueue_Declaration;
const void * const
gIODispatchQueue_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IODispatchQueue_Class;

static kern_return_t
IODispatchQueue_New(OSMetaClass * instance)
{
    if (!new(instance) IODispatchQueueMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IODispatchQueueMetaClass::New(OSObject * instance)
{
    if (!new(instance) IODispatchQueue) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

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
#else /* KERNEL */
kern_return_t
IODispatchQueueMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IODispatchQueue_Create_ID:
            ret = IODispatchQueue::Create_Invoke(rpc, &IODispatchQueue::Create_Impl);
            break;
#endif /* !KERNEL */

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

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
    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

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
IODispatchQueue::Create_Call(
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

    msg->content.name = NULL;

    strlcpy(&msg->content.__name[0], name, sizeof(msg->content.__name));

    msg->content.options = options;

    msg->content.priority = priority;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(IODispatchQueue)->Invoke(rpc);

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
IODispatchQueue::Create_Invoke(const IORPC _rpc,
        Create_Handler func)
{
    IODispatchQueue_Create_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IODispatchQueue_Create_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    if (strnlen(&rpc.message->content.__name[0], sizeof(rpc.message->content.__name)) >= sizeof(rpc.message->content.__name)) return kIOReturnBadArgument;

    ret = (*func)(        &rpc.message->content.__name[0],
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



