// pdvkspin: a spinning cube on mesa's powervr vulkan driver, rendered to linear host-visible images and
// copied into the console framebuffer. usage: pdvkspin [seconds] [width height] [angle]

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#include "cube_frag.h"
#include "cube_vert.h"
#include "cube_math.h"
#include "pdfb.h"

#define NBUF			2
#define FENCE_TIMEOUT_NS	5000000000ull
#define REPORT_NS		5000000000ull

#define CK(x) do { \
	VkResult r_ = (x); \
	if (r_ != VK_SUCCESS) { \
		printf("pdvkspin: %s failed: %d\nFAIL\n", #x, r_); \
		exit(1); \
	} \
} while (0)

struct slot {
	VkImage img;
	VkDeviceMemory mem;
	VkImageView view;
	VkFramebuffer fb;
	VkCommandBuffer cmd;
	VkFence fence;
	uint8_t *px;
	int busy;
};

static uint64_t
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static uint32_t
memtype(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags want)
{
	VkPhysicalDeviceMemoryProperties mp;

	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
		if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
			return i;
	}
	printf("pdvkspin: no memory type for 0x%x\nFAIL\n", want);
	exit(1);
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
	VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "pdvkspin", 1, 0, 0, VK_API_VERSION_1_0 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &ai };
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice pd;
	VkInstance inst;
	VkDevice dev;
	VkQueue q;
	uint32_t n = 1, scale;
	float prio = 1;
	double seconds = argc > 1 ? atof(argv[1]) : 60;
	uint32_t W = argc > 3 ? (uint32_t)atoi(argv[2]) : 960;
	uint32_t H = argc > 3 ? (uint32_t)atoi(argv[3]) : 540;
	int fixed = argc > 4;
	float fixed_t = fixed ? (float)atof(argv[4]) : 0;
	VkFormat fmt = VK_FORMAT_B8G8R8A8_UNORM;
	VkFormatProperties fp;
	struct slot sl[NBUF];
	struct pdfb fb;
	int swap = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	if (pdfb_open(&fb) != 0) {
		printf("pdvkspin: no framebuffer\nFAIL\n");
		return 1;
	}
	scale = fb.width / W < fb.height / H ? fb.width / W : fb.height / H;
	if (scale == 0) {
		printf("pdvkspin: %ux%u does not fit the %ux%u framebuffer\nFAIL\n", W, H, fb.width, fb.height);
		return 1;
	}

	CK(vkCreateInstance(&ici, 0, &inst));
	CK(vkEnumeratePhysicalDevices(inst, &n, &pd));
	if (n == 0) {
		printf("pdvkspin: no physical device\nFAIL\n");
		return 1;
	}
	vkGetPhysicalDeviceProperties(pd, &props);

	// the framebuffer is a8r8g8b8, so render bgra when linear bgra attachments exist, else swap on copy
	vkGetPhysicalDeviceFormatProperties(pd, fmt, &fp);
	if (!(fp.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
		fmt = VK_FORMAT_R8G8B8A8_UNORM;
		swap = 1;
		vkGetPhysicalDeviceFormatProperties(pd, fmt, &fp);
		if (!(fp.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
			printf("pdvkspin: no linear colour attachments\nFAIL\n");
			return 1;
		}
	}
	printf("pdvkspin: device %s, %ux%u %s x%u into %ux%u, %.0f s\n", props.deviceName, W, H,
	    swap ? "rgba (swapped on copy)" : "bgra", scale, fb.width, fb.height, seconds);

	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &qci };

	CK(vkCreateDevice(pd, &dci, 0, &dev));
	vkGetDeviceQueue(dev, 0, 0, &q);

	VkAttachmentDescription att = { 0, fmt, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
	    VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sp = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 1, &ref };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0, 1, &att, 1, &sp };
	VkRenderPass rp;

	CK(vkCreateRenderPass(dev, &rpci, 0, &rp));

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0,
	    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
	VkCommandPool pool;

	CK(vkCreateCommandPool(dev, &cpci, 0, &pool));

	VkImageSubresource sub = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout lay;

	for (int i = 0; i < NBUF; i++) {
		VkImageCreateInfo imci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0, 0, VK_IMAGE_TYPE_2D, fmt,
		    { W, H, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR,
		    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED };
		VkMemoryRequirements mr;

		CK(vkCreateImage(dev, &imci, 0, &sl[i].img));
		vkGetImageMemoryRequirements(dev, sl[i].img, &mr);

		VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, mr.size,
		    memtype(pd, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };

		CK(vkAllocateMemory(dev, &mai, 0, &sl[i].mem));
		CK(vkBindImageMemory(dev, sl[i].img, sl[i].mem, 0));
		CK(vkMapMemory(dev, sl[i].mem, 0, VK_WHOLE_SIZE, 0, (void **)&sl[i].px));

		VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0, 0, sl[i].img,
		    VK_IMAGE_VIEW_TYPE_2D, fmt, { 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

		CK(vkCreateImageView(dev, &vci, 0, &sl[i].view));

		VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0, rp, 1, &sl[i].view,
		    W, H, 1 };

		CK(vkCreateFramebuffer(dev, &fci, 0, &sl[i].fb));

		VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, pool,
		    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
		VkFenceCreateInfo fnci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

		CK(vkAllocateCommandBuffers(dev, &cbai, &sl[i].cmd));
		CK(vkCreateFence(dev, &fnci, 0, &sl[i].fence));
		sl[i].busy = 0;
	}
	vkGetImageSubresourceLayout(dev, sl[0].img, &sub, &lay);
	printf("pdvkspin: linear images, offset %llu row pitch %llu\n", (unsigned long long)lay.offset,
	    (unsigned long long)lay.rowPitch);

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
	VkViewport vp = { 0, 0, W, H, 0, 1 };
	VkRect2D sc = { { 0, 0 }, { W, H } };
	VkPipelineViewportStateCreateInfo vs = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, 0, 0, 1,
	    &vp, 1, &sc };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, 0, 0,
	    VK_SAMPLE_COUNT_1_BIT };
	VkPipelineColorBlendAttachmentState cba = { 0 };
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 0, 0,
	    0, 0, 1, &cba };
	VkPipeline pipe;

	// the cube is convex, so back-face culling stands in for a depth buffer
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1;
	cba.colorWriteMask = 0xf;

	VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, 0, 0, 2, st, &vi,
	    &ia, 0, &vs, &rs, &ms, 0, &cb, 0, pl, rp, 0 };

	CK(vkCreateGraphicsPipelines(dev, 0, 1, &gci, 0, &pipe));
	printf("pdvkspin: pipeline compiled\n");

	VkClearValue clr = { .color = { .float32 = { 0.02f, 0.02f, 0.08f, 1.0f } } };
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	    VK_ACCESS_HOST_READ_BIT };
	uint64_t t0 = now_ns(), last = t0, end = t0 + (uint64_t)(seconds * 1e9);
	uint64_t frames = 0, win_frames = 0, wait_ns = 0, copy_ns = 0, submit_ns = 0;
	float mvp[16];

	// slot k%2 is presented two submits after it was queued, so the cpu copy overlaps the next render
	for (uint64_t k = 0;; k++) {
		struct slot *s = &sl[k % NBUF];
		uint64_t t = now_ns();
		int done = t >= end;

		if (s->busy) {
			uint64_t a = now_ns(), b;

			CK(vkWaitForFences(dev, 1, &s->fence, VK_TRUE, FENCE_TIMEOUT_NS));
			b = now_ns();
			pdfb_blit(&fb, s->px + lay.offset, W, H, lay.rowPitch, scale, swap);
			wait_ns += b - a;
			copy_ns += now_ns() - b;
			CK(vkResetFences(dev, 1, &s->fence));
			s->busy = 0;
			frames++;
			win_frames++;
		}

		t = now_ns();
		if (t - last >= REPORT_NS) {
			printf("pdvkspin: %.1f fps (fence wait %.2f ms, copy %.2f ms, record+submit %.2f ms per frame), "
			    "%llu frames\n", win_frames * 1e9 / (t - last), wait_ns / 1e6 / win_frames,
			    copy_ns / 1e6 / win_frames, submit_ns / 1e6 / win_frames, (unsigned long long)frames);
			last = t;
			win_frames = wait_ns = copy_ns = submit_ns = 0;
		}

		if (done) {
			if (!sl[0].busy && !sl[1].busy)
				break;
			continue;
		}

		VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0,
		    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
		VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0, rp, s->fb, sc, 1, &clr };
		VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, 0, 0, 1, &s->cmd };

		cube_mvp(mvp, fixed ? fixed_t : (t - t0) / 1e9f, (float)W / H);
		t = now_ns();
		CK(vkResetCommandBuffer(s->cmd, 0));
		CK(vkBeginCommandBuffer(s->cmd, &cbbi));
		vkCmdBeginRenderPass(s->cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(s->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		vkCmdPushConstants(s->cmd, pl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), mvp);
		vkCmdDraw(s->cmd, 36, 1, 0, 0);
		vkCmdEndRenderPass(s->cmd);
		vkCmdPipelineBarrier(s->cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		    0, 1, &mb, 0, 0, 0, 0);
		CK(vkEndCommandBuffer(s->cmd));
		CK(vkQueueSubmit(q, 1, &si, s->fence));
		submit_ns += now_ns() - t;
		s->busy = 1;
	}

	t0 = now_ns() - t0;
	printf("pdvkspin: %llu frames in %.1f s, %.1f fps average\n", (unsigned long long)frames, t0 / 1e9,
	    frames * 1e9 / t0);

	vkDeviceWaitIdle(dev);
	vkDestroyPipeline(dev, pipe, 0);
	vkDestroyShaderModule(dev, st[0].module, 0);
	vkDestroyShaderModule(dev, st[1].module, 0);
	vkDestroyPipelineLayout(dev, pl, 0);
	for (int i = 0; i < NBUF; i++) {
		vkDestroyFence(dev, sl[i].fence, 0);
		vkDestroyFramebuffer(dev, sl[i].fb, 0);
		vkDestroyImageView(dev, sl[i].view, 0);
		vkDestroyImage(dev, sl[i].img, 0);
		vkFreeMemory(dev, sl[i].mem, 0);
	}
	vkDestroyCommandPool(dev, pool, 0);
	vkDestroyRenderPass(dev, rp, 0);
	vkDestroyDevice(dev, 0);
	vkDestroyInstance(inst, 0);
	pdfb_close(&fb);

	printf("pdvkspin: %s\n", frames ? "PASS" : "FAIL");
	return frames ? 0 : 1;
}
