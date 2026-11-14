// pdvktri.c - a triangle on the GPU's own display output through VK_KHR_display, no window system: a Linux
// program, run under elftrans
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#include "tri_vert.h"
#include "tri_frag.h"

#define CHECK(what, call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { printf("pdvktri: %s: %d\n", what, r_); return 1; } } while (0)
#define MAX_IMAGES 8

int
main(int argc, char **argv)
{
	int seconds = argc > 1 ? atoi(argv[1]) : 20;
	const char *iexts[] = { "VK_KHR_surface", "VK_KHR_display" };
	const char *dexts[] = { "VK_KHR_swapchain" };
	VkInstance instance;
	VkPhysicalDevice pd;
	uint32_t n = 1;

	setvbuf(stdout, NULL, _IONBF, 0);
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "pdvktri",
		.apiVersion = VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
		.enabledExtensionCount = 2, .ppEnabledExtensionNames = iexts };
	CHECK("vkCreateInstance", vkCreateInstance(&ici, NULL, &instance));
	CHECK("vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices(instance, &n, &pd));

	// the first display, its preferred (else first) mode, a plane that can show it
	VkDisplayPropertiesKHR disp;
	n = 1;
	CHECK("vkGetPhysicalDeviceDisplayPropertiesKHR", vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &n, &disp));
	if (n == 0) {
		printf("pdvktri: no display\n");
		return 1;
	}
	printf("pdvktri: display %s, %ux%u\n", disp.displayName ? disp.displayName : "?",
	    disp.physicalResolution.width, disp.physicalResolution.height);

	VkDisplayModePropertiesKHR modes[64];
	uint32_t nm = 64, pick = 0;
	CHECK("vkGetDisplayModePropertiesKHR", vkGetDisplayModePropertiesKHR(pd, disp.display, &nm, modes));
	for (uint32_t i = 0; i < nm; i++) {
		if (modes[i].parameters.visibleRegion.width == disp.physicalResolution.width &&
		    modes[i].parameters.visibleRegion.height == disp.physicalResolution.height) {
			pick = i;
			break;
		}
	}
	VkExtent2D ext = modes[pick].parameters.visibleRegion;
	printf("pdvktri: mode %ux%u @ %u mHz, %u modes\n", ext.width, ext.height, modes[pick].parameters.refreshRate, nm);

	VkDisplayPlanePropertiesKHR planes[16];
	uint32_t np = 16, plane = UINT32_MAX;
	CHECK("vkGetPhysicalDeviceDisplayPlanePropertiesKHR", vkGetPhysicalDeviceDisplayPlanePropertiesKHR(pd, &np, planes));
	for (uint32_t i = 0; i < np && plane == UINT32_MAX; i++) {
		VkDisplayKHR ds[8];
		uint32_t nd = 8;

		if (vkGetDisplayPlaneSupportedDisplaysKHR(pd, i, &nd, ds) != VK_SUCCESS)
			continue;
		for (uint32_t j = 0; j < nd; j++)
			if (ds[j] == disp.display)
				plane = i;
	}
	if (plane == UINT32_MAX) {
		printf("pdvktri: no plane for the display\n");
		return 1;
	}

	VkDisplaySurfaceCreateInfoKHR sci = { .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
		.displayMode = modes[pick].displayMode, .planeIndex = plane, .planeStackIndex = planes[plane].currentStackIndex,
		.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .globalAlpha = 1.0f,
		.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR, .imageExtent = ext };
	VkSurfaceKHR surface;
	CHECK("vkCreateDisplayPlaneSurfaceKHR", vkCreateDisplayPlaneSurfaceKHR(instance, &sci, NULL, &surface));
	printf("pdvktri: display surface on plane %u\n", plane);

	// a device with one graphics queue that can present to it
	VkBool32 ok = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(pd, 0, surface, &ok);
	if (!ok) {
		printf("pdvktri: queue family 0 cannot present\n");
		return 1;
	}
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0,
		.queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dexts };
	VkDevice dev;
	VkQueue queue;
	CHECK("vkCreateDevice", vkCreateDevice(pd, &dci, NULL, &dev));
	vkGetDeviceQueue(dev, 0, 0, &queue);

	// the swapchain, in the surface's first format
	VkSurfaceFormatKHR fmts[16];
	uint32_t nf = 16;
	VkSurfaceCapabilitiesKHR caps;
	CHECK("vkGetPhysicalDeviceSurfaceFormatsKHR", vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &nf, fmts));
	CHECK("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));
	uint32_t want = caps.minImageCount + 1;
	if (caps.maxImageCount && want > caps.maxImageCount)
		want = caps.maxImageCount;
	VkSwapchainCreateInfoKHR swci = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface,
		.minImageCount = want, .imageFormat = fmts[0].format, .imageColorSpace = fmts[0].colorSpace,
		.imageExtent = ext, .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE };
	VkSwapchainKHR swapchain;
	CHECK("vkCreateSwapchainKHR", vkCreateSwapchainKHR(dev, &swci, NULL, &swapchain));
	VkImage images[MAX_IMAGES];
	uint32_t ni = MAX_IMAGES;
	CHECK("vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR(dev, swapchain, &ni, images));
	printf("pdvktri: swapchain, %u images, format %d\n", ni, fmts[0].format);

	// render pass, views, framebuffers
	VkAttachmentDescription att = { .format = fmts[0].format, .samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1,
		.pColorAttachments = &ref };
	VkSubpassDependency depend = { .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
	VkRenderPassCreateInfo rpci = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1,
		.pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub, .dependencyCount = 1, .pDependencies = &depend };
	VkRenderPass rp;
	CHECK("vkCreateRenderPass", vkCreateRenderPass(dev, &rpci, NULL, &rp));
	VkImageView views[MAX_IMAGES];
	VkFramebuffer fbs[MAX_IMAGES];
	for (uint32_t i = 0; i < ni; i++) {
		VkImageViewCreateInfo vci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmts[0].format,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
		CHECK("vkCreateImageView", vkCreateImageView(dev, &vci, NULL, &views[i]));
		VkFramebufferCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp,
			.attachmentCount = 1, .pAttachments = &views[i], .width = ext.width, .height = ext.height, .layers = 1 };
		CHECK("vkCreateFramebuffer", vkCreateFramebuffer(dev, &fci, NULL, &fbs[i]));
	}

	// the pipeline: the embedded SPIR-V, no vertex input
	VkShaderModule vs, fs;
	VkShaderModuleCreateInfo vsci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sizeof(tri_vert),
		.pCode = tri_vert };
	VkShaderModuleCreateInfo fsci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sizeof(tri_frag),
		.pCode = tri_frag };
	CHECK("vkCreateShaderModule", vkCreateShaderModule(dev, &vsci, NULL, &vs));
	CHECK("vkCreateShaderModule", vkCreateShaderModule(dev, &fsci, NULL, &fs));
	VkPipelineShaderStageCreateInfo stages[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" },
	};
	VkPipelineVertexInputStateCreateInfo vin = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkViewport vp = { 0, 0, (float)ext.width, (float)ext.height, 0, 1 };
	VkRect2D sc = { { 0, 0 }, ext };
	VkPipelineViewportStateCreateInfo vps = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc };
	VkPipelineRasterizationStateCreateInfo rs = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f };
	VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
	VkPipelineColorBlendAttachmentState cba = { .colorWriteMask = 0xf };
	VkPipelineColorBlendStateCreateInfo cb = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1, .pAttachments = &cba };
	VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	VkPipelineLayout layout;
	CHECK("vkCreatePipelineLayout", vkCreatePipelineLayout(dev, &plci, NULL, &layout));
	VkGraphicsPipelineCreateInfo gpci = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 2,
		.pStages = stages, .pVertexInputState = &vin, .pInputAssemblyState = &ia, .pViewportState = &vps,
		.pRasterizationState = &rs, .pMultisampleState = &ms, .pColorBlendState = &cb, .layout = layout,
		.renderPass = rp };
	VkPipeline pipe;
	CHECK("vkCreateGraphicsPipelines", vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe));
	printf("pdvktri: pipeline\n");

	// one command buffer per image, recorded once
	VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = 0 };
	VkCommandPool pool;
	CHECK("vkCreateCommandPool", vkCreateCommandPool(dev, &cpci, NULL, &pool));
	VkCommandBuffer cmds[MAX_IMAGES];
	VkCommandBufferAllocateInfo cbai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = ni };
	CHECK("vkAllocateCommandBuffers", vkAllocateCommandBuffers(dev, &cbai, cmds));
	for (uint32_t i = 0; i < ni; i++) {
		VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		VkClearValue clear = { .color = { { 0.05f, 0.05f, 0.08f, 1.0f } } };
		VkRenderPassBeginInfo rbi = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp,
			.framebuffer = fbs[i], .renderArea = { { 0, 0 }, ext }, .clearValueCount = 1, .pClearValues = &clear };

		vkBeginCommandBuffer(cmds[i], &bi);
		vkCmdBeginRenderPass(cmds[i], &rbi, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmds[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		vkCmdDraw(cmds[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(cmds[i]);
		CHECK("vkEndCommandBuffer", vkEndCommandBuffer(cmds[i]));
	}

	// present until the time is up
	VkSemaphoreCreateInfo semci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
	VkSemaphore acquired, rendered;
	VkFence fence;
	CHECK("vkCreateSemaphore", vkCreateSemaphore(dev, &semci, NULL, &acquired));
	CHECK("vkCreateSemaphore", vkCreateSemaphore(dev, &semci, NULL, &rendered));
	CHECK("vkCreateFence", vkCreateFence(dev, &fci, NULL, &fence));
	time_t end = time(NULL) + seconds;
	unsigned frames = 0;
	while (time(NULL) < end) {
		uint32_t idx;
		VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(dev, 1, &fence);
		CHECK("vkAcquireNextImageKHR", vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &idx));
		VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &acquired,
			.pWaitDstStageMask = &wait, .commandBufferCount = 1, .pCommandBuffers = &cmds[idx],
			.signalSemaphoreCount = 1, .pSignalSemaphores = &rendered };
		CHECK("vkQueueSubmit", vkQueueSubmit(queue, 1, &si, fence));
		VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1,
			.pWaitSemaphores = &rendered, .swapchainCount = 1, .pSwapchains = &swapchain, .pImageIndices = &idx };
		CHECK("vkQueuePresentKHR", vkQueuePresentKHR(queue, &pi));
		if (++frames == 1)
			printf("pdvktri: first frame presented\n");
	}
	vkDeviceWaitIdle(dev);
	printf("pdvktri: %u frames in %d s\n", frames, seconds);
	vkDestroySwapchainKHR(dev, swapchain, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyInstance(instance, NULL);
	return 0;
}
