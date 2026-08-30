/* iig-lite generated from IOExtensiblePaniclog.iig - kernel-side subset; msgids are NOT Apple-ABI */

/*
 * Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOExtensiblePaniclog.h>
#endif
#endif

#ifndef _IOKIT_UIOEXTENSIBLEPANICLOG_H
#define _IOKIT_UIOEXTENSIBLEPANICLOG_H

#include <DriverKit/OSObject.h>  /* .iig include */
#include <DriverKit/IOBufferMemoryDescriptor.h>  /* .iig include */

/*!
                   
                                      
                                   
                                                               
        
*/

enum {
    kIOExtensiblePaniclogOptionsNone = 0x0,
    kIOExtensiblePaniclogOptionsWithBuffer = 0x1, 
};

/* source class IOExtensiblePaniclog IOExtensiblePaniclog.iig:54-161 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

class KERNEL IOExtensiblePaniclog : public OSObject
{
public:
    virtual bool
    init() override;

    virtual void
    free() override;

    /*!
     * @brief       This function is to be called to create IOExtensiblePaniclog object.
     * @discussion  First function to be called.
     * 
     * @param       uuid  The UUID of the handle.
     * @param       data_id The string describing the handle. MAX length of 32.
     * @param       max_len The maximum length of the buffer.
     * @param       options Options to be passed while creating the handle
     * @param       out The pointer to the created IOExtensiblePaniclog object. NULL in case of an error.
     * @return      True in case of success. False in case of an error.
     */
    static kern_return_t
    Create(OSData *uuid, OSString *data_id, uint32_t max_len, uint32_t options,
            IOExtensiblePaniclog **out);

    /*!
     * @brief       This function is called to set the IOExtensiblePaniclog object active.
     * @discussion  When it is set active, it is picked up and added to the extensible paniclog
     *              in case of a panic.
     * 
     * @return      0 on success, negative value in case of failure.
     */
    virtual kern_return_t SetActive();

    /*!
     * @brief       This function is called to set the IOExtensiblePaniclog object inactive.
     * @discussion  When it is set inactive, this buffer is not picked up in case of a panic
     * 
     * @return      True in case of success. False in case of an error.
     */ 
    virtual kern_return_t SetInactive();

    /*!
     * @brief       This function is called to insert data into the buffer.
     * @discussion  This function overwrites the data in the buffer. The write starts from
     *              offset 0 and continues until 'len'
     * 
     * @param       data Data to be inserted
     * @param       len The length to be copied.
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t InsertData(OSData *data);

    /*!
     * @brief       This function is called to insert data into the buffer.
     * @discussion  This function overwrites the data in the buffer. The write starts from
     *              last written byte and continues until 'len'
     * 
     * @param       data Data to be inserted
     * @param       len The length to be copied.
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t AppendData(OSData *data);

     /*!
     * @brief       Function to get the Memory descriptor created in the Create function
     * 
     * @param       mem The pointer to the IOBufferMemoryDescriptor object
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t CopyMemoryDescriptor(IOBufferMemoryDescriptor **mem);

     /*!
     * @brief       This function is called to get a pointer to the ext paniclog buffer
     * @discussion  After this function is called, the user is responsible for copying data into the buffer. 
     *              The entire buffer is copied when a system panics.
     *              After claiming the buffer, YieldBuffer() has to be called to set the used_len of the buffer
     *              before calling InsertData() or AppendData()
     *
     * @param       addr Address of the mapped buffer
     * @param       len The length of the mapped buffer. This is same value as the max_len in
     *                  the Create() function
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t ClaimBuffer(uint64_t *addr, uint64_t *len) LOCALONLY;

    /*!
     * @brief       This function is called to yield the buffer and set the used_len for the buffer
     * @discussion  After this function call, InsertData() and AppendData() can be called.
     *
     * @param       used_len The length of the buffer used by the client.
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t YieldBuffer(uint32_t used_len) LOCALONLY;

    /*!
     * @brief       This function is called to set the used len of the buffer
     *
     * @param       used_len The length of the buffer used by the client.
     * 
     * @return      0 in case of success. Negative in case of an error.
     */
    virtual kern_return_t SetUsedLen(uint32_t used_len);
};

#undef KERNEL
#else /* __DOCUMENTATION__ */

/* generated class IOExtensiblePaniclog IOExtensiblePaniclog.iig:54-161 */

#define IOExtensiblePaniclog_Create_ID            0x2ff068b540915c5fULL
#define IOExtensiblePaniclog_SetActive_ID            0x70b1dfa448ebc488ULL
#define IOExtensiblePaniclog_SetInactive_ID            0x8f685a2854c3e447ULL
#define IOExtensiblePaniclog_InsertData_ID            0x1704c634bd38f005ULL
#define IOExtensiblePaniclog_AppendData_ID            0x53befdf6beeeb91aULL
#define IOExtensiblePaniclog_CopyMemoryDescriptor_ID            0xf30385341a8578dfULL
#define IOExtensiblePaniclog_SetUsedLen_ID            0x165b7978e4e6087cULL

#define IOExtensiblePaniclog_Create_Args \
        OSData * uuid, \
        OSString * data_id, \
        uint32_t max_len, \
        uint32_t options, \
        IOExtensiblePaniclog ** out

#define IOExtensiblePaniclog_SetActive_Args \


#define IOExtensiblePaniclog_SetInactive_Args \


#define IOExtensiblePaniclog_InsertData_Args \
        OSData * data

#define IOExtensiblePaniclog_AppendData_Args \
        OSData * data

#define IOExtensiblePaniclog_CopyMemoryDescriptor_Args \
        IOBufferMemoryDescriptor ** mem

#define IOExtensiblePaniclog_SetUsedLen_Args \
        uint32_t used_len

#define IOExtensiblePaniclog_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(IOExtensiblePaniclog * self, const IORPC rpc);\
\
    static kern_return_t\
    Create(\
        OSData * uuid,\
        OSString * data_id,\
        uint32_t max_len,\
        uint32_t options,\
        IOExtensiblePaniclog ** out);\
\
    kern_return_t\
    SetActive(\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SetInactive(\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    InsertData(\
        OSData * data,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    AppendData(\
        OSData * data,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    CopyMemoryDescriptor(\
        IOBufferMemoryDescriptor ** mem,\
        OSDispatchMethod supermethod = NULL);\
\
    kern_return_t\
    SetUsedLen(\
        uint32_t used_len,\
        OSDispatchMethod supermethod = NULL);\
\
\
protected:\
    /* _Impl methods */\
\
\
public:\
    /* _Invoke methods */\
\
    typedef kern_return_t (*Create_Handler)(IOExtensiblePaniclog_Create_Args);\
    static kern_return_t\
    Create_Invoke(const IORPC rpc,\
        Create_Handler func);\
\
    typedef kern_return_t (*SetActive_Handler)(OSMetaClassBase * target);\
    static kern_return_t\
    SetActive_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetActive_Handler func);\
\
    typedef kern_return_t (*SetInactive_Handler)(OSMetaClassBase * target);\
    static kern_return_t\
    SetInactive_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetInactive_Handler func);\
\
    typedef kern_return_t (*InsertData_Handler)(OSMetaClassBase * target, IOExtensiblePaniclog_InsertData_Args);\
    static kern_return_t\
    InsertData_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        InsertData_Handler func);\
\
    typedef kern_return_t (*AppendData_Handler)(OSMetaClassBase * target, IOExtensiblePaniclog_AppendData_Args);\
    static kern_return_t\
    AppendData_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        AppendData_Handler func);\
\
    typedef kern_return_t (*CopyMemoryDescriptor_Handler)(OSMetaClassBase * target, IOExtensiblePaniclog_CopyMemoryDescriptor_Args);\
    static kern_return_t\
    CopyMemoryDescriptor_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        CopyMemoryDescriptor_Handler func);\
\
    typedef kern_return_t (*SetUsedLen_Handler)(OSMetaClassBase * target, IOExtensiblePaniclog_SetUsedLen_Args);\
    static kern_return_t\
    SetUsedLen_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetUsedLen_Handler func);\
\


#define IOExtensiblePaniclog_KernelMethods \
\
protected:\
    /* _Impl methods */\
\
    static kern_return_t\
    Create_Impl(IOExtensiblePaniclog_Create_Args);\
\
    kern_return_t\
    SetActive_Impl(IOExtensiblePaniclog_SetActive_Args);\
\
    kern_return_t\
    SetInactive_Impl(IOExtensiblePaniclog_SetInactive_Args);\
\
    kern_return_t\
    InsertData_Impl(IOExtensiblePaniclog_InsertData_Args);\
\
    kern_return_t\
    AppendData_Impl(IOExtensiblePaniclog_AppendData_Args);\
\
    kern_return_t\
    CopyMemoryDescriptor_Impl(IOExtensiblePaniclog_CopyMemoryDescriptor_Args);\
\
    kern_return_t\
    SetUsedLen_Impl(IOExtensiblePaniclog_SetUsedLen_Args);\
\


#define IOExtensiblePaniclog_VirtualMethods \
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
    virtual kern_return_t\
    ClaimBuffer(\
        uint64_t * addr,\
        uint64_t * len) APPLE_KEXT_OVERRIDE;\
\
    virtual kern_return_t\
    YieldBuffer(\
        uint32_t used_len) APPLE_KEXT_OVERRIDE;\
\


#if !KERNEL

class IOExtensiblePaniclogInterface : public OSInterface
{
public:
};

struct IOExtensiblePaniclog_IVars;
struct IOExtensiblePaniclog_LocalIVars;

class IOExtensiblePaniclog : public OSObject, public IOExtensiblePaniclogInterface
{
public:
    union
    {
        IOExtensiblePaniclog_IVars * ivars;
        IOExtensiblePaniclog_LocalIVars * lvars;
    };
    using super = OSObject;

    IOExtensiblePaniclog_Methods
    IOExtensiblePaniclog_VirtualMethods
};

#endif /* !KERNEL */

#endif /* !__DOCUMENTATION__ */



#endif /* _IOKIT_UIOEXTENSIBLEPANICLOG_H */
