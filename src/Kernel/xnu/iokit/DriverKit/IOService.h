/* iig-lite generated from IOService.iig - kernel-side subset; msgids are NOT Apple-ABI */

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
class IOServiceStateNotificationDispatchSource;

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
	kIOServicePowerCapabilityLPW = 0x00020000,
};

enum {
	_kIOPMWakeEventSource           = 0x00000001,
	_kIOPMWakeEventFullWake         = 0x00000002,
	_kIOPMWakeEventPossibleFullWake = 0x00000004,
};

// values for OSNumber kIOSystemStateHaltDescriptionKey:kIOSystemStateHaltDescriptionHaltStateKey
enum {
	kIOServiceHaltStatePowerOff = 0x00000001,
	kIOServiceHaltStateRestart  = 0x00000002,
};

// Bitfields for CreatePMAssertion
enum {
    /*! kIOServicePMAssertionCPUBit
     * When set, PM kernel will prefer to leave the CPU and core hardware
     * running in "Dark Wake" state, instead of sleeping.
     */
	kIOServicePMAssertionCPUBit             = 0x001,

    /*! kIOServicePMAssertionForceFullWakeupBit
     * When set, the system will immediately do a full wakeup after going to sleep.
     */
	kIOServicePMAssertionForceFullWakeupBit = 0x800,
};

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

/* source class IOService IOService.iig:107-604 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

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
     * @brief       Create the default IODispatchQueue for an IOService. IOService::init()
     *              calls this to create its default queue.
     * @param       queue Returned, retained queue or NULL.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	CreateDefaultDispatchQueue(
		IODispatchQueue        ** queue) LOCAL;

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
     * @brief       Add an IOService created by Create() to the power manangement tree.
     * @discussion  IOService objects created by matching on a provider are always added to the power management tree.
     *              Any IOService created with the Create() API is not, but may be added by calling this method.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	JoinPMTree(void);

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
     * @brief       Request provider to create a power override.
     * @discussion  Allows a driver to ignore power desires of its children, similar to powerOverrideOnPriv in IOKit, enabling its power state to be governed solely by its own desire (set via IOService::ChangePowerState)
     * @param       enable Whether to enable or disable the power override.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SetPowerOverride(
		bool enable);

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

	/*! @function AdjustBusy
	 * @discussion Adjust the busy state of this service by applying a delta to the current busy state.
	 *             Adjusting the busy state of a service to or from zero will change the provider's busy state by one, in the same direction.
	 * @param       delta  The delta value to apply to the busy state.
	 * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	AdjustBusy(int32_t delta);

	/*! @function GetBusyState
	 * @discussion Get the busy state of this service.
	 * @param      busyState The returned busy state.
	 * @return     kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	GetBusyState(uint32_t *busyState);

   /*!
    * @brief       Post an event to CoreAnalytics.
    * @discussion  Post an event to CoreAnalytics. See the CoreAnalytics documentation for
    *              details.
    * @param       options No options currently defined pass zero.
    * @param       eventName See the CoreAnalytics documentation for details.
    * @param       eventPayload See the CoreAnalytics documentation for details.
    * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
    */
	virtual kern_return_t
	CoreAnalyticsSendEvent(
		uint64_t       options,
		OSString     * eventName,
		OSDictionary * eventPayload);

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

	/*! @function UpdateReport
	 *  @abstract update an IOReporting subscription by reading out channel data.
	 */
	virtual IOReturn UpdateReport(OSData *channels, uint32_t action,
                                   uint32_t *outElementCount,
                                   uint64_t offset, uint64_t capacity,
                                   IOMemoryDescriptor *buffer);

	/*! @function ConfigureReport
	*   @abstract Configure an IOReporting subscription
	*   @discussion outCount is counting channels for enable,disable.  It is counting
	*     elements for getDimensions
	*/
	virtual IOReturn ConfigureReport(OSData *channels, uint32_t action, uint32_t *outCount);

	/*! @function SetLegend
	 * @abstract set IORLegend and IORLegendPublic ioreg properties on this service.
	 * @discussion For use by DriverKit userspace services, since they can't set
	 *  registry properties directly.
	 */
	virtual IOReturn SetLegend(OSArray *legend, bool is_public);

	/*!
	 * @brief       Get the IORegistryEntry name.
	 * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	CopyName(OSString ** name);

	/*! @function StringFromReturn
	 *   @abstract Get a string description for an IOReturn value.
	 *   @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	StringFromReturn(
		 IOReturn    retval,
		 OSString ** str);

	virtual kern_return_t
	_ClaimSystemWakeEvent(
		IOService          * device,
		uint64_t             flags,
		const IOPropertyName reason,
		OSContainer       *  details);

#if PRIVATE_WIFI_ONLY
	/*!
	 * @brief      Optionally supported external method to set properties in this service.
	 * @param      properties The properties to set.
	 * @return     kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	UserSetProperties(OSContainer * properties) LOCAL;

    /*!
     * @brief       Send the kIOMessageServicePropertyChange message
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	virtual kern_return_t
	SendIOMessageServicePropertyChange();

	const char *
	StringFromReturn(
		 IOReturn    retval) LOCALONLY;

#endif /* PRIVATE_WIFI_ONLY */

	/*! @function RemoveProperty
	 *   @abstract Remove a property from the IOService.
	 *   @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	 */
	virtual kern_return_t
	RemoveProperty(OSString * propertyName);

	/*! @function GetProvider
	 *   @abstract Get the provider of this IOService.
	 *   @discussion The DriverKit runtime caches the provider passed to IOService::Start(IOService * provider).
	 *               This method returns the cached object.
	 */
	IOService *
	GetProvider() const LOCALONLY;

   /*!
    * @function CopySystemStateNotificationService
    * @abstract Obtain the system state notification service.
    * @param    service Return IOService object with +1 retain count, to be released
    *           by the caller.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	CopySystemStateNotificationService(IOService ** service);

   /*!
    * @function StateNotificationItemCreate
    * @abstract Create a state notification item.
    * @param    itemName name of the item.
    * @param    value initial value of the item. Can be set to NULL.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	StateNotificationItemCreate(OSString * itemName, OSDictionary * value);

   /*!
    * @function StateNotificationItemSet
    * @abstract Set the value of a state notification item.
    * @param    itemName name of the item.
    * @param    value dictionary value for the item, item creator to define.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	StateNotificationItemSet(OSString * itemName, OSDictionary * value);

   /*!
    * @function StateNotificationItemCopy
    * @abstract Set the value of a state notification item.
    * @param    itemName name of the item.
    * @param    value dictionary value for the item, item creator to define.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	StateNotificationItemCopy(OSString * itemName, OSDictionary ** value);

   /*!
    * @function CreatePMAssertion
    * @abstract Create a power management assertion.
    * @param    assertionBits Bit masks including all the flavors that require to be asserted.
    * @param    assertionID pointer that will contain the unique identifier of the created
    *           power assertion.
    * @param    synced indicates if the assertion must prevent an imminent sleep transition.
    *           When set to true, and if a system sleep is irreversible, the call will return
    *           kIOReturnBusy, in which case the assertion is not created. Only
    *           kIOServicePMAssertionCPUBit is valid for assertionBits if sleepSafe is set to
    *           true.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	CreatePMAssertion(uint32_t assertionBits, uint64_t * assertionID, bool synced);

   /*!
    * @function ReleasePMAssertion
    * @abstract Release a previously created power management assertion.
    * @param    assertionID the assertion ID returned by CreatePMAssertion.
    * @return   kIOReturnSuccess on success. See IOReturn.h for error codes.
	*/
	virtual kern_return_t
	ReleasePMAssertion(uint64_t assertionID);

private:
	virtual void
	Stop_async(
		IOService          * provider) LOCAL;

	virtual kern_return_t
	_NewUserClient(
		uint32_t type,
		OSDictionary *  entitlements,
		IOUserClient ** userClient) LOCAL;
};

#undef KERNEL
#else /* __DOCUMENTATION__ */

/* generated class IOService IOService.iig:107-604 */

#define IOService_Start_ID            0xf6f426352a1c353eULL
#define IOService_Stop_ID            0xde435fcc366d7edaULL
#define IOService_ClientCrashed_ID            0x50b09593011fefecULL
#define IOService_GetRegistryEntryID_ID            0xc64a7afc94271418ULL
#define IOService_SetName_ID            0x163b417fa467310fULL
#define IOService_RegisterService_ID            0x0e98c1151bbd9c67ULL
#define IOService_CreateDefaultDispatchQueue_ID            0x92d265e60e237f9eULL
#define IOService_CopyProperties_ID            0x5784fc96e2f4c58dULL
#define IOService_SearchProperty_ID            0x134ebd2ff4760828ULL
#define IOService_SetProperties_ID            0x48fc8c2408f7c750ULL
#define IOService_JoinPMTree_ID            0x5f673b878b30f29cULL
#define IOService_SetPowerState_ID            0x6228a2d1e7d3194bULL
#define IOService_ChangePowerState_ID            0xf999fe0203243723ULL
#define IOService_SetPowerOverride_ID            0x8557b79c48db868cULL
#define IOService_NewUserClient_ID            0x432ea52220dd49f3ULL
#define IOService_Create_ID            0xb185332687908e2eULL
#define IOService_Terminate_ID            0xad4452655ae445b3ULL
#define IOService_CopyProviderProperties_ID            0x37b0be4369e89e02ULL
#define IOService_RequireMaxBusStall_ID            0xa14e2b1ee3658977ULL
#define IOService_AdjustBusy_ID            0x932cad729ca97d26ULL
#define IOService_GetBusyState_ID            0xaa45c1f451675d51ULL
#define IOService_CoreAnalyticsSendEvent_ID            0x4637b898595b170aULL
#define IOService_UpdateReport_ID            0xa7f313ccedc73c38ULL
#define IOService_ConfigureReport_ID            0x9e1e556f26d94fcfULL
#define IOService_SetLegend_ID            0xd74e2078bffdd0b7ULL
#define IOService_CopyName_ID            0xcdf4a9bcc715cd3cULL
#define IOService_StringFromReturn_ID            0xfcc3f0eceb556499ULL
#define IOService__ClaimSystemWakeEvent_ID            0x79c0774f4562beffULL
#define IOService_UserSetProperties_ID            0x0151b1a33b12fdd8ULL
#define IOService_SendIOMessageServicePropertyChange_ID            0xdf52ff390b68607eULL
#define IOService_RemoveProperty_ID            0x60e3dd00874c1149ULL
#define IOService_CopySystemStateNotificationService_ID            0x436dcf96b0fbc909ULL
#define IOService_StateNotificationItemCreate_ID            0x739223be085517f1ULL
#define IOService_StateNotificationItemSet_ID            0xb668e7a575264847ULL
#define IOService_StateNotificationItemCopy_ID            0x9b1059c3262c3694ULL
#define IOService_CreatePMAssertion_ID            0x8284ca11beb8d95fULL
#define IOService_ReleasePMAssertion_ID            0x9099180cadd05a78ULL
#define IOService_Stop_async_ID            0xe0ae185736c0b8c1ULL
#define IOService__NewUserClient_ID            0xae417ba2fd3f8284ULL

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
        const IOServiceName name

#define IOService_RegisterService_Args \


#define IOService_SetDispatchQueue_Args \
        const IODispatchQueueName name, \
        IODispatchQueue * queue

#define IOService_CopyDispatchQueue_Args \
        const IODispatchQueueName name, \
        IODispatchQueue ** queue

#define IOService_CreateDefaultDispatchQueue_Args \
        IODispatchQueue ** queue

#define IOService_CopyProperties_Args \
        OSDictionary ** properties

#define IOService_SearchProperty_Args \
        const IOPropertyName name, \
        const IORegistryPlaneName plane, \
        uint64_t options, \
        OSContainer ** property

#define IOService_SetProperties_Args \
        OSDictionary * properties

#define IOService_JoinPMTree_Args \


#define IOService_SetPowerState_Args \
        uint32_t powerFlags

#define IOService_ChangePowerState_Args \
        uint32_t powerFlags

#define IOService_SetPowerOverride_Args \
        bool enable

#define IOService_NewUserClient_Args \
        uint32_t type, \
        IOUserClient ** userClient

#define IOService_Create_Args \
        IOService * provider, \
        const IOPropertyName propertiesKey, \
        IOService ** result

#define IOService_Terminate_Args \
        uint64_t options

#define IOService_CopyProviderProperties_Args \
        OSArray * propertyKeys, \
        OSArray ** properties

#define IOService_RequireMaxBusStall_Args \
        uint64_t maxBusStall

#define IOService_AdjustBusy_Args \
        int32_t delta

#define IOService_GetBusyState_Args \
        uint32_t * busyState

#define IOService_CoreAnalyticsSendEvent_Args \
        uint64_t options, \
        OSString * eventName, \
        OSDictionary * eventPayload

#define IOService_UpdateReport_Args \
        OSData * channels, \
        uint32_t action, \
        uint32_t * outElementCount, \
        uint64_t offset, \
        uint64_t capacity, \
        IOMemoryDescriptor * buffer

#define IOService_ConfigureReport_Args \
        OSData * channels, \
        uint32_t action, \
        uint32_t * outCount

#define IOService_SetLegend_Args \
        OSArray * legend, \
        bool is_public

#define IOService_CopyName_Args \
        OSString ** name

#define IOService_StringFromReturn_Args \
        IOReturn retval, \
        OSString ** str

#define IOService__ClaimSystemWakeEvent_Args \
        IOService * device, \
        uint64_t flags, \
        const IOPropertyName reason, \
        OSContainer * details

#define IOService_UserSetProperties_Args \
        OSContainer * properties

#define IOService_SendIOMessageServicePropertyChange_Args \


#define IOService_RemoveProperty_Args \
        OSString * propertyName

#define IOService_CopySystemStateNotificationService_Args \
        IOService ** service

#define IOService_StateNotificationItemCreate_Args \
        OSString * itemName, \
        OSDictionary * value

#define IOService_StateNotificationItemSet_Args \
        OSString * itemName, \
        OSDictionary * value

#define IOService_StateNotificationItemCopy_Args \
        OSString * itemName, \
        OSDictionary ** value

#define IOService_CreatePMAssertion_Args \
        uint32_t assertionBits, \
        uint64_t * assertionID, \
        bool synced

#define IOService_ReleasePMAssertion_Args \
        uint64_t assertionID

#define IOService_Stop_async_Args \
        IOService * provider

#define IOService__NewUserClient_Args \
        uint32_t type, \
        OSDictionary * entitlements, \
        IOUserClient ** userClient

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
        const char * name,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    RegisterService(\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CreateDefaultDispatchQueue(\
        IODispatchQueue ** queue,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CopyProperties(\
        OSDictionary ** properties,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SearchProperty(\
        const char * name,\
        const char * plane,\
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
    JoinPMTree(\
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
    SetPowerOverride(\
        bool enable,\
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
        const char * propertiesKey,\
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
    kern_return_t\
    AdjustBusy(\
        int32_t delta,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    GetBusyState(\
        uint32_t * busyState,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CoreAnalyticsSendEvent(\
        uint64_t options,\
        OSString * eventName,\
        OSDictionary * eventPayload,\
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
    IOReturn\
    UpdateReport(\
        OSData * channels,\
        uint32_t action,\
        uint32_t * outElementCount,\
        uint64_t offset,\
        uint64_t capacity,\
        IOMemoryDescriptor * buffer,\
        OSDispatchMethod supermethod = NULL);\
\
    IOReturn\
    ConfigureReport(\
        OSData * channels,\
        uint32_t action,\
        uint32_t * outCount,\
        OSDispatchMethod supermethod = NULL);\
\
    IOReturn\
    SetLegend(\
        OSArray * legend,\
        bool is_public,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CopyName(\
        OSString ** name,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    StringFromReturn(\
        IOReturn retval,\
        OSString ** str,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    _ClaimSystemWakeEvent(\
        IOService * device,\
        uint64_t flags,\
        const char * reason,\
        OSContainer * details,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    UserSetProperties(\
        OSContainer * properties,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SendIOMessageServicePropertyChange(\
        OSDispatchMethod supermethod = NULL);\
\
    const char *\
    StringFromReturn(\
        IOReturn retval);\
\
    kern_return_t\
    RemoveProperty(\
        OSString * propertyName,\
        OSDispatchMethod supermethod = NULL);\
\
    IOService *\
    GetProvider(\
) const;\
\
    kern_return_t\
    CopySystemStateNotificationService(\
        IOService ** service,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    StateNotificationItemCreate(\
        OSString * itemName,\
        OSDictionary * value,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    StateNotificationItemSet(\
        OSString * itemName,\
        OSDictionary * value,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    StateNotificationItemCopy(\
        OSString * itemName,\
        OSDictionary ** value,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CreatePMAssertion(\
        uint32_t assertionBits,\
        uint64_t * assertionID,\
        bool synced,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    ReleasePMAssertion(\
        uint64_t assertionID,\
        OSDispatchMethod supermethod = NULL);\
\
    void\
    Stop_async(\
        IOService * provider,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    _NewUserClient(\
        uint32_t type,\
        OSDictionary * entitlements,\
        IOUserClient ** userClient,\
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
    CreateDefaultDispatchQueue_Impl(IOService_CreateDefaultDispatchQueue_Args);\
\
    kern_return_t\
    SetPowerState_Impl(IOService_SetPowerState_Args);\
\
    kern_return_t\
    Create_Impl(IOService_Create_Args);\
\
    kern_return_t\
    UserSetProperties_Impl(IOService_UserSetProperties_Args);\
\
    void\
    Stop_async_Impl(IOService_Stop_async_Args);\
\
    kern_return_t\
    _NewUserClient_Impl(IOService__NewUserClient_Args);\
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
    typedef kern_return_t (*RegisterService_Handler)(OSMetaClassBase * target);\
    static kern_return_t\
    RegisterService_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        RegisterService_Handler func);\
\
    typedef kern_return_t (*CreateDefaultDispatchQueue_Handler)(OSMetaClassBase * target, IOService_CreateDefaultDispatchQueue_Args);\
    static kern_return_t\
    CreateDefaultDispatchQueue_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CreateDefaultDispatchQueue_Handler func);\
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
    typedef kern_return_t (*JoinPMTree_Handler)(OSMetaClassBase * target);\
    static kern_return_t\
    JoinPMTree_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        JoinPMTree_Handler func);\
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
    typedef kern_return_t (*SetPowerOverride_Handler)(OSMetaClassBase * target, IOService_SetPowerOverride_Args);\
    static kern_return_t\
    SetPowerOverride_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetPowerOverride_Handler func);\
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
    typedef kern_return_t (*AdjustBusy_Handler)(OSMetaClassBase * target, IOService_AdjustBusy_Args);\
    static kern_return_t\
    AdjustBusy_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        AdjustBusy_Handler func);\
\
    typedef kern_return_t (*GetBusyState_Handler)(OSMetaClassBase * target, IOService_GetBusyState_Args);\
    static kern_return_t\
    GetBusyState_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        GetBusyState_Handler func);\
\
    typedef kern_return_t (*CoreAnalyticsSendEvent_Handler)(OSMetaClassBase * target, IOService_CoreAnalyticsSendEvent_Args);\
    static kern_return_t\
    CoreAnalyticsSendEvent_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CoreAnalyticsSendEvent_Handler func);\
\
    typedef IOReturn (*UpdateReport_Handler)(OSMetaClassBase * target, IOService_UpdateReport_Args);\
    static kern_return_t\
    UpdateReport_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        UpdateReport_Handler func);\
\
    typedef IOReturn (*ConfigureReport_Handler)(OSMetaClassBase * target, IOService_ConfigureReport_Args);\
    static kern_return_t\
    ConfigureReport_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        ConfigureReport_Handler func);\
\
    typedef IOReturn (*SetLegend_Handler)(OSMetaClassBase * target, IOService_SetLegend_Args);\
    static kern_return_t\
    SetLegend_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetLegend_Handler func);\
\
    typedef kern_return_t (*CopyName_Handler)(OSMetaClassBase * target, IOService_CopyName_Args);\
    static kern_return_t\
    CopyName_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CopyName_Handler func);\
\
    typedef kern_return_t (*StringFromReturn_Handler)(OSMetaClassBase * target, IOService_StringFromReturn_Args);\
    static kern_return_t\
    StringFromReturn_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        StringFromReturn_Handler func);\
\
    typedef kern_return_t (*_ClaimSystemWakeEvent_Handler)(OSMetaClassBase * target, IOService__ClaimSystemWakeEvent_Args);\
    static kern_return_t\
    _ClaimSystemWakeEvent_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        _ClaimSystemWakeEvent_Handler func);\
\
    typedef kern_return_t (*UserSetProperties_Handler)(OSMetaClassBase * target, IOService_UserSetProperties_Args);\
    static kern_return_t\
    UserSetProperties_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        UserSetProperties_Handler func);\
\
    typedef kern_return_t (*SendIOMessageServicePropertyChange_Handler)(OSMetaClassBase * target);\
    static kern_return_t\
    SendIOMessageServicePropertyChange_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SendIOMessageServicePropertyChange_Handler func);\
\
    typedef kern_return_t (*RemoveProperty_Handler)(OSMetaClassBase * target, IOService_RemoveProperty_Args);\
    static kern_return_t\
    RemoveProperty_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        RemoveProperty_Handler func);\
\
    typedef kern_return_t (*CopySystemStateNotificationService_Handler)(OSMetaClassBase * target, IOService_CopySystemStateNotificationService_Args);\
    static kern_return_t\
    CopySystemStateNotificationService_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CopySystemStateNotificationService_Handler func);\
\
    typedef kern_return_t (*StateNotificationItemCreate_Handler)(OSMetaClassBase * target, IOService_StateNotificationItemCreate_Args);\
    static kern_return_t\
    StateNotificationItemCreate_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        StateNotificationItemCreate_Handler func);\
\
    typedef kern_return_t (*StateNotificationItemSet_Handler)(OSMetaClassBase * target, IOService_StateNotificationItemSet_Args);\
    static kern_return_t\
    StateNotificationItemSet_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        StateNotificationItemSet_Handler func);\
\
    typedef kern_return_t (*StateNotificationItemCopy_Handler)(OSMetaClassBase * target, IOService_StateNotificationItemCopy_Args);\
    static kern_return_t\
    StateNotificationItemCopy_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        StateNotificationItemCopy_Handler func);\
\
    typedef kern_return_t (*CreatePMAssertion_Handler)(OSMetaClassBase * target, IOService_CreatePMAssertion_Args);\
    static kern_return_t\
    CreatePMAssertion_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CreatePMAssertion_Handler func);\
\
    typedef kern_return_t (*ReleasePMAssertion_Handler)(OSMetaClassBase * target, IOService_ReleasePMAssertion_Args);\
    static kern_return_t\
    ReleasePMAssertion_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        ReleasePMAssertion_Handler func);\
\
    typedef void (*Stop_async_Handler)(OSMetaClassBase * target, IOService_Stop_async_Args);\
    static kern_return_t\
    Stop_async_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        Stop_async_Handler func);\
\
    typedef kern_return_t (*_NewUserClient_Handler)(OSMetaClassBase * target, IOService__NewUserClient_Args);\
    static kern_return_t\
    _NewUserClient_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        _NewUserClient_Handler func);\
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
    JoinPMTree_Impl(IOService_JoinPMTree_Args);\
\
    kern_return_t\
    ChangePowerState_Impl(IOService_ChangePowerState_Args);\
\
    kern_return_t\
    SetPowerOverride_Impl(IOService_SetPowerOverride_Args);\
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
    kern_return_t\
    AdjustBusy_Impl(IOService_AdjustBusy_Args);\
\
    kern_return_t\
    GetBusyState_Impl(IOService_GetBusyState_Args);\
\
    kern_return_t\
    CoreAnalyticsSendEvent_Impl(IOService_CoreAnalyticsSendEvent_Args);\
\
    IOReturn\
    UpdateReport_Impl(IOService_UpdateReport_Args);\
\
    IOReturn\
    ConfigureReport_Impl(IOService_ConfigureReport_Args);\
\
    IOReturn\
    SetLegend_Impl(IOService_SetLegend_Args);\
\
    kern_return_t\
    CopyName_Impl(IOService_CopyName_Args);\
\
    kern_return_t\
    StringFromReturn_Impl(IOService_StringFromReturn_Args);\
\
    kern_return_t\
    _ClaimSystemWakeEvent_Impl(IOService__ClaimSystemWakeEvent_Args);\
\
    kern_return_t\
    SendIOMessageServicePropertyChange_Impl(IOService_SendIOMessageServicePropertyChange_Args);\
\
    kern_return_t\
    RemoveProperty_Impl(IOService_RemoveProperty_Args);\
\
    kern_return_t\
    CopySystemStateNotificationService_Impl(IOService_CopySystemStateNotificationService_Args);\
\
    kern_return_t\
    StateNotificationItemCreate_Impl(IOService_StateNotificationItemCreate_Args);\
\
    kern_return_t\
    StateNotificationItemSet_Impl(IOService_StateNotificationItemSet_Args);\
\
    kern_return_t\
    StateNotificationItemCopy_Impl(IOService_StateNotificationItemCopy_Args);\
\
    kern_return_t\
    CreatePMAssertion_Impl(IOService_CreatePMAssertion_Args);\
\
    kern_return_t\
    ReleasePMAssertion_Impl(IOService_ReleasePMAssertion_Args);\
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

class IOServiceInterface : public OSInterface
{
public:
};

struct IOService_IVars;
struct IOService_LocalIVars;

class IOService : public OSObject, public IOServiceInterface
{
public:
    union
    {
        IOService_IVars * ivars;
        IOService_LocalIVars * lvars;
    };
    using super = OSObject;

    IOService_Methods
    IOService_VirtualMethods
};

#endif /* !KERNEL */

#endif /* !__DOCUMENTATION__ */



#endif /* ! _IOKIT_UIOSERVICE_H */
