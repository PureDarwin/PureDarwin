// pdvkdisplay: the spinning cube through VK_KHR_display, rendered straight into scanout buffers that
// PDSun60iDisplay flips at vblank. usage: pdvkdisplay [seconds] [width height]; the cube is drawn in a
// width x height viewport centred on the screen (default: the whole screen)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#include "cube_frag.h"
#include "cube_vert.h"
#include "cube_math.h"

#define MAX_IMAGES		8
#define IN_FLIGHT		2
#define TIMEOUT_NS		5000000000ull
#define REPORT_NS		5000000000ull

#define CK(x) do { \
	VkResult r_ = (x); \
	if (r_ != VK_SUCCESS) { \
		printf("pdvkdisplay: %s failed: %d\nFAIL\n", #x, r_); \
		exit(1); \
	} \
} while (0)

static uint64_t
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static VkShaderModule
shader(VkDevice d, const uint32_t *code, size_t size)
{
	VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0, 0, size, code };
	VkShaderModule m;

	CK(vkCreateShaderModule(d, &ci, 0, &m));
	return m;
}

int
main(int argc, char **argv)
{
	const char *iext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME };
	const char *dext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "pdvkdisplay", 1, 0, 0, VK_API_VERSION_1_0 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &ai, 0, 0, 2, iext };
	double seconds = argc > 1 ? atof(argv[1]) : 60;
	uint32_t vw = argc > 3 ? (uint32_t)atoi(argv[2]) : 0, vh = argc > 3 ? (uint32_t)atoi(argv[3]) : 0;
	VkPhysicalDeviceProperties props;
	VkDisplayPropertiesKHR disp;
	VkDisplayModePropertiesKHR modes[8];
	VkPhysicalDevice pd;
	VkInstance inst;
	VkDevice dev;
	VkQueue q;
	VkSurfaceKHR surf;
	uint32_t n = 1, nmodes = 8;
	float prio = 1;

	setvbuf(stdout, NULL, _IONBF, 0);
	CK(vkCreateInstance(&ici, 0, &inst));
	CK(vkEnumeratePhysicalDevices(inst, &n, &pd));
	if (n == 0) {
		printf("pdvkdisplay: no physical device\nFAIL\n");
		return 1;
	}
	vkGetPhysicalDeviceProperties(pd, &props);

	n = 1;
	if (vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &n, &disp) < 0 || n == 0) {
		printf("pdvkdisplay: no display\nFAIL\n");
		return 1;
	}
	CK(vkGetDisplayModePropertiesKHR(pd, disp.display, &nmodes, modes));
	if (nmodes == 0) {
		printf("pdvkdisplay: no display mode\nFAIL\n");
		return 1;
	}
	VkExtent2D ext = modes[0].parameters.visibleRegion;

	if (vw == 0 || vw > ext.width || vh == 0 || vh > ext.height) {
		vw = ext.width;
		vh = ext.height;
	}
	printf("pdvkdisplay: device %s, display %s %ux%u @ %.3f hz, cube %ux%u, %.0f s\n", props.deviceName,
	    disp.displayName ? disp.displayName : "?", ext.width, ext.height,
	    modes[0].parameters.refreshRate / 1000.0, vw, vh, seconds);

	VkDisplaySurfaceCreateInfoKHR dsci = { VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR, 0, 0,
	    modes[0].displayMode, 0, 0, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, 1.0f,
	    VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR, ext };

	CK(vkCreateDisplayPlaneSurfaceKHR(inst, &dsci, 0, &surf));

	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &qci, 0, 0, 1, dext };
	VkBool32 supported = VK_FALSE;

	CK(vkGetPhysicalDeviceSurfaceSupportKHR(pd, 0, surf, &supported));
	if (!supported) {
		printf("pdvkdisplay: queue cannot present to the display\nFAIL\n");
		return 1;
	}
	CK(vkCreateDevice(pd, &dci, 0, &dev));
	vkGetDeviceQueue(dev, 0, 0, &q);

	VkSurfaceFormatKHR fmts[16];
	uint32_t nfmt = 16;
	VkFormat fmt = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR cs = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	CK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surf, &nfmt, fmts));
	for (uint32_t i = 0; i < nfmt; i++) {
		if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || (fmt == VK_FORMAT_UNDEFINED &&
		    fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB)) {
			fmt = fmts[i].format;
			cs = fmts[i].colorSpace;
		}
	}
	if (fmt == VK_FORMAT_UNDEFINED) {
		fmt = fmts[0].format;
		cs = fmts[0].colorSpace;
	}

	VkSurfaceCapabilitiesKHR caps;
	uint32_t want = 3;

	CK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &caps));
	if (want < caps.minImageCount)
		want = caps.minImageCount;
	if (caps.maxImageCount && want > caps.maxImageCount)
		want = caps.maxImageCount;

	VkSwapchainCreateInfoKHR sci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, 0, 0, surf, want, fmt, cs, ext, 1,
	    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, 0,
	    VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR,
	    VK_TRUE, VK_NULL_HANDLE };
	VkSwapchainKHR sc;
	VkImage imgs[MAX_IMAGES];
	uint32_t nimg = MAX_IMAGES;

	CK(vkCreateSwapchainKHR(dev, &sci, 0, &sc));
	CK(vkGetSwapchainImagesKHR(dev, sc, &nimg, imgs));
	printf("pdvkdisplay: swapchain of %u images, format %d\n", nimg, fmt);

	VkAttachmentDescription att = { 0, fmt, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
	    VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sp = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 1, &ref };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0, 1, &att, 1, &sp };
	VkRenderPass rp;
	VkImageView views[MAX_IMAGES];
	VkFramebuffer fbs[MAX_IMAGES];

	CK(vkCreateRenderPass(dev, &rpci, 0, &rp));
	for (uint32_t i = 0; i < nimg; i++) {
		VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0, 0, imgs[i],
		    VK_IMAGE_VIEW_TYPE_2D, fmt, { 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
		VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0, rp, 1, &views[i],
		    ext.width, ext.height, 1 };

		CK(vkCreateImageView(dev, &vci, 0, &views[i]));
		CK(vkCreateFramebuffer(dev, &fci, 0, &fbs[i]));
	}

	VkPushConstantRange pcr = { VK_SHADER_STAGE_VERTEX_BIT, 0, 64 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 0, 0, 0, 0, 1, &pcr };
	VkPipelineLayout pl;

	CK(vkCreatePipelineLayout(dev, &plci, 0, &pl));

	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_VERTEX_BIT,
		    shader(dev, cube_vert, sizeof(cube_vert)), "main" },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
		    shader(dev, cube_frag, sizeof(cube_frag)), "main" },
	};
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	    0, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkViewport vp = { (float)(ext.width - vw) / 2, (float)(ext.height - vh) / 2, vw, vh, 0, 1 };
	VkRect2D full = { { 0, 0 }, ext };
	VkPipelineViewportStateCreateInfo vs = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, 0, 0, 1,
	    &vp, 1, &full };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, 0, 0,
	    VK_SAMPLE_COUNT_1_BIT };
	VkPipelineColorBlendAttachmentState cba = { 0 };
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 0, 0,
	    0, 0, 1, &cba };
	VkPipeline pipe;

	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1;
	cba.colorWriteMask = 0xf;

	VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, 0, 0, 2, st, &vi,
	    &ia, 0, &vs, &rs, &ms, 0, &cb, 0, pl, rp, 0 };

	CK(vkCreateGraphicsPipelines(dev, 0, 1, &gci, 0, &pipe));

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0,
	    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, 0,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, IN_FLIGHT };
	VkSemaphoreCreateInfo semci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fnci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 0, VK_FENCE_CREATE_SIGNALED_BIT };
	VkCommandPool pool;
	VkCommandBuffer cmd[IN_FLIGHT];
	VkSemaphore acquired[IN_FLIGHT], rendered[MAX_IMAGES];
	VkFence fence[IN_FLIGHT];

	CK(vkCreateCommandPool(dev, &cpci, 0, &pool));
	cbai.commandPool = pool;
	CK(vkAllocateCommandBuffers(dev, &cbai, cmd));
	for (int i = 0; i < IN_FLIGHT; i++) {
		CK(vkCreateSemaphore(dev, &semci, 0, &acquired[i]));
		CK(vkCreateFence(dev, &fnci, 0, &fence[i]));
	}
	for (uint32_t i = 0; i < nimg; i++) {
		CK(vkCreateSemaphore(dev, &semci, 0, &rendered[i]));
	}
	printf("pdvkdisplay: pipeline compiled, presenting\n");

	VkClearValue clr = { .color = { .float32 = { 0.02f, 0.02f, 0.08f, 1.0f } } };
	uint64_t t0 = now_ns(), last = t0, end = t0 + (uint64_t)(seconds * 1e9);
	uint64_t frames = 0, win = 0, fence_ns = 0, acq_ns = 0, sub_ns = 0, pres_ns = 0;
	float mvp[16];

	for (uint64_t k = 0; now_ns() < end; k++) {
		uint32_t f = k % IN_FLIGHT, idx;
		uint64_t a = now_ns(), b, c, d;

		CK(vkWaitForFences(dev, 1, &fence[f], VK_TRUE, TIMEOUT_NS));
		CK(vkResetFences(dev, 1, &fence[f]));
		b = now_ns();
		CK(vkAcquireNextImageKHR(dev, sc, TIMEOUT_NS, acquired[f], VK_NULL_HANDLE, &idx));
		c = now_ns();

		VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
		VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0, rp, fbs[idx], full, 1, &clr };
		VkPipelineStageFlags ws = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 1, &acquired[f], &ws, 1, &cmd[f], 1, &rendered[idx] };
		VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, 0, 1, &rendered[idx], 1, &sc, &idx, 0 };

		cube_mvp(mvp, (a - t0) / 1e9f, (float)vw / vh);
		CK(vkResetCommandBuffer(cmd[f], 0));
		CK(vkBeginCommandBuffer(cmd[f], &cbbi));
		vkCmdBeginRenderPass(cmd[f], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmd[f], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		vkCmdPushConstants(cmd[f], pl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), mvp);
		vkCmdDraw(cmd[f], 36, 1, 0, 0);
		vkCmdEndRenderPass(cmd[f]);
		CK(vkEndCommandBuffer(cmd[f]));
		CK(vkQueueSubmit(q, 1, &si, fence[f]));
		d = now_ns();
		CK(vkQueuePresentKHR(q, &pi));

		fence_ns += b - a;
		acq_ns += c - b;
		sub_ns += d - c;
		pres_ns += now_ns() - d;
		frames++;
		win++;
		if (now_ns() - last >= REPORT_NS) {
			uint64_t t = now_ns();

			printf("pdvkdisplay: %ux%u %.1f fps (fence %.2f, acquire %.2f, record+submit %.2f, present %.2f ms "
			    "per frame), %llu frames\n", vw, vh, win * 1e9 / (t - last), fence_ns / 1e6 / win,
			    acq_ns / 1e6 / win, sub_ns / 1e6 / win, pres_ns / 1e6 / win, (unsigned long long)frames);
			last = t;
			win = fence_ns = acq_ns = sub_ns = pres_ns = 0;
		}
	}

	t0 = now_ns() - t0;
	printf("pdvkdisplay: %ux%u %llu frames in %.1f s, %.1f fps average\n", vw, vh, (unsigned long long)frames,
	    t0 / 1e9, frames * 1e9 / t0);

	vkDeviceWaitIdle(dev);
	vkDestroySwapchainKHR(dev, sc, 0);
	vkDestroySurfaceKHR(inst, surf, 0);
	vkDestroyDevice(dev, 0);
	vkDestroyInstance(inst, 0);
	printf("pdvkdisplay: %s\n", frames ? "PASS" : "FAIL");
	return frames ? 0 : 1;
}
