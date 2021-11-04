/* iig(DriverKit-187) generated from IOService.iig */

/* IOService.iig:1-60 */
/*
 * Copyright (c) 2019-2019 Apple Inc. All rights reserved.
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

#if !__IIG
#if KERNEL
#include <IOKit/IOService.h>
#endif
#endif

#ifndef _IOKIT_UIOSERVICE_H
#define _IOKIT_UIOSERVICE_H

#include <DriverKit/OSObject.h>  /* .iig include */

class IOMemoryDescriptor;
class IOBufferMemoryDescriptor;
class IOUserClient;
class OSAction;

typedef char IOServiceName[128];
typedef char IOPropertyName[128];
typedef char IORegistryPlaneName[128];

enum {
	kIOServiceSearchPropertyParents = 0x00000001,
};

#define kIOServiceDefaultQueueName	"Default"

enum {
	kIOServicePowerCapabilityOff = 0x00000000,
	kIOServicePowerCapabilityOn  = 0x00000002,
	kIOServicePowerCapabilityLow = 0x00010000,
};

/* source class IOService IOService.iig:61-360 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

/*!
 * @class IOService
 *
 * @abstract
 * IOService represents an device or OS service in IOKit and DriverKit.
 *
 * @discussion
 * IOKit provides driver lifecycle management through the IOService APIs. 
 * Drivers and devices are represented as subclasses of IOService.
 *

*/

class KERNEL IOService : public OSObject
{
public:
	virtual bool
	init() override;

	virtual void
	free() override;

    /*!
     * @brief       First call made to a matched IOService.
     * @discussion  During matching IOKit will create an IOService object for successful matches.
     *              Start is the first call made to the new object.
     * @param       provider The IOService provider for the match. This should be OSRequiredCast to the expected class.
     *              The provider is retained by DriverKit for the duration of Start() and on successful Start() until
     *              IOService::Stop() is called.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	Start(IOService * provider) LOCAL;

    /*!
     * @brief       Terminate access to provider.
     * @discussion  During termination IOKit will teardown any IOService objects attached to a terminated provider.
     *              Stop should quiesce all activity and when complete, pass the call to super. After calling super, the
     *              provider is no longer valid and this object will likely be freed.
     * @param       provider The IOService provider for being terminated, one previously passed to Start
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	Stop(IOService * provider) LOCAL;

	/*! @function   ClientCrashed
	 * @discussion  Notification for kernel objects of a client crash.
     * @param       client Attached client.
     * @param       options No options are currently defined.
	 * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	ClientCrashed(IOService * client, uint64_t options);

    /*!
     * @brief       Obtain IOKit IORegistryEntryID.
     * @param       registryEntryID IORegistryEntryID for the IOKit object.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	GetRegistryEntryID(uint64_t * registryEntryID) LOCAL;

    /*!
     * @brief       Set the IORegistryEntry name.
     * @param       name Name for the IOKit object. The c-string will be copied.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SetName(
	const IOServiceName name);

    /*!
     * @brief       Start the matching process on the IOService object.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	RegisterService();

    /*!
     * @brief       Set the IODispatchQueue for a given name on the IOService.
     * @param       name Name for the queue. The name may be referenced by methods in the .iig class definition
     *              with the QUEUENAME() attribute to indicate the method must be invoked on that queue. If a method
     *              is invoked before the queue is set for the name, the default queue is used. A default queue is
     *              created by DriverKit for every new IOService object with the name kIOServiceDefaultQueueName.
     * @param       queue Queue to be associated with the name on this IOService.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SetDispatchQueue(
		const IODispatchQueueName name,
		IODispatchQueue         * queue) override LOCAL;

    /*!
     * @brief       Obtain the IODispatchQueue for a given name on the IOService.
     * @param       name Name for the queue.
     * @param       queue Returned, retained queue or NULL. The caller should release this queue.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	CopyDispatchQueue(
		const IODispatchQueueName name,
		IODispatchQueue        ** queue) override;

    /*!
     * @brief       Obtain the IOKit registry properties for the IOService.
     * @param       properties Returned, retained dictionary of properties or NULL. The caller should release this dictionary.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	CopyProperties(
		OSDictionary ** properties);

    /*!
     * @brief       Obtain the an IOKit registry properties from the service or one of its parents.
     * @param       name Name of the property as a c-string.
     * @param       plane Name of the registry plane to be searched, if the option kIOServiceSearchPropertyParents
     *              is used.
     * @param       options Pass kIOServiceSearchPropertyParents to search for the property in the IOService and all
     *              its parents in the IOKit registry.
     * @param       property Returned, retained property object or NULL. The caller should release this property.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SearchProperty(
		const IOPropertyName name,
		const IORegistryPlaneName plane,
		uint64_t options,
		OSContainer ** property);

    /*!
     * @brief       Send a dictionary of properties to an IOService.
     * @discussion  By default the method will fail. A DriverKit subclass or kernel class may implement this method.
     * @param       properties Dictionary of properties.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SetProperties(
		OSDictionary * properties);

    /*!
     * @brief       Notification of change in power state of a provider.
     * @discussion  DriverKit notifies of changes in power of a provider. The driver should make itself safe for
     *              the new state before passing the call to super. 
     * @param       powerFlags The power capabilities of the new state. The values possible are:
	 *	kIOServicePowerCapabilityOff the system will be entering sleep state
	 *	kIOServicePowerCapabilityOn  the device and system are fully powered
	 *  kIOServicePowerCapabilityLow the device is in a reduced power state while the system is running
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SetPowerState(
		uint32_t powerFlags) LOCAL;

    /*!
     * @brief       Allow provider to enter a low power state.
     * @discussion  A driver may allow a device to enter a lower power state. 
     * @param       powerFlags The power capabilities of the new state. The values possible are:
	 *  kIOServicePowerCapabilityLow the device is in a reduced power state while the system is running
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	ChangePowerState(
		uint32_t powerFlags);

    /*!
     * @brief       Request create a new user client for a client process.
     * @discussion  An application may request an IOUserClient be opened with the IOKit framework
     *              IOServiceOpen() call. The type parameter of that call is passed here. The driver should respond to
     *              the call by calling IOService::Create() with a plist entry describing the new user client object.
     * @param       type The type passed to IOServiceOpen().
     * @param       userClient The object created by IOService::Create()
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	NewUserClient(
		uint32_t type,
		IOUserClient ** userClient);

    /*!
     * @brief       Request to create an IOService object from a plist property.
     * @discussion  An IOService interface or IOUserClient subclass may be created from a plist property of the driver.
     *              The plist should contain the following IOKit matching keys:
     *              IOClass - kernel class of IOUserUserClient
     *              IOUserClass - DriverKit class to be instantiated
     *              IOServiceDEXTEntitlements - Array of entitlements to be checked against a user client owning task
     * @param       provider The provider of the new object.
     * @param       propertiesKey The name of the properties dictionary in this IOService
     * @param       result The created object retained, to be released by the caller.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	Create(
		IOService          * provider,
		const IOPropertyName propertiesKey,
		IOService         ** result) LOCAL;

    /*!
     * @brief       Start an IOService termination.
     * @discussion  An IOService object created with Create() may be removed by calling Terminate().
     *              The termination is asynchronous and will later call Stop() on the service.
     * @param       options No options are currently defined, pass zero.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	Terminate(
		uint64_t			 options);

   /*!
    * @brief       Obtain supportable properties describing the provider chain.
    * @discussion  Obtain supportable properties describing the provider chain. This will be a subset of registry
    *              properties the OS considers supportable.
    *              The array is ordered with a dictionary of properties for each entry in the provider chain from this
    *              service towards the root.
    * @param       propertyKeys If only certain property values are need, they may be passed in this array.
    * @param       properties Returned, retained array of dictionaries of properties or NULL. The caller should release
    *              this array.
    * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
    */
	virtual kern_return_t
	CopyProviderProperties(
		OSArray  * propertyKeys,
		OSArray ** properties);

   /*!
    * @brief       Reduce power saving modes in the system in order to provide decreased latency
	*			   to hardware DMA requests.
    * @discussion  When the system enters lower power states DMA access to memory may be affected.
	*			   The best way by far to handle this is to change how you schedule your time-critical DMA operations in
	*			   your driver such that an occasional delay will not affect the proper functioning of your device.
	*			   However, if this is not possible, your driver can inform power management when a time-critical transfer
	*			   begins and ends so that the system will not enter the lowest power states during that time. To do this,
	*			   pass a value to requireMaxBusStall that informs power management of the maximum memory access latency in
	*			   nanoseconds that can be tolerated by the driver. This value is hardware dependent and is related to the
	*			   amount of buffering available in the hardware.
	*			   Supported values are given by the kIOMaxBusStall* enum in IOTypes.h
	*			   Pass the largest value possible that works for your device. This will minimize power
	*			   consumption and maximize battery life by still allowing some level of CPU power management.
    * @param       maxBusStall A value from the kIOMaxBusStall* enum in IOTypes.h
    * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
    */
	virtual kern_return_t
	RequireMaxBusStall(
		uint64_t maxBusStall);

	/*! @function IOCreatePropertyMatchingDictionary
	 *   @abstract Construct a matching dictionary for property matching.
	 */
	static OSDictionary *
	CreatePropertyMatchingDictionary(const char * key, OSObjectPtr value, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreatePropertyMatchingDictionary
	 *   @abstract Construct a matching dictionary for property matching.
	 */
	static OSDictionary *
	CreatePropertyMatchingDictionary(const char * key, const char * stringValue, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateKernelClassMatchingDictionary
	 *   @abstract Construct a matching dictionary for kernel class matching.
	 */
	static OSDictionary *
	CreateKernelClassMatchingDictionary(OSString * className, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateKernelClassMatchingDictionary
	 *   @abstract Construct a matching dictionary for kernel class matching.
	 */
	static OSDictionary *
	CreateKernelClassMatchingDictionary(const char * className, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateUserClassMatchingDictionary
	 *   @abstract Construct a matching dictionary for user class matching.
	 */
	static OSDictionary *
	CreateUserClassMatchingDictionary(OSString * className, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateUserClassMatchingDictionary
	 *   @abstract Construct a matching dictionary for user class matching.
	 */
	static OSDictionary *
	CreateUserClassMatchingDictionary(const char * className, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateNameMatchingDictionary
	 *   @abstract Construct a matching dictionary for IOService name matching.
	 */
	static OSDictionary *
	CreateNameMatchingDictionary(OSString * serviceName, OSDictionary * matching) LOCALONLY;

	/*! @function IOCreateNameMatchingDictionary
	 *   @abstract Construct a matching dictionary for IOService name matching.
	 */
	static OSDictionary *
	CreateNameMatchingDictionary(const char * serviceName, OSDictionary * matching) LOCALONLY;


private:
	virtual void
	Stop_async(
		IOService          * provider) LOCAL;
};

#undef KERNEL
#else /* __DOCUMENTATION__ */

/* generated class IOService IOService.iig:61-360 */

#define IOService_Start_ID            0xab6f76dde6d693f2ULL
#define IOService_Stop_ID            0x98e715041c459fa5ULL
#define IOService_ClientCrashed_ID            0xf608d8d479c824a3ULL
#define IOService_GetRegistryEntryID_ID            0x8a6f522f3894a0d7ULL
#define IOService_SetName_ID            0xff378dd57615eb9fULL
#define IOService_RegisterService_ID            0xe9722c2bb1347c28ULL
#define IOService_CopyProperties_ID            0x833ec8f92f8b5362ULL
#define IOService_SearchProperty_ID            0xad751cdb94c8dcc8ULL
#define IOService_SetProperties_ID            0xd8a753ea156f9162ULL
#define IOService_SetPowerState_ID            0xd200fde7d57ecca6ULL
#define IOService_ChangePowerState_ID            0xdb75cfc3395484a0ULL
#define IOService_NewUserClient_ID            0xf669efffcb89ed9cULL
#define IOService_Create_ID            0xe1a46dbd68bbe09cULL
#define IOService_Terminate_ID            0xf7a595d9927810c8ULL
#define IOService_CopyProviderProperties_ID            0xc2a554959002c8e7ULL
#define IOService_RequireMaxBusStall_ID            0xc21228652ff536afULL
#define IOService_Stop_async_ID            0xa8c93137712a16a2ULL

#define IOService_Start_Args \
        IOService * provider

#define IOService_Stop_Args \
        IOService * provider

#define IOService_ClientCrashed_Args \
        IOService * client, \
        uint64_t options

#define IOService_GetRegistryEntryID_Args \
        uint64_t * registryEntryID

#define IOService_SetName_Args \
        const char * name

#define IOService_RegisterService_Args \


#define IOService_SetDispatchQueue_Args \
        const char * name, \
        IODispatchQueue * queue

#define IOService_CopyDispatchQueue_Args \
        const char * name, \
        IODispatchQueue ** queue

#define IOService_CopyProperties_Args \
        OSDictionary ** properties

#define IOService_SearchProperty_Args \
        const char * name, \
        const char * plane, \
        uint64_t options, \
        OSContainer ** property

#define IOService_SetProperties_Args \
        OSDictionary * properties

#define IOService_SetPowerState_Args \
        uint32_t powerFlags

#define IOService_ChangePowerState_Args \
        uint32_t powerFlags

#define IOService_NewUserClient_Args \
        uint32_t type, \
        IOUserClient ** userClient

#define IOService_Create_Args \
        IOService * provider, \
        const char * propertiesKey, \
        IOService ** result

#define IOService_Terminate_Args \
        uint64_t options

#define IOService_CopyProviderProperties_Args \
        OSArray * propertyKeys, \
        OSArray ** properties

#define IOService_RequireMaxBusStall_Args \
        uint64_t maxBusStall

#define IOService_Stop_async_Args \
        IOService * provider

#define IOService_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(IOService * self, const IORPC rpc);\
\
    kern_return_t\
    Start(\
        IOService * provider,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    Stop(\
        IOService * provider,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    ClientCrashed(\
        IOService * client,\
        uint64_t options,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    GetRegistryEntryID(\
        uint64_t * registryEntryID,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SetName(\
        const IOServiceName name,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    RegisterService(\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CopyProperties(\
        OSDictionary ** properties,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SearchProperty(\
        const IOPropertyName name,\
        const IORegistryPlaneName plane,\
        uint64_t options,\
        OSContainer ** property,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SetProperties(\
        OSDictionary * properties,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SetPowerState(\
        uint32_t powerFlags,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    ChangePowerState(\
        uint32_t powerFlags,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    NewUserClient(\
        uint32_t type,\
        IOUserClient ** userClient,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    Create(\
        IOService * provider,\
        const IOPropertyName propertiesKey,\
        IOService ** result,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    Terminate(\
        uint64_t options,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CopyProviderProperties(\
        OSArray * propertyKeys,\
        OSArray ** properties,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    RequireMaxBusStall(\
        uint64_t maxBusStall,\
        OSDispatchMethod supermethod = NULL);\
\
    static OSDictionary *\
    CreatePropertyMatchingDictionary(\
        const char * key,\
        OSObjectPtr value,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreatePropertyMatchingDictionary(\
        const char * key,\
        const char * stringValue,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateKernelClassMatchingDictionary(\
        OSString * className,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateKernelClassMatchingDictionary(\
        const char * className,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateUserClassMatchingDictionary(\
        OSString * className,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateUserClassMatchingDictionary(\
        const char * className,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateNameMatchingDictionary(\
        OSString * serviceName,\
        OSDictionary * matching);\
\
    static OSDictionary *\
    CreateNameMatchingDictionary(\
        const char * serviceName,\
        OSDictionary * matching);\
\
    void\
    Stop_async(\
        IOService * provider,\
        OSDispatchMethod supermethod = NULL);\
\
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    Start_Impl(IOService_Start_Args);\
\
    kern_return_t\
    Stop_Impl(IOService_Stop_Args);\
\
    kern_return_t\
    GetRegistryEntryID_Impl(IOService_GetRegistryEntryID_Args);\
\
    kern_return_t\
    SetDispatchQueue_Impl(OSObject_SetDispatchQueue_Args);\
\
    kern_return_t\
    SetPowerState_Impl(IOService_SetPowerState_Args);\
\
    kern_return_t\
    Create_Impl(IOService_Create_Args);\
\
    void\
    Stop_async_Impl(IOService_Stop_async_Args);\
\
\
public:\
    /* _Invoke methods */\
\
    typedef kern_return_t (*Start_Handler)(OSMetaClassBase * target, IOService_Start_Args);\
    static kern_return_t\
    Start_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Start_Handler func);\
\
    typedef kern_return_t (*Stop_Handler)(OSMetaClassBase * target, IOService_Stop_Args);\
    static kern_return_t\
    Stop_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Stop_Handler func);\
\
    typedef kern_return_t (*ClientCrashed_Handler)(OSMetaClassBase * target, IOService_ClientCrashed_Args);\
    static kern_return_t\
    ClientCrashed_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        ClientCrashed_Handler func);\
\
    typedef kern_return_t (*GetRegistryEntryID_Handler)(OSMetaClassBase * target, IOService_GetRegistryEntryID_Args);\
    static kern_return_t\
    GetRegistryEntryID_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        GetRegistryEntryID_Handler func);\
\
    typedef kern_return_t (*SetName_Handler)(OSMetaClassBase * target, IOService_SetName_Args);\
    static kern_return_t\
    SetName_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetName_Handler func);\
\
    typedef kern_return_t (*RegisterService_Handler)(OSMetaClassBase * targetIOService_RegisterService_Args);\
    static kern_return_t\
    RegisterService_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        RegisterService_Handler func);\
\
    typedef kern_return_t (*CopyProperties_Handler)(OSMetaClassBase * target, IOService_CopyProperties_Args);\
    static kern_return_t\
    CopyProperties_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CopyProperties_Handler func);\
\
    typedef kern_return_t (*SearchProperty_Handler)(OSMetaClassBase * target, IOService_SearchProperty_Args);\
    static kern_return_t\
    SearchProperty_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SearchProperty_Handler func);\
\
    typedef kern_return_t (*SetProperties_Handler)(OSMetaClassBase * target, IOService_SetProperties_Args);\
    static kern_return_t\
    SetProperties_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetProperties_Handler func);\
\
    typedef kern_return_t (*SetPowerState_Handler)(OSMetaClassBase * target, IOService_SetPowerState_Args);\
    static kern_return_t\
    SetPowerState_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetPowerState_Handler func);\
\
    typedef kern_return_t (*ChangePowerState_Handler)(OSMetaClassBase * target, IOService_ChangePowerState_Args);\
    static kern_return_t\
    ChangePowerState_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        ChangePowerState_Handler func);\
\
    typedef kern_return_t (*NewUserClient_Handler)(OSMetaClassBase * target, IOService_NewUserClient_Args);\
    static kern_return_t\
    NewUserClient_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        NewUserClient_Handler func);\
\
    typedef kern_return_t (*Create_Handler)(OSMetaClassBase * target, IOService_Create_Args);\
    static kern_return_t\
    Create_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Create_Handler func);\
\
    typedef kern_return_t (*Terminate_Handler)(OSMetaClassBase * target, IOService_Terminate_Args);\
    static kern_return_t\
    Terminate_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Terminate_Handler func);\
\
    typedef kern_return_t (*CopyProviderProperties_Handler)(OSMetaClassBase * target, IOService_CopyProviderProperties_Args);\
    static kern_return_t\
    CopyProviderProperties_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CopyProviderProperties_Handler func);\
\
    typedef kern_return_t (*RequireMaxBusStall_Handler)(OSMetaClassBase * target, IOService_RequireMaxBusStall_Args);\
    static kern_return_t\
    RequireMaxBusStall_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        RequireMaxBusStall_Handler func);\
\
    typedef void (*Stop_async_Handler)(OSMetaClassBase * target, IOService_Stop_async_Args);\
    static kern_return_t\
    Stop_async_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Stop_async_Handler func);\
\


#define IOService_KernelMethods \
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    ClientCrashed_Impl(IOService_ClientCrashed_Args);\
\
    kern_return_t\
    SetName_Impl(IOService_SetName_Args);\
\
    kern_return_t\
    RegisterService_Impl(IOService_RegisterService_Args);\
\
    kern_return_t\
    CopyDispatchQueue_Impl(OSObject_CopyDispatchQueue_Args);\
\
    kern_return_t\
    CopyProperties_Impl(IOService_CopyProperties_Args);\
\
    kern_return_t\
    SearchProperty_Impl(IOService_SearchProperty_Args);\
\
    kern_return_t\
    SetProperties_Impl(IOService_SetProperties_Args);\
\
    kern_return_t\
    ChangePowerState_Impl(IOService_ChangePowerState_Args);\
\
    kern_return_t\
    NewUserClient_Impl(IOService_NewUserClient_Args);\
\
    kern_return_t\
    Terminate_Impl(IOService_Terminate_Args);\
\
    kern_return_t\
    CopyProviderProperties_Impl(IOService_CopyProviderProperties_Args);\
\
    kern_return_t\
    RequireMaxBusStall_Impl(IOService_RequireMaxBusStall_Args);\
\


#define IOService_VirtualMethods \
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

extern OSMetaClass          * gIOServiceMetaClass;
extern const OSClassLoadInformation IOService_Class;

class IOServiceMetaClass : public OSMetaClass
{
public:
    virtual kern_return_t
    New(OSObject * instance) override;
    virtual kern_return_t
    Dispatch(const IORPC rpc) override;
};

#endif /* !KERNEL */

#if !KERNEL

class IOServiceInterface : public OSInterface
{
public:
};

struct IOService_IVars;
struct IOService_LocalIVars;

class IOService : public OSObject, public IOServiceInterface
{
#if !KERNEL
    friend class IOServiceMetaClass;
#endif /* !KERNEL */

#if !KERNEL
public:
#ifdef IOService_DECLARE_IVARS
IOService_DECLARE_IVARS
#else /* IOService_DECLARE_IVARS */
    union
    {
        IOService_IVars * ivars;
        IOService_LocalIVars * lvars;
    };
#endif /* IOService_DECLARE_IVARS */
#endif /* !KERNEL */

#if !KERNEL
    static OSMetaClass *
    sGetMetaClass() { return gIOServiceMetaClass; };
#endif /* KERNEL */

    using super = OSObject;

#if !KERNEL
    IOService_Methods
    IOService_VirtualMethods
#endif /* !KERNEL */

};
#endif /* !KERNEL */


#endif /* !__DOCUMENTATION__ */

/* IOService.iig:362- */

#endif /* ! _IOKIT_UIOSERVICE_H */
