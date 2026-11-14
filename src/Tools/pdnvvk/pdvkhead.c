// pdvkhead.c - headless Vulkan on whatever driver the loader finds: devices, memory, a device and a buffer,
// no window system extensions. A Linux program, run under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CHECK(what, call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { printf("pdvkhead: %s: %d\n", what, r_); return 1; } } while (0)

int
main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	// "display": also VK_KHR_surface and VK_KHR_display, and a display query, as vulkaninfo makes
	int display = argc > 1 && !strcmp(argv[1], "display");
	int allext = argc > 1 && !strcmp(argv[1], "allext");
	const char *exts[64] = { "VK_KHR_surface", "VK_KHR_display" };
	VkExtensionProperties props[64];
	uint32_t next = 64, enabled = display ? 2 : 0;

	// "allext": every instance extension the loader offers, as vulkaninfo enables them
	if (allext && vkEnumerateInstanceExtensionProperties(NULL, &next, props) >= 0) {
		for (uint32_t i = 0; i < next; i++) {
			exts[enabled++] = props[i].extensionName;
			printf("pdvkhead: enabling %s\n", props[i].extensionName);
		}
	}
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "pdvkhead",
		.apiVersion = VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
		.enabledExtensionCount = enabled, .ppEnabledExtensionNames = exts };
	VkInstance instance;
	VkPhysicalDevice pds[8];
	uint32_t n = 8;

	CHECK("vkCreateInstance", vkCreateInstance(&ici, NULL, &instance));
	printf("pdvkhead: instance, %u extensions\n", enabled);
	CHECK("vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices(instance, &n, pds));
	printf("pdvkhead: %u physical devices\n", n);

	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceProperties p;
		VkPhysicalDeviceMemoryProperties m;

		vkGetPhysicalDeviceProperties(pds[i], &p);
		vkGetPhysicalDeviceMemoryProperties(pds[i], &m);
		printf("pdvkhead: %u: %s, Vulkan %u.%u.%u, driver %u.%u, vendor 0x%x device 0x%x\n", i, p.deviceName,
		    VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion),
		    (p.driverVersion >> 22) & 0x3ff, (p.driverVersion >> 14) & 0xff, p.vendorID, p.deviceID);
		for (uint32_t h = 0; h < m.memoryHeapCount; h++)
			printf("pdvkhead:    heap %u: %llu MB%s\n", h, (unsigned long long)(m.memoryHeaps[h].size >> 20),
			    m.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? ", device local" : "");
	}
	if (n == 0)
		return 1;
	if (display) {
		PFN_vkGetPhysicalDeviceDisplayPropertiesKHR get = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)
		    vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
		uint32_t nd = 0;
		VkResult r = get ? get(pds[0], &nd, NULL) : VK_ERROR_EXTENSION_NOT_PRESENT;

		printf("pdvkhead: display properties: %d, %u displays\n", r, nd);
		fflush(stdout);
	}

	// a device with one queue, and a buffer bound to host-visible memory, written and read back
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0,
		.queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci };
	VkDevice dev;
	CHECK("vkCreateDevice", vkCreateDevice(pds[0], &dci, NULL, &dev));
	printf("pdvkhead: device\n");

	VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 1 << 20,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT };
	VkBuffer buf;
	VkMemoryRequirements req;
	VkPhysicalDeviceMemoryProperties mp;
	uint32_t type = UINT32_MAX;
	CHECK("vkCreateBuffer", vkCreateBuffer(dev, &bci, NULL, &buf));
	vkGetBufferMemoryRequirements(dev, buf, &req);
	vkGetPhysicalDeviceMemoryProperties(pds[0], &mp);
	for (uint32_t t = 0; t < mp.memoryTypeCount && type == UINT32_MAX; t++) {
		VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		if ((req.memoryTypeBits & (1u << t)) && (mp.memoryTypes[t].propertyFlags & want) == want)
			type = t;
	}
	if (type == UINT32_MAX) {
		printf("pdvkhead: no host-visible memory type\n");
		return 1;
	}
	VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size,
		.memoryTypeIndex = type };
	VkDeviceMemory mem;
	void *p;
	CHECK("vkAllocateMemory", vkAllocateMemory(dev, &mai, NULL, &mem));
	CHECK("vkBindBufferMemory", vkBindBufferMemory(dev, buf, mem, 0));
	CHECK("vkMapMemory", vkMapMemory(dev, mem, 0, req.size, 0, &p));
	unsigned bad = 0;
	for (uint32_t i = 0; i < (1 << 20) / 4; i++)
		((volatile uint32_t *)p)[i] = 0x5eed0000u ^ i;
	for (uint32_t i = 0; i < (1 << 20) / 4; i++)
		bad += ((volatile uint32_t *)p)[i] != (0x5eed0000u ^ i);
	printf("pdvkhead: 1 MB buffer in memory type %u mapped, %u words wrong\n", type, bad);

	vkUnmapMemory(dev, mem);
	vkFreeMemory(dev, mem, NULL);
	vkDestroyBuffer(dev, buf, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(instance, NULL);
	printf("pdvkhead: done\n");
	return bad != 0;
}
