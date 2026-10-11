// pdvktri: an offscreen 64x64 vulkan triangle on mesa's powervr driver, checked against the linux render.
// "copy" (default) reads back through a transfer job like the linux test, "linear" via a linear image

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "tri_frag.h"
#include "tri_vert.h"

#define W			64
#define H			64
#define CLEAR_B			0x40
#define FENCE_TIMEOUT_NS	5000000000ull

// the linux render on this gpu with mesa 26.0.3: fnv-1a of the rgb plane, and its coverage
#define LINUX_RGB_FNV		0xd2132299u
#define LINUX_COVERED		1352

#define CK(x) do { \
	VkResult r_ = (x); \
	if (r_ != VK_SUCCESS) { \
		printf("pdvktri: %s failed: %d\nFAIL\n", #x, r_); \
		exit(1); \
	} \
} while (0)

struct probe {
	int x, y;
	uint8_t r, g, b;
};

// pixels of the linux render: the clear colour off the triangle, the gouraud blend inside it
static const struct probe probes[] = {
	{ 1, 1, 0x00, 0x00, 0x40 },
	{ 62, 62, 0x00, 0x00, 0x40 },
	{ 32, 5, 0x00, 0x00, 0x40 },
	{ 32, 12, 0xe1, 0x12, 0x0d },
	{ 32, 32, 0x7d, 0x43, 0x3f },
	{ 20, 50, 0x23, 0x35, 0xa7 },
	{ 44, 50, 0x23, 0xac, 0x30 },
};

static uint32_t
memtype(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags want)
{
	VkPhysicalDeviceMemoryProperties mp;

	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
		if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
			return i;
	}
	printf("pdvktri: no memory type for 0x%x\nFAIL\n", want);
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

static int
near(uint8_t a, uint8_t b)
{
	return abs((int)a - (int)b) <= 2;
}

// compares the readback with the linux render, returns the number of failed checks
static int
check(const uint8_t *px)
{
	uint32_t fnv = 0x811c9dc5u;
	int covered = 0, bad = 0, alpha = 0;

	for (int i = 0; i < W * H; i++) {
		const uint8_t *p = px + i * 4;

		for (int c = 0; c < 3; c++) {
			fnv = (fnv ^ p[c]) * 0x01000193u;
		}
		if (p[0] != 0 || p[1] != 0 || p[2] != CLEAR_B)
			covered++;
		if (p[3] != 0xff)
			alpha++;
	}

	for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
		const struct probe *pr = &probes[i];
		const uint8_t *p = px + (pr->y * W + pr->x) * 4;
		int ok = near(p[0], pr->r) && near(p[1], pr->g) && near(p[2], pr->b) && p[3] == 0xff;

		printf("pdvktri: pixel (%d,%d) = %02x %02x %02x %02x, linux %02x %02x %02x ff %s\n", pr->x, pr->y,
		    p[0], p[1], p[2], p[3], pr->r, pr->g, pr->b, ok ? "ok" : "MISMATCH");
		if (!ok)
			bad++;
	}

	printf("pdvktri: %d of %d pixels covered (linux %d), %d with alpha != ff\n", covered, W * H, LINUX_COVERED,
	    alpha);
	if (abs(covered - LINUX_COVERED) > LINUX_COVERED / 50)
		bad++;
	if (alpha)
		bad++;

	printf("pdvktri: rgb fnv-1a 0x%08x, linux 0x%08x: %s\n", fnv, LINUX_RGB_FNV,
	    fnv == LINUX_RGB_FNV ? "bit-exact" : "differs");
	return bad;
}

static void
write_ppm(const char *path, const uint8_t *px)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL)
		return;

	fprintf(f, "P6 %d %d 255\n", W, H);
	for (int i = 0; i < W * H; i++) {
		fwrite(px + i * 4, 1, 3, f);
	}
	fclose(f);
	printf("pdvktri: wrote %s\n", path);
}

int
main(int argc, char **argv)
{
	VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "pdvktri", 1, 0, 0, VK_API_VERSION_1_0 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &ai };
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice pd;
	VkInstance inst;
	VkDevice dev;
	VkQueue q;
	uint32_t n = 1;
	float prio = 1;
	int linear = argc > 1 && strcmp(argv[1], "linear") == 0;
	const char *ppm = argc > 2 ? argv[2] : (linear ? "/var/tmp/pdvktri-linear.ppm" : "/var/tmp/pdvktri.ppm");
	VkFormatProperties fp;
	uint8_t *px, *packed;
	int bad;

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("pdvktri: %s readback\n", linear ? "linear image" : "copy to buffer");
	CK(vkCreateInstance(&ici, 0, &inst));
	CK(vkEnumeratePhysicalDevices(inst, &n, &pd));
	if (n == 0) {
		printf("pdvktri: no physical device\nFAIL\n");
		return 1;
	}

	vkGetPhysicalDeviceProperties(pd, &props);
	printf("pdvktri: device %s, api %u.%u.%u, driver 0x%x\n", props.deviceName,
	    VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
	    VK_API_VERSION_PATCH(props.apiVersion), props.driverVersion);

	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &qci };

	vkGetPhysicalDeviceFormatProperties(pd, VK_FORMAT_R8G8B8A8_UNORM, &fp);
	if (linear && !(fp.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
		printf("pdvktri: no linear rgba8 colour attachments (features 0x%x)\nFAIL\n", fp.linearTilingFeatures);
		return 1;
	}

	CK(vkCreateDevice(pd, &dci, 0, &dev));
	vkGetDeviceQueue(dev, 0, 0, &q);
	printf("pdvktri: device created\n");

	VkImageCreateInfo imci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 0, 0, VK_IMAGE_TYPE_2D,
	    VK_FORMAT_R8G8B8A8_UNORM, { W, H, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
	    linear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL,
	    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (linear ? 0 : VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
	    VK_SHARING_MODE_EXCLUSIVE, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED };
	VkMemoryRequirements mr;
	VkDeviceMemory imem, bmem;
	VkImage img;

	CK(vkCreateImage(dev, &imci, 0, &img));
	vkGetImageMemoryRequirements(dev, img, &mr);

	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, mr.size,
	    memtype(pd, mr.memoryTypeBits, linear ?
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };

	CK(vkAllocateMemory(dev, &mai, 0, &imem));
	CK(vkBindImageMemory(dev, img, imem, 0));

	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 0, 0, W * H * 4,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT };
	VkBuffer buf;

	CK(vkCreateBuffer(dev, &bci, 0, &buf));
	vkGetBufferMemoryRequirements(dev, buf, &mr);
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = memtype(pd, mr.memoryTypeBits,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CK(vkAllocateMemory(dev, &mai, 0, &bmem));
	CK(vkBindBufferMemory(dev, buf, bmem, 0));

	// poison the readback buffer so an unwritten copy cannot pass
	CK(vkMapMemory(dev, bmem, 0, W * H * 4, 0, (void **)&px));
	memset(px, 0xa5, W * H * 4);
	vkUnmapMemory(dev, bmem);
	if (linear) {
		CK(vkMapMemory(dev, imem, 0, VK_WHOLE_SIZE, 0, (void **)&px));
		memset(px, 0xa5, W * H * 4);
		vkUnmapMemory(dev, imem);
	}

	VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 0, 0, img, VK_IMAGE_VIEW_TYPE_2D,
	    VK_FORMAT_R8G8B8A8_UNORM, { 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
	VkImageView view;

	CK(vkCreateImageView(dev, &vci, 0, &view));

	VkAttachmentDescription att = { 0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
	    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	    VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
	    linear ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sp = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 1, &ref };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, 0, 0, 1, &att, 1, &sp };
	VkRenderPass rp;

	CK(vkCreateRenderPass(dev, &rpci, 0, &rp));

	VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 0, 0, rp, 1, &view, W, H, 1 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	VkPipelineLayout pl;
	VkFramebuffer fb;

	CK(vkCreateFramebuffer(dev, &fci, 0, &fb));
	CK(vkCreatePipelineLayout(dev, &plci, 0, &pl));

	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_VERTEX_BIT,
		    shader(dev, tri_vert, sizeof(tri_vert)), "main" },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
		    shader(dev, tri_frag, sizeof(tri_frag)), "main" },
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

	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.lineWidth = 1;
	cba.colorWriteMask = 0xf;

	VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, 0, 0, 2, st, &vi,
	    &ia, 0, &vs, &rs, &ms, 0, &cb, 0, pl, rp, 0 };

	CK(vkCreateGraphicsPipelines(dev, 0, 1, &gci, 0, &pipe));
	printf("pdvktri: pipeline compiled\n");

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0, 0, 0 };
	VkCommandPool pool;

	CK(vkCreateCommandPool(dev, &cpci, 0, &pool));

	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, pool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkClearValue clr = { .color = { .float32 = { 0.0f, 0.0f, 0.25f, 1.0f } } };
	VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0, rp, fb, sc, 1, &clr };
	VkBufferImageCopy bic = { 0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { W, H, 1 } };
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
	    VK_ACCESS_HOST_READ_BIT };
	VkCommandBuffer cmd;

	CK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
	CK(vkBeginCommandBuffer(cmd, &cbbi));
	vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmd);
	if (linear) {
		mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
		    &mb, 0, 0, 0, 0);
	} else {
		vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &bic);
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, 0,
		    0, 0);
	}
	CK(vkEndCommandBuffer(cmd));

	VkFenceCreateInfo fnci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, 0, 0, 1, &cmd };
	VkFence fence;

	CK(vkCreateFence(dev, &fnci, 0, &fence));
	CK(vkQueueSubmit(q, 1, &si, fence));
	printf("pdvktri: submitted\n");
	CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, FENCE_TIMEOUT_NS));
	printf("pdvktri: fence signalled\n");

	packed = malloc(W * H * 4);
	if (linear) {
		VkImageSubresource sub = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout lay;

		vkGetImageSubresourceLayout(dev, img, &sub, &lay);
		printf("pdvktri: linear image offset %llu row pitch %llu\n", (unsigned long long)lay.offset,
		    (unsigned long long)lay.rowPitch);
		CK(vkMapMemory(dev, imem, 0, VK_WHOLE_SIZE, 0, (void **)&px));
		for (int y = 0; y < H; y++) {
			memcpy(packed + y * W * 4, px + lay.offset + y * lay.rowPitch, W * 4);
		}
		vkUnmapMemory(dev, imem);
	} else {
		CK(vkMapMemory(dev, bmem, 0, W * H * 4, 0, (void **)&px));
		memcpy(packed, px, W * H * 4);
		vkUnmapMemory(dev, bmem);
	}
	bad = check(packed);
	write_ppm(ppm, packed);
	free(packed);

	vkDestroyFence(dev, fence, 0);
	vkDestroyCommandPool(dev, pool, 0);
	vkDestroyPipeline(dev, pipe, 0);
	vkDestroyShaderModule(dev, st[0].module, 0);
	vkDestroyShaderModule(dev, st[1].module, 0);
	vkDestroyPipelineLayout(dev, pl, 0);
	vkDestroyFramebuffer(dev, fb, 0);
	vkDestroyRenderPass(dev, rp, 0);
	vkDestroyImageView(dev, view, 0);
	vkDestroyBuffer(dev, buf, 0);
	vkFreeMemory(dev, bmem, 0);
	vkDestroyImage(dev, img, 0);
	vkFreeMemory(dev, imem, 0);
	vkDestroyDevice(dev, 0);
	vkDestroyInstance(inst, 0);

	printf("pdvktri: %s\n", bad ? "FAIL" : "PASS");
	return bad ? 1 : 0;
}
