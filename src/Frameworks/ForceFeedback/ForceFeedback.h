// ForceFeedback.h - force feedback devices behind IOKit HID services
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <stdint.h>

typedef int32_t HRESULT;
typedef struct __FFDHIDDEN *FFDeviceObjectReference;
typedef struct __FFEHIDDEN *FFEffectObjectReference;
typedef unsigned int io_service_t;

#define FF_OK ((HRESULT)0)
// the COM codes the API returns, E_NOINTERFACE and E_NOTIMPL
#define FFERR_NOINTERFACE ((HRESULT)0x80004002)
#define FFERR_UNSUPPORTED ((HRESULT)0x80004001)

HRESULT FFIsForceFeedback(io_service_t hidDevice);
HRESULT FFCreateDevice(io_service_t hidDevice, FFDeviceObjectReference *deviceReference);
HRESULT FFReleaseDevice(FFDeviceObjectReference deviceReference);
HRESULT FFDeviceGetForceFeedbackCapabilities(FFDeviceObjectReference device, void *capabilities);
HRESULT FFDeviceGetForceFeedbackProperty(FFDeviceObjectReference device, uint32_t property, void *value, uint32_t size);
HRESULT FFDeviceSetForceFeedbackProperty(FFDeviceObjectReference device, uint32_t property, void *value);
HRESULT FFDeviceSendForceFeedbackCommand(FFDeviceObjectReference device, uint32_t command);
HRESULT FFDeviceCreateEffect(FFDeviceObjectReference device, const void *uuid, void *effect, FFEffectObjectReference *effectReference);
HRESULT FFDeviceReleaseEffect(FFDeviceObjectReference device, FFEffectObjectReference effect);
HRESULT FFEffectSetParameters(FFEffectObjectReference effect, void *parameters, uint32_t flags);
HRESULT FFEffectStart(FFEffectObjectReference effect, uint32_t iterations, uint32_t flags);
HRESULT FFEffectStop(FFEffectObjectReference effect);
HRESULT FFEffectGetEffectStatus(FFEffectObjectReference effect, uint32_t *status);
