/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2016 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <mach/shared_region.h>
#include <mach/mach_vm.h>
#include <libkern/OSAtomic.h>
#include <execinfo.h>
#include <mach-o/dyld_priv.h>
#include <mach-o/dyld_process_info.h>
#include <mach-o/dyld_images.h>
#include <Block.h>
#include <dlfcn.h>

#include "dyld_process_info_internal.h"

#include "Loading.h"
#include "Tracing.h"
#include "AllImages.h"

extern "C" int _dyld_func_lookup(const char* name, void** address);

typedef void (^Notify)(bool unload, uint64_t timestamp, uint64_t machHeader, const uuid_t uuid, const char* path);
typedef void (^NotifyExit)();
typedef void (^NotifyMain)();

//
// Object used for monitoring another processes dyld loads
//
struct __attribute__((visibility("hidden"))) dyld_process_info_notify_base
{
                        dyld_process_info_notify_base(dispatch_queue_t queue, Notify notify, NotifyExit notifyExit, task_t task, kern_return_t* kr);
                        ~dyld_process_info_notify_base();
    bool                enabled() const;
    void                retain();
    void                release();

    void                setNotifyMain(NotifyMain notifyMain) const {
        if (_notifyMain == notifyMain) { return; }
        Block_release(_notifyMain);
        _notifyMain = Block_copy(notifyMain);
    }

    // override new and delete so we don't need to link with libc++
    static void*        operator new(size_t sz) { return malloc(sz); }
    static void         operator delete(void* p) { free(p); }

private:
    void                handleEvent();
    void                disconnect();
    void                teardownMachPorts();
    void                replyToMonitoredProcess(mach_msg_header_t& header);

    kern_return_t       task_dyld_process_info_notify_register(task_read_t target_task, mach_port_t notify);
    kern_return_t       task_dyld_process_info_notify_deregister(task_read_t target_task, mach_port_t notify);

    RemoteBuffer                    _remoteAllImageInfoBuffer;
    mutable std::atomic<uint32_t>   _retainCount;
    dispatch_queue_t                _queue;
    mutable Notify                  _notify;
    mutable NotifyExit              _notifyExit;
    mutable NotifyMain              _notifyMain;
    dispatch_source_t               _machSource;
    task_t                          _task;
    mach_port_t                     _port;      // monitor is process being notified of image loading/unloading
    std::atomic<bool>               _connected;
#if TARGET_OS_SIMULATOR
    uint32_t                        _portInTarget;
#endif
};

#if TARGET_OS_SIMULATOR

template<typename F>
kern_return_t withRemotePortArray(task_t target_task, F f) {
    // Get the all image info
    task_dyld_info_data_t taskDyldInfo;
    mach_msg_type_number_t taskDyldInfoCount = TASK_DYLD_INFO_COUNT;
    auto kr = task_info(target_task, TASK_DYLD_INFO, (task_info_t)&taskDyldInfo, &taskDyldInfoCount);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    vm_prot_t cur_protection = VM_PROT_NONE;
    vm_prot_t max_protection = VM_PROT_NONE;
    mach_vm_address_t localAddress = 0;
    mach_vm_size_t size = sizeof(dyld_all_image_infos_64);
    if ( taskDyldInfo.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_32 ) {
        size = sizeof(dyld_all_image_infos_32);
    }
    kr = mach_vm_remap(mach_task_self(),
                        &localAddress,
                       size,
                        0,  // mask
                        VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR| VM_FLAGS_RESILIENT_CODESIGN | VM_FLAGS_RESILIENT_MEDIA,
                        target_task,
                        taskDyldInfo.all_image_info_addr,
                        false,
                        &cur_protection,
                        &max_protection,
                        VM_INHERIT_NONE);

    static_assert(sizeof(mach_port_t) == sizeof(uint32_t), "machport size not 32-bits");
    uint32_t* notifyMachPorts;
    if ( taskDyldInfo.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_32 ) {
        notifyMachPorts = (uint32_t *)((uint8_t *)localAddress + offsetof(dyld_all_image_infos_32,notifyMachPorts));
    } else {
        notifyMachPorts = (uint32_t *)((uint8_t *)localAddress + offsetof(dyld_all_image_infos_64,notifyMachPorts));
    }
    kr = f(notifyMachPorts);
    (void)vm_deallocate(target_task, localAddress, size);
    return kr;
}

#endif

kern_return_t dyld_process_info_notify_base::task_dyld_process_info_notify_register(task_t target_task, mach_port_t notify) {
#if TARGET_OS_SIMULATOR
    static dispatch_once_t onceToken;
    static kern_return_t (*tdpinr)(task_t, mach_port_t) = nullptr;
    dispatch_once(&onceToken, ^{
        tdpinr = (kern_return_t (*)(task_t, mach_port_t))dlsym(RTLD_DEFAULT, "task_dyld_process_info_notify_register");
    });
    if (tdpinr) {
        return tdpinr(target_task, notify);
    }
    // Our libsystem does not have task_dyld_process_info_notify_register, emulate
    return withRemotePortArray(target_task, [this,target_task,notify](uint32_t* portArray){
        mach_port_t portInTarget = MACH_PORT_NULL;
        // Insert the right
        kern_return_t kr = KERN_NAME_EXISTS;
        while (kr == KERN_NAME_EXISTS) {
            portInTarget = MACH_PORT_NULL;
            kr = mach_port_allocate(target_task, MACH_PORT_RIGHT_DEAD_NAME, &portInTarget);
            if (kr != KERN_SUCCESS) {
                return kr;
            }
            (void)mach_port_deallocate(target_task, portInTarget);
            kr = mach_port_insert_right(target_task, portInTarget, notify, MACH_MSG_TYPE_MAKE_SEND);
        }
        // The call is not succesfull return
        if (kr != KERN_SUCCESS) {
            (void)mach_port_deallocate(target_task, portInTarget);
            return kr;
        }
        // Find a slot for the right
        for (uint8_t notifySlot=0; notifySlot < DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT; ++notifySlot) {
            if (OSAtomicCompareAndSwap32(0, portInTarget, (volatile int32_t*)&portArray[notifySlot])) {
                _portInTarget = portInTarget;
                return KERN_SUCCESS;
            }
        }
        // The array was full, we need to fail
        (void)mach_port_deallocate(target_task, portInTarget);
        return KERN_UREFS_OVERFLOW;
    });
#else
    return ::task_dyld_process_info_notify_register(target_task, notify);
#endif
}

kern_return_t dyld_process_info_notify_base::task_dyld_process_info_notify_deregister(task_t target_task, mach_port_t notify) {
#if TARGET_OS_SIMULATOR
    static dispatch_once_t onceToken;
    static kern_return_t (*tdpind)(task_t, mach_port_t) = nullptr;
    dispatch_once(&onceToken, ^{
        tdpind = (kern_return_t (*)(task_t, mach_port_t))dlsym(RTLD_DEFAULT, "task_dyld_process_info_notify_deregister");
    });
    if (tdpind) {
        return tdpind(target_task, notify);
    }
    // Our libsystem does not have task_dyld_process_info_notify_deregister, emulate
    return withRemotePortArray(target_task, [this](uint32_t* portArray){
        // Find a slot for the right
        for (uint8_t notifySlot=0; notifySlot < DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT; ++notifySlot) {
            if (OSAtomicCompareAndSwap32(0, _portInTarget, (volatile int32_t*)&portArray[notifySlot])) {
                return KERN_SUCCESS;
            }
        }
        return KERN_FAILURE;
    });
#else
    // Our libsystem does not have task_dyld_process_info_notify_deregister, emulate
    return ::task_dyld_process_info_notify_deregister(target_task, notify);
#endif
}

dyld_process_info_notify_base::dyld_process_info_notify_base(dispatch_queue_t queue, Notify notify, NotifyExit notifyExit,
                                                             task_t task, kern_return_t* kr) :
        _retainCount(0), _queue(queue), _notify(Block_copy(notify)), _notifyExit(Block_copy(notifyExit)),
        _notifyMain(nullptr), _machSource(nullptr), _task(task), _port(MACH_PORT_NULL), _connected(false)
#if TARGET_OS_SIMULATOR
        , _portInTarget(0)
#endif
{
    assert(kr != NULL);
    dispatch_retain(_queue);
    // Allocate a port to listen on in this monitoring task
    mach_port_options_t options = { .flags = MPO_IMPORTANCE_RECEIVER | MPO_CONTEXT_AS_GUARD | MPO_STRICT, .mpl = { MACH_PORT_QLIMIT_DEFAULT }};
    *kr = mach_port_construct(mach_task_self(), &options, (mach_port_context_t)this, &_port);
    if (*kr != KERN_SUCCESS) {
        teardownMachPorts();
        return;
    }

    mach_port_t previous = MACH_PORT_NULL;
    *kr = mach_port_request_notification(mach_task_self(), _port, MACH_NOTIFY_NO_SENDERS, 1, _port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    if ((*kr != KERN_SUCCESS) || previous != MACH_PORT_NULL) {
        teardownMachPorts();
        return;
    }
    //FIXME: Should we retry here if we fail?
    *kr = task_dyld_process_info_notify_register(_task, _port);
    dyld3::kdebug_trace_dyld_marker(DBG_DYLD_TASK_NOTIFY_REGISTER, (uint64_t)_task, (uint64_t)_port, *kr, 0);
    if (*kr != KERN_SUCCESS) {
        teardownMachPorts();
        return;
    }

    // Setup the event handler for the port
    _machSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, _port, 0, _queue);
    if (_machSource == nullptr) {
        teardownMachPorts();
        return;
    }
    dispatch_source_set_event_handler(_machSource, ^{ handleEvent(); });
    dispatch_source_set_cancel_handler(_machSource, ^{ teardownMachPorts(); });
    dispatch_activate(_machSource);
    _connected = true;
}

dyld_process_info_notify_base::~dyld_process_info_notify_base() {
    if (_connected) { fprintf(stderr, "dyld: ~dyld_process_info_notify_base called while still connected\n"); }
    Block_release(_notify);
    Block_release(_notifyMain);
    Block_release(_notifyExit);
    dispatch_release(_queue);
}

void dyld_process_info_notify_base::teardownMachPorts() {
    if ( _port != 0 ) {
        kern_return_t kr = task_dyld_process_info_notify_deregister(_task, _port);
        dyld3::kdebug_trace_dyld_marker(DBG_DYLD_TASK_NOTIFY_DEREGISTER, (uint64_t)_task, (uint64_t)_port, kr, 0);
        (void)mach_port_destruct(mach_task_self(), _port, 0, (mach_port_context_t)this);
        _port = 0;
    }
}

void dyld_process_info_notify_base::disconnect() {
    if (_connected) {
        _connected = false;
        // The connection to the target is dead.  Clean up ports
        if ( _machSource ) {
            dispatch_source_cancel(_machSource);
            dispatch_release(_machSource);
            _machSource = NULL;
        } 
        if (_notifyExit) {
            dispatch_async(_queue, ^{
                // There was a not a mach source, so if we have any ports they will not get torn down by its cancel handler
                _notifyExit();
            });
        }
    }
}

bool dyld_process_info_notify_base::enabled() const
{
    return _connected;
}

void dyld_process_info_notify_base::retain()
{
    _retainCount.fetch_add(1, std::memory_order_relaxed);
}

void dyld_process_info_notify_base::release()
{
    if (_retainCount.fetch_sub(1, std::memory_order_acq_rel) == 0) {
        // When we subtracted the ref count was 0, which means it was the last reference
        disconnect();
        dispatch_async(_queue, ^{
            delete this;
        });
    }
}

void dyld_process_info_notify_base::replyToMonitoredProcess(mach_msg_header_t& header) {
    mach_msg_header_t replyHeader;
    replyHeader.msgh_bits        = MACH_MSGH_BITS_SET(MACH_MSGH_BITS_REMOTE(header.msgh_bits), 0, 0, 0);
    replyHeader.msgh_id          = 0;
    replyHeader.msgh_local_port  = MACH_PORT_NULL;
    replyHeader.msgh_remote_port  = header.msgh_remote_port;
    replyHeader.msgh_reserved    = 0;
    replyHeader.msgh_size        = sizeof(replyHeader);
    kern_return_t r = mach_msg(&replyHeader, MACH_SEND_MSG, replyHeader.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
    if (r == KERN_SUCCESS) {
        header.msgh_remote_port = MACH_PORT_NULL;
    } else {
        disconnect();
    }
}

void dyld_process_info_notify_base::handleEvent() {
    // References object may still exist even after the ports are dead. Disable event dispatching
    // if the ports have been torn down.
    if (!_connected) { return; }

    // This event handler block has an implicit reference to "this"
    // if incrementing the count goes to one, that means the object may have already been destroyed
    uint8_t messageBuffer[DYLD_PROCESS_INFO_NOTIFY_MAX_BUFFER_SIZE] = {};
    mach_msg_header_t* h = (mach_msg_header_t*)messageBuffer;

    kern_return_t r = mach_msg(h, MACH_RCV_MSG | MACH_RCV_VOUCHER| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0), 0, sizeof(messageBuffer)-sizeof(mach_msg_audit_trailer_t), _port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if ( r == KERN_SUCCESS && !(h->msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
        //fprintf(stderr, "received message id=0x%X, size=%d\n", h->msgh_id, h->msgh_size);

        if ( h->msgh_id == DYLD_PROCESS_INFO_NOTIFY_LOAD_ID || h->msgh_id == DYLD_PROCESS_INFO_NOTIFY_UNLOAD_ID ) {
            // run notifier block for each [un]load image
            const dyld_process_info_notify_header* header = (dyld_process_info_notify_header*)messageBuffer;
            if (sizeof(*header) <= h->msgh_size
                && header->imagesOffset <= h->msgh_size
                && header->stringsOffset <= h->msgh_size
                && (header->imageCount * sizeof(dyld_process_info_image_entry)) <= (h->msgh_size - header->imagesOffset)) {
                const dyld_process_info_image_entry* entries = (dyld_process_info_image_entry*)&messageBuffer[header->imagesOffset];
                const char* const stringPool = (char*)&messageBuffer[header->stringsOffset];
                for (unsigned i=0; i < header->imageCount; ++i) {
                    bool isUnload = (h->msgh_id == DYLD_PROCESS_INFO_NOTIFY_UNLOAD_ID);
                    if (entries[i].pathStringOffset <= h->msgh_size - header->stringsOffset) {
                        //fprintf(stderr, "Notifying about: %s\n", stringPool + entries[i].pathStringOffset);
                        _notify(isUnload, header->timestamp, entries[i].loadAddress, entries[i].uuid, stringPool + entries[i].pathStringOffset);
                    } else {
                        disconnect();
                        break;
                    }
                }
                // reply to dyld, so it can continue
                replyToMonitoredProcess(*h);
            } else {
                disconnect();
            }
        }
        else if ( h->msgh_id == DYLD_PROCESS_INFO_NOTIFY_MAIN_ID ) {
            if (h->msgh_size != sizeof(mach_msg_header_t)) {
                disconnect();
            } else if ( _notifyMain != NULL )  {
                _notifyMain();
            }
            replyToMonitoredProcess(*h);
        } else if ( h->msgh_id == MACH_NOTIFY_NO_SENDERS ) {
            // Validate this notification came from the kernel
            const mach_msg_audit_trailer_t *audit_tlr = (mach_msg_audit_trailer_t *)((uint8_t *)h + round_msg(h->msgh_size));
            if (audit_tlr->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0
                && audit_tlr->msgh_trailer_size >= sizeof(mach_msg_audit_trailer_t)
                // We cannot link to libbsm, so we are hardcoding the audit token offset (5)
                // And the value the represents the kernel (0)
                && audit_tlr->msgh_audit.val[5] == 0) {
                disconnect();
            }
        }
        else {
            fprintf(stderr, "dyld: received unknown message id=0x%X, size=%d\n", h->msgh_id, h->msgh_size);
        }
    } else {
        fprintf(stderr, "dyld: received unknown message id=0x%X, size=%d\n", h->msgh_id, h->msgh_size);
    }
    mach_msg_destroy(h);
}

dyld_process_info_notify _dyld_process_info_notify(task_t task, dispatch_queue_t queue,
                                                   void (^notify)(bool unload, uint64_t timestamp, uint64_t machHeader, const uuid_t uuid, const char* path),
                                                   void (^notifyExit)(),
                                                   kern_return_t* kr)
{
    kern_return_t krSink = KERN_SUCCESS;
    if (kr == nullptr) {
        kr = &krSink;
    }
    *kr = KERN_SUCCESS;
    
    dyld_process_info_notify result = new dyld_process_info_notify_base(queue, notify, notifyExit, task, kr);
    if (result->enabled())
        return result;
    const_cast<dyld_process_info_notify_base*>(result)->release();
    return nullptr;
}

void _dyld_process_info_notify_main(dyld_process_info_notify object, void (^notifyMain)())
{
	object->setNotifyMain(notifyMain);
}

void _dyld_process_info_notify_retain(dyld_process_info_notify object)
{
    const_cast<dyld_process_info_notify_base*>(object)->retain();
}

void _dyld_process_info_notify_release(dyld_process_info_notify object)
{
    const_cast<dyld_process_info_notify_base*>(object)->release();
}

static void (*sNotifyMonitoringDyldMain)() = nullptr;
static void (*sNotifyMonitoringDyld)(bool unloading, unsigned imageCount, const struct mach_header* loadAddresses[],
                                     const char* imagePaths[]) = nullptr;

void setNotifyMonitoringDyldMain(void (*func)())
{
    sNotifyMonitoringDyldMain = func;
}

void setNotifyMonitoringDyld(void (*func)(bool unloading, unsigned imageCount,
                                          const struct mach_header* loadAddresses[],
                                          const char* imagePaths[]))
{
    sNotifyMonitoringDyld = func;
}

namespace dyld3 {

void AllImages::notifyMonitorMain()
{
#if !TARGET_OS_DRIVERKIT
    assert(sNotifyMonitoringDyldMain != nullptr);
    sNotifyMonitoringDyldMain();
#endif
}

void AllImages::notifyMonitorLoads(const Array<LoadedImage>& newImages)
{
#if !TARGET_OS_DRIVERKIT
    assert(sNotifyMonitoringDyld != nullptr);
    const struct mach_header* loadAddresses[newImages.count()];
    const char* loadPaths[newImages.count()];
    for(uint32_t i = 0; i<newImages.count(); ++i) {
        loadAddresses[i] = newImages[i].loadedAddress();
        loadPaths[i] = newImages[i].image()->path();
    }
    sNotifyMonitoringDyld(false, (unsigned)newImages.count(), loadAddresses, loadPaths);
#endif
}

void AllImages::notifyMonitorUnloads(const Array<LoadedImage>& unloadingImages)
{
#if !TARGET_OS_DRIVERKIT
    assert(sNotifyMonitoringDyld != nullptr);
    const struct mach_header* loadAddresses[unloadingImages.count()];
    const char* loadPaths[unloadingImages.count()];
    for(uint32_t i = 0; i<unloadingImages.count(); ++i) {
        loadAddresses[i] = unloadingImages[i].loadedAddress();
        loadPaths[i] = unloadingImages[i].image()->path();
    }
    sNotifyMonitoringDyld(true, (unsigned)unloadingImages.count(), loadAddresses, loadPaths);
#endif
}

} // namespace dyld3




