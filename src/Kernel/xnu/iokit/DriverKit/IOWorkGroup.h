/* iig-lite generated from IOWorkGroup.iig - kernel-side subset; msgids are NOT Apple-ABI */

/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_UIOWORKGROUP_H
#define _IOKIT_UIOWORKGROUP_H

#include <DriverKit/OSObject.h>  /* .iig include */
#include <DriverKit/IOUserClient.h>  /* .iig include */

enum {
	kIOWorkGroupMaxNameLength = 64,
};

/*!
 * @class IOWorkGroup
 *
 * @abstract
 * Workgroups allow multiple threads to coordinate activities for realtime operations.
 *
 * @discussion
 *
 * Applications that open user clients to a DriverKit driver can send a workgroup to use in the driver.
 *
 * The application will have to first create an workgroup object. The application then should copy the
 * workgroup port with os_workgroup_copy_port(). To send the workgroup port to the driver, use:
 *
 * const char * name = "Work Group Name"; // This must match the name the driver used in IOWorkGroup::Create().
 * kern_return_t ret = IOConnectTrap3(connect, // user client connection (io_connect_t)
 *                                    1, // specifies event link configuration trap
 *                                    (uintptr_t)name,
 *                                    (uintptr_t)strlen(name),
 *                                    (uintptr_t)wgPort // port from os_workgroup_copy_port
 *                                    );
 *
 * Once the workgroup port has been sent to the driver, the driver should be notified with a user-defined external method
 * or other existing signaling mechanism.
 */
/* source class IOWorkGroup IOWorkGroup.iig:63-142 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

class NATIVE KERNEL IOWorkGroup : public OSObject
{
public:

	virtual bool
	init() override;

	virtual void
	free() override;

    /*!
     * @brief       Create an IOWorkGroup object. This object is not functional until a workgroup port has been set.
     * @param       name        Name of the workgroup
     * @param       userClient  Userclient to create the workgroup in. The DriverKit runtime will retain the userclient, and will
     *                          release it in Invalidate() or when the IOWorkGroup is freed.
     * @param       workgroup   Created IOWorkGroup with +1 retain count to be released by the caller.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	static kern_return_t
	Create(OSString * name, IOUserClient * userClient, IOWorkGroup ** workgroup) LOCAL;

#if DRIVERKIT_PRIVATE

    /*!
     * @brief       Set the port for this workgroup. This should not be called directly.
     * @param       port Workgroup port
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
    virtual kern_return_t
    SetWorkGroupPort(mach_port_t port PORTCOPYSEND) LOCAL;

#endif /* DRIVERKIT_PRIVATE */

    /*!
     * @brief       Get the size of the workgroup token.
     * @discussion  Join() and Leave() require the caller to pass a token. This token should be allocated by the caller, and freed when
     *              no longer needed. Use this method to determine how much memory to allocate for the token.
     * @return      Workgroup token size
     */
    size_t
    GetTokenSize() LOCALONLY;

    /*!
     * @brief       Join the workgroup. 
     * @discussion  Before calling this method, the caller must allocate a token. This token must be passed to this method. When leaving
     *              a workgroup with Leave(), use the same token that was passed to Join().
     * @param       token  The workgroup token.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
    kern_return_t
    Join(void * token) LOCALONLY;

    /*!
     * @brief       Leave the workgroup. 
     * @discussion  The workgroup must have been joined with Join(). Use the same token in Join() for this method.
     * @param       token  The workgroup token.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
    kern_return_t
    Leave(void * token) LOCALONLY;

    /*!
     * @brief       Invalidate the IOWorkGroup.
     * @discussion  This releases the kernel reference to the IOWorkGroup, allowing the name to be used for a different
     *              IOWorkGroup. This method should be called after the client has configured the eventlink with the IOConnectTrap
     *              call. After invalidation, the IOWorkGroup can no longer be configured through the IOConnectTrap call. No other
     *              functionality is affected.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
    kern_return_t
    Invalidate() LOCALONLY;

#if DRIVERKIT_PRIVATE

    virtual kern_return_t
    InvalidateKernel(IOUserClient * client);

#endif /* DRIVERKIT_PRIVATE */

};

#undef KERNEL
#else /* __DOCUMENTATION__ */

/* generated class IOWorkGroup IOWorkGroup.iig:63-142 */

#define IOWorkGroup_Create_ID            0xb8b9d488a13e2251ULL
#define IOWorkGroup_SetWorkGroupPort_ID            0xdd9fcee0c0f89189ULL
#define IOWorkGroup_InvalidateKernel_ID            0x1c0b96919171962cULL

#define IOWorkGroup_Create_Args \
        OSString * name, \
        IOUserClient * userClient, \
        IOWorkGroup ** workgroup

#define IOWorkGroup_SetWorkGroupPort_Args \
        mach_port_t port

#define IOWorkGroup_InvalidateKernel_Args \
        IOUserClient * client

#define IOWorkGroup_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(IOWorkGroup * self, const IORPC rpc);\
\
    static kern_return_t\
    Create(\
        OSString * name,\
        IOUserClient * userClient,\
        IOWorkGroup ** workgroup);\
\
    kern_return_t\
    SetWorkGroupPort(\
        mach_port_t port,\
        OSDispatchMethod supermethod = NULL);\
\
    size_t\
    GetTokenSize(\
);\
\
    kern_return_t\
    Join(\
        void * token);\
\
    kern_return_t\
    Leave(\
        void * token);\
\
    kern_return_t\
    Invalidate(\
);\
\
    kern_return_t\
    InvalidateKernel(\
        IOUserClient * client,\
        OSDispatchMethod supermethod = NULL);\
\
\
protected:\
    /* _Impl methods */\
\
    static kern_return_t\
    Create_Impl(IOWorkGroup_Create_Args);\
\
    kern_return_t\
    SetWorkGroupPort_Impl(IOWorkGroup_SetWorkGroupPort_Args);\
\
\
public:\
    /* _Invoke methods */\
\
    typedef kern_return_t (*Create_Handler)(IOWorkGroup_Create_Args);\
    static kern_return_t\
    Create_Invoke(const IORPC rpc,\
        Create_Handler func);\
\
    typedef kern_return_t (*SetWorkGroupPort_Handler)(OSMetaClassBase * target, IOWorkGroup_SetWorkGroupPort_Args);\
    static kern_return_t\
    SetWorkGroupPort_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetWorkGroupPort_Handler func);\
\
    typedef kern_return_t (*InvalidateKernel_Handler)(OSMetaClassBase * target, IOWorkGroup_InvalidateKernel_Args);\
    static kern_return_t\
    InvalidateKernel_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        InvalidateKernel_Handler func);\
\


#define IOWorkGroup_KernelMethods \
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    InvalidateKernel_Impl(IOWorkGroup_InvalidateKernel_Args);\
\


#define IOWorkGroup_VirtualMethods \
\
public:\
\
    virtual bool\
    init(\
) APPLE_KEXT_OVERRIDE;\
\
    virtual void\
    free(\
) APPLE_KEXT_OVERRIDE;\
\


#if !KERNEL

extern OSMetaClass          * gIOWorkGroupMetaClass;
extern const OSClassLoadInformation IOWorkGroup_Class;

class IOWorkGroupMetaClass : public OSMetaClass
{
public:
    virtual kern_return_t
    New(OSObject * instance) override;
    virtual kern_return_t
    Dispatch(const IORPC rpc) override;
};

#endif /* !KERNEL */

class IOWorkGroupInterface : public OSInterface
{
public:
};

struct IOWorkGroup_IVars;
struct IOWorkGroup_LocalIVars;

class IOWorkGroup : public OSObject, public IOWorkGroupInterface
{
#if KERNEL
    OSDeclareDefaultStructorsWithDispatch(IOWorkGroup);
#endif /* KERNEL */

#if !KERNEL
    friend class IOWorkGroupMetaClass;
#endif /* !KERNEL */

public:
#ifdef IOWorkGroup_DECLARE_IVARS
IOWorkGroup_DECLARE_IVARS
#else /* IOWorkGroup_DECLARE_IVARS */
    union
    {
        IOWorkGroup_IVars * ivars;
        IOWorkGroup_LocalIVars * lvars;
    };
#endif /* IOWorkGroup_DECLARE_IVARS */
#if !KERNEL
    static OSMetaClass *
    sGetMetaClass() { return gIOWorkGroupMetaClass; };
    virtual const OSMetaClass *
    getMetaClass() const APPLE_KEXT_OVERRIDE { return gIOWorkGroupMetaClass; };
#endif /* KERNEL */

    using super = OSObject;

#if !KERNEL
    IOWorkGroup_Methods
#endif /* !KERNEL */

    IOWorkGroup_VirtualMethods
};


#endif /* !__DOCUMENTATION__ */



#endif /* ! _IOKIT_UIOWORKGROUP_H */
