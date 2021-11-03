/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from OSAction.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	OSAction.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/OSAction.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


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
    OSString * typeName;
#if !defined(__LP64__)
    uint32_t __typeNamePad;
#endif /* !defined(__LP64__) */
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
    mach_msg_ool_descriptor_t  typeName__descriptor;
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
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
extern OSMetaClass * gOSStringMetaClass;
#endif /* !KERNEL */

#if KERNEL
OSDefineMetaClassAndStructors(OSAction, OSObject);
#endif /* KERNEL */

#if !KERNEL

#define OSAction_QueueNames  ""

#define OSAction_MethodNames  ""

#define OSActionMetaClass_MethodNames  ""

struct OSClassDescription_OSAction_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(OSAction_QueueNames)];
    char               methodNames[sizeof(OSAction_MethodNames)];
    char               metaMethodNames[sizeof(OSActionMetaClass_MethodNames)];
};

const struct OSClassDescription_OSAction_t
OSClassDescription_OSAction =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_OSAction_t),
        .name                    = "OSAction",
        .superName               = "OSObject",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_OSAction_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_OSAction_t, metaMethodOptions),
        .queueNamesSize       = sizeof(OSAction_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_OSAction_t, queueNames),
        .methodNamesSize         = sizeof(OSAction_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_OSAction_t, methodNames),
        .metaMethodNamesSize     = sizeof(OSActionMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_OSAction_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = OSAction_QueueNames,
    .methodNames     = OSAction_MethodNames,
    .metaMethodNames = OSActionMetaClass_MethodNames,
};

OSMetaClass * gOSActionMetaClass;

static kern_return_t
OSAction_New(OSMetaClass * instance);

const OSClassLoadInformation
OSAction_Class = 
{
    .description       = &OSClassDescription_OSAction.base,
    .metaPointer       = &gOSActionMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(OSAction),

    .New               = &OSAction_New,
};

extern const void * const
gOSAction_Declaration;
const void * const
gOSAction_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &OSAction_Class;

static kern_return_t
OSAction_New(OSMetaClass * instance)
{
    if (!new(instance) OSActionMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
OSActionMetaClass::New(OSObject * instance)
{
    if (!new(instance) OSAction) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

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
        case OSAction_Aborted_ID:
        {
            ret = OSAction::Aborted_Invoke(rpc, self, SimpleMemberFunctionCast(OSAction::Aborted_Handler, *self, &OSAction::Aborted_Impl));
            break;
        }

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
#else /* KERNEL */
kern_return_t
OSActionMetaClass::Dispatch(const IORPC rpc)
{
#endif /* !KERNEL */

    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case OSAction_Create_ID:
            ret = OSAction::Create_Invoke(rpc, &OSAction::Create_Impl);
            break;
#endif /* !KERNEL */
#if KERNEL
        case OSAction_CreateWithTypeName_ID:
            ret = OSAction::CreateWithTypeName_Invoke(rpc, &OSAction::CreateWithTypeName_Impl);
            break;
#endif /* !KERNEL */

        default:
            ret = OSMetaClassBase::Dispatch(rpc);
            break;
    }

    return (ret);
}

kern_return_t
OSAction::Create_Call(
        OSObject * target,
        uint64_t targetmsgid,
        uint64_t msgid,
        size_t referenceSize,
        OSAction ** action)
{
    kern_return_t ret;
    union
    {
        OSAction_Create_Msg msg;
        struct
        {
            OSAction_Create_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct OSAction_Create_Msg * msg = &buf.msg;
    struct OSAction_Create_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct OSAction_Create_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = OSAction_Create_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(OSAction);
    msg->content.__hdr.objectRefs = OSAction_Create_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 2;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->target__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.target = (OSObjectRef) target;

    msg->content.targetmsgid = targetmsgid;

    msg->content.msgid = msgid;

    msg->content.referenceSize = referenceSize;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(OSAction)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != OSAction_Create_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (OSAction_Create_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *action = OSDynamicCast(OSAction, (OSObject *) rpl->content.action);
        if (rpl->content.action && !*action) ret = kIOReturnBadArgument;
    }


    return (ret);
}

kern_return_t
OSAction::CreateWithTypeName_Call(
        OSObject * target,
        uint64_t targetmsgid,
        uint64_t msgid,
        size_t referenceSize,
        OSString * typeName,
        OSAction ** action)
{
    kern_return_t ret;
    union
    {
        OSAction_CreateWithTypeName_Msg msg;
        struct
        {
            OSAction_CreateWithTypeName_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct OSAction_CreateWithTypeName_Msg * msg = &buf.msg;
    struct OSAction_CreateWithTypeName_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct OSAction_CreateWithTypeName_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 0*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = OSAction_CreateWithTypeName_ID;
    msg->content.__object = (OSObjectRef) OSTypeID(OSAction);
    msg->content.__hdr.objectRefs = OSAction_CreateWithTypeName_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 3;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    msg->target__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    msg->content.target = (OSObjectRef) target;

    msg->content.targetmsgid = targetmsgid;

    msg->content.msgid = msgid;

    msg->content.referenceSize = referenceSize;

    msg->typeName__descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    msg->typeName__descriptor.copy = MACH_MSG_VIRTUAL_COPY;
    msg->typeName__descriptor.address = (void *) __builtin_offsetof(OSAction_CreateWithTypeName_Msg_Content, typeName);
    msg->content.typeName = typeName;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    ret = OSMTypeID(OSAction)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != OSAction_CreateWithTypeName_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 1) { ret = kIOReturnIPCError; break; };
            if (OSAction_CreateWithTypeName_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
        }
    }
    while (false);
    if (kIOReturnSuccess == ret)
    {
        *action = OSDynamicCast(OSAction, (OSObject *) rpl->content.action);
        if (rpl->content.action && !*action) ret = kIOReturnBadArgument;
    }


    return (ret);
}

void
OSAction::Aborted(        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        OSAction_Aborted_Msg msg;
    } buf;
    struct OSAction_Aborted_Msg * msg = &buf.msg;

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

    IORPC rpc = { .message = &buf.msg.mach, .reply = NULL, .sendSize = sizeof(*msg), .replySize = 0 };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

}

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

    ret = (*func)(        target,
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

    if (OSAction_CreateWithTypeName_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);
    target = OSDynamicCast(OSObject, (OSObject *) rpc.message->content.target);
    if (!target && rpc.message->content.target) return (kIOReturnBadArgument);

    ret = (*func)(        target,
        rpc.message->content.targetmsgid,
        rpc.message->content.msgid,
        rpc.message->content.referenceSize,
        rpc.message->content.typeName,
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



