/* iig(DriverKit-187 Aug  3 2021 18:40:13) generated from IOMemoryMap.iig */

#undef	IIG_IMPLEMENTATION
#define	IIG_IMPLEMENTATION 	IOMemoryMap.iig

#if KERNEL
#include <libkern/c++/OSString.h>
#else
#include <DriverKit/DriverKit.h>
#endif /* KERNEL */
#include <DriverKit/IOReturn.h>
#include <DriverKit/IOMemoryMap.h>


#if __has_builtin(__builtin_load_member_function_pointer)
#define SimpleMemberFunctionCast(cfnty, self, func) (cfnty)__builtin_load_member_function_pointer(self, func)
#else
#define SimpleMemberFunctionCast(cfnty, self, func) ({ union { typeof(func) memfun; cfnty cfun; } pair; pair.memfun = func; pair.cfun; })
#endif


struct IOMemoryMap__CopyState_Msg_Content
{
    IORPCMessage __hdr;
    OSObjectRef  __object;
};
#pragma pack(4)
struct IOMemoryMap__CopyState_Msg
{
    IORPCMessageMach           mach;
    mach_msg_port_descriptor_t __object__descriptor;
    IOMemoryMap__CopyState_Msg_Content content;
};
#pragma pack()
#define IOMemoryMap__CopyState_Msg_ObjRefs (1)

struct IOMemoryMap__CopyState_Rpl_Content
{
    IORPCMessage __hdr;
    _IOMemoryMapPrivateState  state;
};
#pragma pack(4)
struct IOMemoryMap__CopyState_Rpl
{
    IORPCMessageMach           mach;
    IOMemoryMap__CopyState_Rpl_Content content;
};
#pragma pack()
#define IOMemoryMap__CopyState_Rpl_ObjRefs (0)


typedef union
{
    const IORPC rpc;
    struct
    {
        const struct IOMemoryMap__CopyState_Msg * message;
        struct IOMemoryMap__CopyState_Rpl       * reply;
        uint32_t sendSize;
        uint32_t replySize;
    };
}
IOMemoryMap__CopyState_Invocation;
#if !KERNEL
extern OSMetaClass * gOSContainerMetaClass;
extern OSMetaClass * gOSDataMetaClass;
extern OSMetaClass * gOSNumberMetaClass;
extern OSMetaClass * gOSStringMetaClass;
extern OSMetaClass * gOSBooleanMetaClass;
extern OSMetaClass * gOSDictionaryMetaClass;
extern OSMetaClass * gOSArrayMetaClass;
extern OSMetaClass * gIODispatchQueueMetaClass;
#endif /* !KERNEL */

#if !KERNEL

#define IOMemoryMap_QueueNames  ""

#define IOMemoryMap_MethodNames  ""

#define IOMemoryMapMetaClass_MethodNames  ""

struct OSClassDescription_IOMemoryMap_t
{
    OSClassDescription base;
    uint64_t           methodOptions[2 * 0];
    uint64_t           metaMethodOptions[2 * 0];
    char               queueNames[sizeof(IOMemoryMap_QueueNames)];
    char               methodNames[sizeof(IOMemoryMap_MethodNames)];
    char               metaMethodNames[sizeof(IOMemoryMapMetaClass_MethodNames)];
};

const struct OSClassDescription_IOMemoryMap_t
OSClassDescription_IOMemoryMap =
{
    .base =
    {
        .descriptionSize         = sizeof(OSClassDescription_IOMemoryMap_t),
        .name                    = "IOMemoryMap",
        .superName               = "OSObject",
        .methodOptionsSize       = 2 * sizeof(uint64_t) * 0,
        .methodOptionsOffset     = __builtin_offsetof(struct OSClassDescription_IOMemoryMap_t, methodOptions),
        .metaMethodOptionsSize   = 2 * sizeof(uint64_t) * 0,
        .metaMethodOptionsOffset = __builtin_offsetof(struct OSClassDescription_IOMemoryMap_t, metaMethodOptions),
        .queueNamesSize       = sizeof(IOMemoryMap_QueueNames),
        .queueNamesOffset     = __builtin_offsetof(struct OSClassDescription_IOMemoryMap_t, queueNames),
        .methodNamesSize         = sizeof(IOMemoryMap_MethodNames),
        .methodNamesOffset       = __builtin_offsetof(struct OSClassDescription_IOMemoryMap_t, methodNames),
        .metaMethodNamesSize     = sizeof(IOMemoryMapMetaClass_MethodNames),
        .metaMethodNamesOffset   = __builtin_offsetof(struct OSClassDescription_IOMemoryMap_t, metaMethodNames),
        .flags                   = 1*kOSClassCanRemote,
    },
    .methodOptions =
    {
    },
    .metaMethodOptions =
    {
    },
    .queueNames      = IOMemoryMap_QueueNames,
    .methodNames     = IOMemoryMap_MethodNames,
    .metaMethodNames = IOMemoryMapMetaClass_MethodNames,
};

OSMetaClass * gIOMemoryMapMetaClass;

static kern_return_t
IOMemoryMap_New(OSMetaClass * instance);

const OSClassLoadInformation
IOMemoryMap_Class = 
{
    .description       = &OSClassDescription_IOMemoryMap.base,
    .metaPointer       = &gIOMemoryMapMetaClass,
    .version           = 1,
    .instanceSize      = sizeof(IOMemoryMap),

    .New               = &IOMemoryMap_New,
};

extern const void * const
gIOMemoryMap_Declaration;
const void * const
gIOMemoryMap_Declaration
__attribute__((visibility("hidden"),section("__DATA_CONST,__osclassinfo,regular,no_dead_strip")))
    = &IOMemoryMap_Class;

static kern_return_t
IOMemoryMap_New(OSMetaClass * instance)
{
    if (!new(instance) IOMemoryMapMetaClass) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

kern_return_t
IOMemoryMapMetaClass::New(OSObject * instance)
{
    if (!new(instance) IOMemoryMap) return (kIOReturnNoMemory);
    return (kIOReturnSuccess);
}

#endif /* !KERNEL */

kern_return_t
IOMemoryMap::Dispatch(const IORPC rpc)
{
    return _Dispatch(this, rpc);
}

kern_return_t
IOMemoryMap::_Dispatch(IOMemoryMap * self, const IORPC rpc)
{
    kern_return_t ret = kIOReturnUnsupported;
    IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);

    switch (msg->msgid)
    {
#if KERNEL
        case IOMemoryMap__CopyState_ID:
        {
            ret = IOMemoryMap::_CopyState_Invoke(rpc, self, SimpleMemberFunctionCast(IOMemoryMap::_CopyState_Handler, *self, &IOMemoryMap::_CopyState_Impl));
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
IOMemoryMap::MetaClass::Dispatch(const IORPC rpc)
{
#else /* KERNEL */
kern_return_t
IOMemoryMapMetaClass::Dispatch(const IORPC rpc)
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

kern_return_t
IOMemoryMap::_CopyState(
        _IOMemoryMapPrivateState * state,
        OSDispatchMethod supermethod)
{
    kern_return_t ret;
    union
    {
        IOMemoryMap__CopyState_Msg msg;
        struct
        {
            IOMemoryMap__CopyState_Rpl rpl;
            mach_msg_max_trailer_t trailer;
        } rpl;
    } buf;
    struct IOMemoryMap__CopyState_Msg * msg = &buf.msg;
    struct IOMemoryMap__CopyState_Rpl * rpl = &buf.rpl.rpl;

    memset(msg, 0, sizeof(struct IOMemoryMap__CopyState_Msg));
    msg->mach.msgh.msgh_id   = kIORPCVersion190615;
    msg->mach.msgh.msgh_size = sizeof(*msg);
    msg->content.__hdr.flags = 0*kIORPCMessageOneway
                             | 1*kIORPCMessageSimpleReply
                             | 0*kIORPCMessageLocalHost
                             | 0*kIORPCMessageOnqueue;
    msg->content.__hdr.msgid = IOMemoryMap__CopyState_ID;
    msg->content.__object = (OSObjectRef) this;
    msg->content.__hdr.objectRefs = IOMemoryMap__CopyState_Msg_ObjRefs;
    msg->mach.msgh_body.msgh_descriptor_count = 1;

    msg->__object__descriptor.type = MACH_MSG_PORT_DESCRIPTOR;

    IORPC rpc = { .message = &buf.msg.mach, .reply = &buf.rpl.rpl.mach, .sendSize = sizeof(buf.msg), .replySize = sizeof(buf.rpl) };
    if (supermethod) ret = supermethod((OSObject *)this, rpc);
    else             ret = ((OSObject *)this)->Invoke(rpc);

    if (kIOReturnSuccess == ret)
    do {
        {
            if (rpl->mach.msgh.msgh_size                  != sizeof(*rpl)) { ret = kIOReturnIPCError; break; };
            if (rpl->content.__hdr.msgid                  != IOMemoryMap__CopyState_ID) { ret = kIOReturnIPCError; break; };
            if (rpl->mach.msgh_body.msgh_descriptor_count != 0) { ret = kIOReturnIPCError; break; };
            if (IOMemoryMap__CopyState_Rpl_ObjRefs   != rpl->content.__hdr.objectRefs) { ret = kIOReturnIPCError; break; };
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
IOMemoryMap::_CopyState_Invoke(const IORPC _rpc,
        OSMetaClassBase * target,
        _CopyState_Handler func)
{
    IOMemoryMap__CopyState_Invocation rpc = { _rpc };
    kern_return_t ret;

    if (IOMemoryMap__CopyState_Msg_ObjRefs != rpc.message->content.__hdr.objectRefs) return (kIOReturnIPCError);

    ret = (*func)(target,
        &rpc.reply->content.state);

    if (kIOReturnSuccess != ret) return (ret);

    rpc.reply->content.__hdr.msgid = IOMemoryMap__CopyState_ID;
    rpc.reply->content.__hdr.flags = kIORPCMessageOneway;
    rpc.reply->mach.msgh.msgh_id   = kIORPCVersion190615Reply;
    rpc.reply->mach.msgh.msgh_size = sizeof(*rpc.reply);
    rpc.reply->mach.msgh_body.msgh_descriptor_count = 0;
    rpc.reply->content.__hdr.objectRefs = IOMemoryMap__CopyState_Rpl_ObjRefs;

    return (ret);
}



