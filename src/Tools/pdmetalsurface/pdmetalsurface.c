// pdmetalsurface.c - VK_EXT_metal_surface as an implicit layer, a CAMetalLayer's Wayland subsurface handed to the driver below as a VK_KHR_wayland_surface.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#define LAYER_NAME "VK_LAYER_PD_metal_surface"
#define MAX_INSTANCES 64
#define MAX_DEVICES 64

struct instance_entry {
	VkInstance instance;
	PFN_vkGetInstanceProcAddr gipa;
	int metal_surface;
};

struct device_entry {
	VkDevice device;
	PFN_vkGetDeviceProcAddr gdpa;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct instance_entry instances[MAX_INSTANCES];
static struct device_entry devices[MAX_DEVICES];

static struct instance_entry *
find_instance(VkInstance instance)
{
	struct instance_entry *e = NULL;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_INSTANCES; i++) {
		if (instances[i].instance == instance) {
			e = &instances[i];
			break;
		}
	}
	pthread_mutex_unlock(&lock);
	return e;
}

// a physical device's instance is not known from the handle: any instance's chain resolves the same entry points
static PFN_vkGetInstanceProcAddr
any_gipa(void)
{
	PFN_vkGetInstanceProcAddr gipa = NULL;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_INSTANCES && !gipa; i++)
		gipa = instances[i].gipa;
	pthread_mutex_unlock(&lock);
	return gipa;
}

static int
has_extension(const char *const *names, uint32_t count, const char *name)
{
	for (uint32_t i = 0; i < count; i++) {
		if (!strcmp(names[i], name))
			return 1;
	}
	return 0;
}

static VKAPI_ATTR VkResult VKAPI_CALL
pd_vkCreateInstance(const VkInstanceCreateInfo *info, const VkAllocationCallbacks *alloc, VkInstance *instance)
{
	VkLayerInstanceCreateInfo *chain = (VkLayerInstanceCreateInfo *)info->pNext;
	PFN_vkGetInstanceProcAddr gipa;
	PFN_vkCreateInstance create;
	VkInstanceCreateInfo next_info = *info;
	const char **names = NULL;
	uint32_t count = 0;
	int metal;
	VkResult res;

	while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO))
		chain = (VkLayerInstanceCreateInfo *)chain->pNext;
	if (!chain)
		return VK_ERROR_INITIALIZATION_FAILED;
	gipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
	if (!(create = (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance")))
		return VK_ERROR_INITIALIZATION_FAILED;

	// the driver below sees wayland surfaces where the app asked for metal ones
	metal = has_extension(info->ppEnabledExtensionNames, info->enabledExtensionCount, VK_EXT_METAL_SURFACE_EXTENSION_NAME);
	if (metal) {
		if (!(names = calloc(info->enabledExtensionCount + 2, sizeof(*names))))
			return VK_ERROR_OUT_OF_HOST_MEMORY;
		for (uint32_t i = 0; i < info->enabledExtensionCount; i++) {
			if (strcmp(info->ppEnabledExtensionNames[i], VK_EXT_METAL_SURFACE_EXTENSION_NAME))
				names[count++] = info->ppEnabledExtensionNames[i];
		}
		if (!has_extension(names, count, VK_KHR_SURFACE_EXTENSION_NAME))
			names[count++] = VK_KHR_SURFACE_EXTENSION_NAME;
		names[count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
		next_info.ppEnabledExtensionNames = names;
		next_info.enabledExtensionCount = count;
	}

	res = create(&next_info, alloc, instance);
	free(names);
	if (res != VK_SUCCESS)
		return res;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_INSTANCES; i++) {
		if (!instances[i].instance) {
			instances[i].instance = *instance;
			instances[i].gipa = gipa;
			instances[i].metal_surface = metal;
			break;
		}
	}
	pthread_mutex_unlock(&lock);
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL
pd_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *alloc)
{
	struct instance_entry *e = find_instance(instance);
	PFN_vkDestroyInstance destroy;

	if (!e)
		return;
	destroy = (PFN_vkDestroyInstance)e->gipa(instance, "vkDestroyInstance");
	pthread_mutex_lock(&lock);
	e->instance = VK_NULL_HANDLE;
	e->gipa = NULL;
	pthread_mutex_unlock(&lock);
	if (destroy)
		destroy(instance, alloc);
}

// QuartzCore's -[CAMetalLayer _pdWaylandSurface:], reached through the runtime so this layer needs no objc linkage
static void *
layer_wayland_surface(const void *layer, void **display)
{
	static void *(*msg)(const void *, void *, void **);
	static void *sel;

	if (!msg) {
		void *(*reg)(const char *) = dlsym(RTLD_DEFAULT, "sel_registerName");

		msg = dlsym(RTLD_DEFAULT, "objc_msgSend");
		sel = reg ? reg("_pdWaylandSurface:") : NULL;
	}
	if (!msg || !sel || !layer)
		return NULL;
	return msg(layer, sel, display);
}

static VKAPI_ATTR VkResult VKAPI_CALL
pd_vkCreateMetalSurfaceEXT(VkInstance instance, const VkMetalSurfaceCreateInfoEXT *info,
    const VkAllocationCallbacks *alloc, VkSurfaceKHR *surface)
{
	struct instance_entry *e = find_instance(instance);
	VkWaylandSurfaceCreateInfoKHR wl = { .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
	PFN_vkCreateWaylandSurfaceKHR create;
	void *display = NULL;

	if (!e)
		return VK_ERROR_INITIALIZATION_FAILED;
	if (!(wl.surface = layer_wayland_surface(info->pLayer, &display)) || !display) {
		fprintf(stderr, LAYER_NAME ": layer %p has no Wayland subsurface (not in a window?)\n", (void *)info->pLayer);
		return VK_ERROR_SURFACE_LOST_KHR;
	}
	wl.display = display;
	if (!(create = (PFN_vkCreateWaylandSurfaceKHR)e->gipa(instance, "vkCreateWaylandSurfaceKHR")))
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	return create(instance, &wl, alloc, surface);
}

static VKAPI_ATTR VkResult VKAPI_CALL
pd_vkCreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo *info, const VkAllocationCallbacks *alloc,
    VkDevice *device)
{
	VkLayerDeviceCreateInfo *chain = (VkLayerDeviceCreateInfo *)info->pNext;
	PFN_vkGetInstanceProcAddr gipa;
	PFN_vkGetDeviceProcAddr gdpa;
	PFN_vkCreateDevice create;
	VkResult res;

	while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO))
		chain = (VkLayerDeviceCreateInfo *)chain->pNext;
	if (!chain)
		return VK_ERROR_INITIALIZATION_FAILED;
	gipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	gdpa = chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
	if (!(create = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice")))
		return VK_ERROR_INITIALIZATION_FAILED;
	if ((res = create(physical, info, alloc, device)) != VK_SUCCESS)
		return res;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (!devices[i].device) {
			devices[i].device = *device;
			devices[i].gdpa = gdpa;
			break;
		}
	}
	pthread_mutex_unlock(&lock);
	return VK_SUCCESS;
}

static PFN_vkGetDeviceProcAddr
find_gdpa(VkDevice device)
{
	PFN_vkGetDeviceProcAddr gdpa = NULL;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (devices[i].device == device) {
			gdpa = devices[i].gdpa;
			break;
		}
	}
	pthread_mutex_unlock(&lock);
	return gdpa;
}

static VKAPI_ATTR void VKAPI_CALL
pd_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *alloc)
{
	PFN_vkGetDeviceProcAddr gdpa = find_gdpa(device);
	PFN_vkDestroyDevice destroy = gdpa ? (PFN_vkDestroyDevice)gdpa(device, "vkDestroyDevice") : NULL;

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (devices[i].device == device)
			devices[i].device = VK_NULL_HANDLE;
	}
	pthread_mutex_unlock(&lock);
	if (destroy)
		destroy(device, alloc);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL pd_vkGetDeviceProcAddr(VkDevice device, const char *name);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
pd_vkGetDeviceProcAddr(VkDevice device, const char *name)
{
	PFN_vkGetDeviceProcAddr gdpa;

	if (!strcmp(name, "vkGetDeviceProcAddr"))
		return (PFN_vkVoidFunction)pd_vkGetDeviceProcAddr;
	if (!strcmp(name, "vkDestroyDevice"))
		return (PFN_vkVoidFunction)pd_vkDestroyDevice;
	gdpa = find_gdpa(device);
	return gdpa ? gdpa(device, name) : NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL pd_vkGetInstanceProcAddr(VkInstance instance, const char *name);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
pd_vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
	struct instance_entry *e;

	if (!strcmp(name, "vkGetInstanceProcAddr"))
		return (PFN_vkVoidFunction)pd_vkGetInstanceProcAddr;
	if (!strcmp(name, "vkCreateInstance"))
		return (PFN_vkVoidFunction)pd_vkCreateInstance;
	if (!strcmp(name, "vkDestroyInstance"))
		return (PFN_vkVoidFunction)pd_vkDestroyInstance;
	if (!strcmp(name, "vkCreateDevice"))
		return (PFN_vkVoidFunction)pd_vkCreateDevice;
	if (!strcmp(name, "vkGetDeviceProcAddr"))
		return (PFN_vkVoidFunction)pd_vkGetDeviceProcAddr;
	if (!strcmp(name, "vkDestroyDevice"))
		return (PFN_vkVoidFunction)pd_vkDestroyDevice;
	if (!strcmp(name, "vkCreateMetalSurfaceEXT"))
		return (PFN_vkVoidFunction)pd_vkCreateMetalSurfaceEXT;
	if (!instance) {
		PFN_vkGetInstanceProcAddr gipa = any_gipa();
		return gipa ? gipa(instance, name) : NULL;
	}
	e = find_instance(instance);
	return e ? e->gipa(instance, name) : NULL;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *iface)
{
	if (!iface || iface->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
		return VK_ERROR_INITIALIZATION_FAILED;
	if (iface->loaderLayerInterfaceVersion > 2)
		iface->loaderLayerInterfaceVersion = 2;
	iface->pfnGetInstanceProcAddr = pd_vkGetInstanceProcAddr;
	iface->pfnGetDeviceProcAddr = pd_vkGetDeviceProcAddr;
	iface->pfnGetPhysicalDeviceProcAddr = NULL;
	return VK_SUCCESS;
}
