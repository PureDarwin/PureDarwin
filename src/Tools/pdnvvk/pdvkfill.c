// pdvkfill.c - does the GPU run work a process submits: a buffer filled on the GPU with vkCmdFillBuffer, a fence
// waited on, the buffer read back. A Linux program, run under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// The answer splits three ways: the fence signals and the words are there (the GPU runs submitted work), the
// words are there but the fence never signals (the work runs, its completion is not seen), or neither (the work
// never reaches the GPU or is never scheduled).
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#define CHECK(what, call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { printf("pdvkfill: %s: %d\n", what, r_); return 1; } } while (0)

#define WORDS (1024 * 1024 / 4)
#define PATTERN 0xc0ffee42u

static double
now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

int
main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "pdvkfill",
		.apiVersion = VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
	VkInstance instance;
	VkPhysicalDevice pd;
	uint32_t n = 1;

	CHECK("vkCreateInstance", vkCreateInstance(&ici, NULL, &instance));
	if (vkEnumeratePhysicalDevices(instance, &n, &pd) < 0 || n == 0) {
		printf("pdvkfill: no physical device\n");
		return 1;
	}
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(pd, &props);
	printf("pdvkfill: %s\n", props.deviceName);

	// the first queue family that can transfer (graphics and compute families can too)
	VkQueueFamilyProperties qf[16];
	uint32_t nq = 16, family = UINT32_MAX;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf);
	for (uint32_t i = 0; i < nq && family == UINT32_MAX; i++)
		if (qf[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
			family = i;
	if (family == UINT32_MAX) {
		printf("pdvkfill: no queue family that can transfer\n");
		return 1;
	}
	printf("pdvkfill: queue family %u, flags 0x%x\n", family, qf[family].queueFlags);

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = family,
		.queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci };
	VkDevice dev;
	VkQueue queue;
	CHECK("vkCreateDevice", vkCreateDevice(pd, &dci, NULL, &dev));
	vkGetDeviceQueue(dev, family, 0, &queue);

	// a host-visible, coherent buffer, so what the GPU writes is read straight back
	VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = WORDS * 4,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
	VkBuffer buf;
	CHECK("vkCreateBuffer", vkCreateBuffer(dev, &bci, NULL, &buf));
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	uint32_t type = UINT32_MAX;
	VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	for (uint32_t i = 0; i < mp.memoryTypeCount && type == UINT32_MAX; i++)
		if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
			type = i;
	if (type == UINT32_MAX) {
		printf("pdvkfill: no host-visible coherent memory\n");
		return 1;
	}
	VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size,
		.memoryTypeIndex = type };
	VkDeviceMemory mem;
	uint32_t *words;
	CHECK("vkAllocateMemory", vkAllocateMemory(dev, &mai, NULL, &mem));
	CHECK("vkBindBufferMemory", vkBindBufferMemory(dev, buf, mem, 0));
	CHECK("vkMapMemory", vkMapMemory(dev, mem, 0, WORDS * 4, 0, (void **)&words));
	memset(words, 0, WORDS * 4);
	printf("pdvkfill: memory type %u (flags 0x%x), cleared\n", type, mp.memoryTypes[type].propertyFlags);

	VkCommandPoolCreateInfo pci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = family };
	VkCommandPool pool;
	CHECK("vkCreateCommandPool", vkCreateCommandPool(dev, &pci, NULL, &pool));
	VkCommandBufferAllocateInfo cai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
	VkCommandBuffer cb;
	CHECK("vkAllocateCommandBuffers", vkAllocateCommandBuffers(dev, &cai, &cb));
	VkCommandBufferBeginInfo cbi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	CHECK("vkBeginCommandBuffer", vkBeginCommandBuffer(cb, &cbi));
	vkCmdFillBuffer(cb, buf, 0, WORDS * 4, PATTERN);
	// the fill visible to the host once the fence signals
	VkMemoryBarrier mb = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_HOST_READ_BIT };
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
	CHECK("vkEndCommandBuffer", vkEndCommandBuffer(cb));

	VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	CHECK("vkCreateFence", vkCreateFence(dev, &fci, NULL, &fence));
	VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
	double t0 = now();
	CHECK("vkQueueSubmit", vkQueueSubmit(queue, 1, &si, fence));
	printf("pdvkfill: submitted a 1 MB fill\n");

	// five seconds is far longer than a 1 MB fill takes
	VkResult wait = vkWaitForFences(dev, 1, &fence, VK_TRUE, 5ull * 1000 * 1000 * 1000);
	double waited = now() - t0;
	uint32_t filled = 0;
	for (uint32_t i = 0; i < WORDS; i++)
		filled += words[i] == PATTERN;
	printf("pdvkfill: fence %s after %.3f s, %u of %u words filled\n",
	    wait == VK_SUCCESS ? "signalled" : wait == VK_TIMEOUT ? "timed out" : "failed", waited, filled, WORDS);

	if (wait == VK_SUCCESS && filled == WORDS)
		printf("pdvkfill: PASS, the GPU ran the fill and its completion was seen\n");
	else if (filled == WORDS)
		printf("pdvkfill: the GPU ran the fill but the fence did not signal: completion is not seen\n");
	else if (filled != 0)
		printf("pdvkfill: the GPU wrote part of the buffer\n");
	else
		printf("pdvkfill: nothing was written: the work did not reach the GPU or was not scheduled\n");
	return wait == VK_SUCCESS && filled == WORDS ? 0 : 1;
}
