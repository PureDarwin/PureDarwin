// zc-spike.c - whether a tile can live in Venus memory the CPU maps and zink renders into
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// exports host-visible memory as an fd, times CPU access against malloc memory, renders into it through
// GL_EXT_memory_object_fd and reads the result back through the CPU mapping
#include <OpenGL/OpenGL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

// libGL doesn't export the memory object entry points, they come from EGL's lookup
static void *gl(const char *name) {
	static void *(*lookup)(const char *);
	if (lookup == NULL)
		lookup = (void *(*)(const char *))dlsym(dlopen("/usr/lib/libEGL.1.dylib", RTLD_LAZY), "eglGetProcAddress");
	return lookup ? lookup(name) : NULL;
}
#define glCreateMemoryObjectsEXT ((PFNGLCREATEMEMORYOBJECTSEXTPROC)gl("glCreateMemoryObjectsEXT"))
#define glMemoryObjectParameterivEXT ((PFNGLMEMORYOBJECTPARAMETERIVEXTPROC)gl("glMemoryObjectParameterivEXT"))
#define glImportMemoryFdEXT ((PFNGLIMPORTMEMORYFDEXTPROC)gl("glImportMemoryFdEXT"))
#define glTexStorageMem2DEXT ((PFNGLTEXSTORAGEMEM2DEXTPROC)gl("glTexStorageMem2DEXT"))
#define glGenSemaphoresEXT ((PFNGLGENSEMAPHORESEXTPROC)gl("glGenSemaphoresEXT"))
#define glImportSemaphoreFdEXT ((PFNGLIMPORTSEMAPHOREFDEXTPROC)gl("glImportSemaphoreFdEXT"))
#define glSignalSemaphoreEXT ((PFNGLSIGNALSEMAPHOREEXTPROC)gl("glSignalSemaphoreEXT"))

#define W 1024
#define H 512

static double now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// copies 2 MB n times from src to dst and gives MB/s
static double rate(void *dst, const void *src, size_t size, int n) {
	double t = now();
	for (int i = 0; i < n; i++)
		memcpy(dst, src, size);
	return size * (double)n / (1 << 20) / ((now() - t) / 1000.0);
}

int main(void) {
	VkInstance inst;
	VkPhysicalDevice pds[8], pd = VK_NULL_HANDLE;
	VkDevice dev;
	uint32_t n = 8, type = UINT32_MAX;
	const size_t size = (size_t)W * H * 4;
	const char *iext[] = { "VK_KHR_external_memory_capabilities", "VK_KHR_get_physical_device_properties2" };
	const char *dext[] = { "VK_KHR_external_memory", "VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf", "VK_KHR_external_semaphore_fd" };
	VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .enabledExtensionCount = 2, .ppEnabledExtensionNames = iext };
	int failures = 0;

	if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS) {
		printf("FAIL no vulkan instance\n");
		return 1;
	}
	vkEnumeratePhysicalDevices(inst, &n, pds);
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(pds[i], &p);
		if (p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
			pd = pds[i];
			printf("device %s\n", p.deviceName);
			break;
		}
	}
	if (pd == VK_NULL_HANDLE) {
		printf("FAIL no GPU device\n");
		return 1;
	}
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
		VkMemoryPropertyFlags f = mp.memoryTypes[i].propertyFlags;
		printf("memory type %u flags %#x\n", i, f);
		if (type == UINT32_MAX && (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) &&
		    (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
			type = i;
	}
	for (uint32_t i = 0; type == UINT32_MAX && i < mp.memoryTypeCount; i++)
		if ((mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
		    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			type = i;
	printf("using memory type %u\n", type);

	float prio = 1;
	VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
		.enabledExtensionCount = 4, .ppEnabledExtensionNames = dext };
	if (vkCreateDevice(pd, &dci, NULL, &dev) != VK_SUCCESS) {
		printf("FAIL no device with external memory fd\n");
		return 1;
	}

	VkExportMemoryAllocateInfo export = { .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
		.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT };
	VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &export, .allocationSize = size,
		.memoryTypeIndex = type };
	VkDeviceMemory mem;
	VkResult r = vkAllocateMemory(dev, &mai, NULL, &mem);
	printf("%s exportable host-visible allocation (%d)\n", r == VK_SUCCESS ? "ok  " : "FAIL", r);
	if (r != VK_SUCCESS)
		return 1;
	uint32_t *px;
	r = vkMapMemory(dev, mem, 0, size, 0, (void **)&px);
	printf("%s mapped (%d)\n", r == VK_SUCCESS ? "ok  " : "FAIL", r);
	if (r != VK_SUCCESS)
		return 1;

	void *ram = malloc(size);
	memset(ram, 0x40, size);
	printf("cpu write ram->venus %.0f MB/s, read venus->ram %.0f MB/s, ram->ram %.0f MB/s\n",
	    rate(px, ram, size, 50), rate(ram, px, size, 50), rate((char *)ram, (char *)ram + size / 2, size / 2, 100));

	PFN_vkGetMemoryFdKHR getfd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(dev, "vkGetMemoryFdKHR");
	VkMemoryGetFdInfoKHR gfi = { .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, .memory = mem,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT };
	int fd = -1;
	r = getfd ? getfd(dev, &gfi, &fd) : VK_ERROR_EXTENSION_NOT_PRESENT;
	printf("%s exported fd %d (%d)\n", r == VK_SUCCESS ? "ok  " : "FAIL", fd, r);
	if (r != VK_SUCCESS)
		return 1;

	// vulkan only: a second device imports a dup of the fd and fills a buffer on it, the first mapping must see it
	{
		VkDevice dev2;
		VkQueue q;
		int fd2 = dup(fd);
		if (vkCreateDevice(pd, &dci, NULL, &dev2) != VK_SUCCESS) {
			printf("FAIL second device\n");
			return 1;
		}
		vkGetDeviceQueue(dev2, 0, 0, &q);
		VkImportMemoryFdInfoKHR imp = { .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
			.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, .fd = fd2 };
		VkMemoryAllocateInfo mai2 = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &imp, .allocationSize = size,
			.memoryTypeIndex = type };
		VkDeviceMemory mem2;
		r = vkAllocateMemory(dev2, &mai2, NULL, &mem2);
		printf("%s vulkan import (%d)\n", r == VK_SUCCESS ? "ok  " : "FAIL", r);
		VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
		VkBuffer buf;
		vkCreateBuffer(dev2, &bci, NULL, &buf);
		vkBindBufferMemory(dev2, buf, mem2, 0);
		VkCommandPoolCreateInfo pci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		VkCommandPool pool;
		vkCreateCommandPool(dev2, &pci, NULL, &pool);
		VkCommandBufferAllocateInfo cai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
		VkCommandBuffer cb;
		vkAllocateCommandBuffers(dev2, &cai, &cb);
		VkCommandBufferBeginInfo cbi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer(cb, &cbi);
		vkCmdFillBuffer(cb, buf, 0, size, 0x12345678);
		vkEndCommandBuffer(cb);
		VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
		memset(px, 0, size);
		vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(q);
		printf("%s vulkan fill on the imported memory reaches the first mapping (%08x)\n",
		    px[1000] == 0x12345678u ? "ok  " : "FAIL", px[1000]);
		// and the importing device's own mapping
		uint32_t *own = NULL;
		vkMapMemory(dev2, mem2, 0, size, 0, (void **)&own);
		printf("importer's own mapping %08x\n", own ? own[1000] : 0);
	}

	// zink through CGL
	CGLPixelFormatAttribute attrs[] = { kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core, 0 };
	CGLPixelFormatObj pix;
	CGLContextObj ctx;
	GLint npix;
	CGLChoosePixelFormat(attrs, &pix, &npix);
	CGLCreateContext(pix, NULL, &ctx);
	CGLSetCurrentContext(ctx);
	printf("renderer %s\n", glGetString(GL_RENDERER));
	int has = 0;
	GLint next = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &next);
	for (GLint i = 0; i < next; i++)
		if (strcmp((const char *)glGetStringi(GL_EXTENSIONS, i), "GL_EXT_memory_object_fd") == 0)
			has = 1;
	printf("%s GL_EXT_memory_object_fd\n", has ? "ok  " : "FAIL");
	if (!has)
		return 1;

	GLuint gmem, tex, fbo;
	glCreateMemoryObjectsEXT(1, &gmem);
	GLint dedicated = GL_FALSE;
	glMemoryObjectParameterivEXT(gmem, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);
	glImportMemoryFdEXT(gmem, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
	GLenum e = glGetError();
	printf("%s imported into GL (%#x)\n", e == GL_NO_ERROR ? "ok  " : "FAIL", e);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_LINEAR_TILING_EXT);
	glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, getenv("ZC_RGBA") ? GL_RGBA8 : 0x93A1, W, H, gmem, 0); // 0x93A1 is GL_BGRA8_EXT
	e = glGetError();
	printf("%s linear texture on the memory (%#x)\n", e == GL_NO_ERROR ? "ok  " : "FAIL", e);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	printf("%s renderable (%#x)\n", st == GL_FRAMEBUFFER_COMPLETE ? "ok  " : "FAIL", st);

	// before rendering: does GL see what the CPU writes into the shared memory
	for (size_t i = 0; i < (size_t)W * H; i++)
		px[i] = 0xff8040c0u;
	{
		uint32_t seen = 0;
		glReadPixels(10, 100, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &seen);
		printf("%s GL reads the CPU's pattern through the import (%08x)\n", seen == 0xff8040c0u ? "ok  " : "FAIL", seen);
	}
	// left half green, right half blue, then read through the CPU mapping
	memset(px, 0, size);
	glViewport(0, 0, W, H);
	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, W / 2, H);
	glClearColor(0, 1, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	// a drawn quad over the left half as well, a clear alone may only touch compression metadata
	{
		static const char *vs = "#version 150\nin vec2 p;\nvoid main(){gl_Position=vec4(p,0,1);}\n";
		static const char *fs = "#version 150\nout vec4 c;\nvoid main(){c=vec4(0,1,0,1);}\n";
		static const float q[] = { -1, -1, 0, -1, -1, 1, 0, -1, 0, 1, -1, 1 };
		GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER), prog = glCreateProgram(), vao, vbo;
		glShaderSource(v, 1, &vs, NULL), glCompileShader(v);
		glShaderSource(f, 1, &fs, NULL), glCompileShader(f);
		glAttachShader(prog, v), glAttachShader(prog, f), glBindAttribLocation(prog, 0, "p"), glLinkProgram(prog), glUseProgram(prog);
		glGenVertexArrays(1, &vao), glBindVertexArray(vao);
		glGenBuffers(1, &vbo), glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0), glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	double t = now();
	glFinish();
	double wait = now() - t;
	// BGRA8 in memory: B G R A bytes, so green is 0xff00ff00 and blue 0xff0000ff read as little-endian words
	uint32_t a = px[100 * W + 10], b = px[100 * W + W - 10];
	size_t nonzero = 0, first = 0;
	for (size_t i = 0; i < (size_t)W * H; i++)
		if (px[i] != 0 && nonzero++ == 0)
			first = i;
	uint32_t gp[2] = { 0, 0 };
	glReadPixels(10, 100, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &gp[0]);
	glReadPixels(W - 10, 100, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &gp[1]);
	printf("mapping has %zu non-zero words, first at %zu (%08x); GL reads %08x %08x\n", nonzero, first, px[first], gp[0], gp[1]);
	// a second mapping of the same memory, in case the first one is stale
	uint32_t *again = NULL;
	vkUnmapMemory(dev, mem);
	vkMapMemory(dev, mem, 0, size, 0, (void **)&again);
	printf("remapped: %08x %08x\n", again ? again[100 * W + 10] : 0, again ? again[100 * W + W - 10] : 0);
	px = again ? again : px;
	int ok = a == 0xff00ff00u && b == (getenv("ZC_RGBA") ? 0xffff0000u : 0xff0000ffu);
	printf("%s GPU rendering visible in the CPU mapping, pitch %d (%08x %08x), finish %.2f ms\n", ok ? "ok  " : "FAIL", W * 4, a, b, wait);
	failures += !ok;

	// the per-frame cost the copy-back has today: read 2 MB from the mapping
	double read = rate(ram, px, size, 20);
	printf("reading a tile from the mapping %.2f ms\n", size / (read * (1 << 20)) * 1000);

	printf("%s zc-spike, %d failures\n", failures ? "FAIL" : "PASS", failures);
	return failures != 0;
}
