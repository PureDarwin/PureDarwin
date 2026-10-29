// ForceFeedback.c - force feedback with no force feedback drivers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// no HID service has a force feedback plugin, so every device answers that it has none
#include "ForceFeedback.h"

HRESULT FFIsForceFeedback(io_service_t hidDevice) { return FFERR_NOINTERFACE; }

HRESULT FFCreateDevice(io_service_t hidDevice, FFDeviceObjectReference *deviceReference)
{
    if (deviceReference)
        *deviceReference = 0;
    return FFERR_NOINTERFACE;
}

HRESULT FFReleaseDevice(FFDeviceObjectReference deviceReference) { return FF_OK; }
HRESULT FFDeviceGetForceFeedbackCapabilities(FFDeviceObjectReference device, void *capabilities) { return FFERR_UNSUPPORTED; }
HRESULT FFDeviceGetForceFeedbackProperty(FFDeviceObjectReference device, uint32_t property, void *value, uint32_t size) { return FFERR_UNSUPPORTED; }
HRESULT FFDeviceSetForceFeedbackProperty(FFDeviceObjectReference device, uint32_t property, void *value) { return FFERR_UNSUPPORTED; }
HRESULT FFDeviceSendForceFeedbackCommand(FFDeviceObjectReference device, uint32_t command) { return FFERR_UNSUPPORTED; }

HRESULT FFDeviceCreateEffect(FFDeviceObjectReference device, const void *uuid, void *effect, FFEffectObjectReference *effectReference)
{
    if (effectReference)
        *effectReference = 0;
    return FFERR_UNSUPPORTED;
}

HRESULT FFDeviceReleaseEffect(FFDeviceObjectReference device, FFEffectObjectReference effect) { return FF_OK; }
HRESULT FFEffectSetParameters(FFEffectObjectReference effect, void *parameters, uint32_t flags) { return FFERR_UNSUPPORTED; }
HRESULT FFEffectStart(FFEffectObjectReference effect, uint32_t iterations, uint32_t flags) { return FFERR_UNSUPPORTED; }
HRESULT FFEffectStop(FFEffectObjectReference effect) { return FFERR_UNSUPPORTED; }

HRESULT FFEffectGetEffectStatus(FFEffectObjectReference effect, uint32_t *status)
{
    if (status)
        *status = 0;
    return FFERR_UNSUPPORTED;
}
