// vnprobe: a venus icd loaded directly (no loader, as airjitd does with AIRJITD_VK_ICD): instance, devices,
// each device's name and api version, timings of each step
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vulkan/vulkan.h>

static double
now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int
main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/usr/local/lib/pdvk/libvulkan_virtio.dylib";
	typedef VkResult (*negotiate_fn)(uint32_t *);
	PFN_vkGetInstanceProcAddr gipa;
	uint32_t ver = 5, n = 16;
	VkPhysicalDevice pd[16];
	VkInstance inst;
	double t0 = now_ms();
	void *lib;

	setvbuf(stdout, NULL, _IONBF, 0);
	if (!(lib = dlopen(path, RTLD_NOW | RTLD_LOCAL))) {
		printf("vnprobe: dlopen %s: %s\nFAIL\n", path, dlerror());
		return 1;
	}
	negotiate_fn neg = (negotiate_fn)dlsym(lib, "vk_icdNegotiateLoaderICDInterfaceVersion");
	if (neg)
		neg(&ver);
	gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
	if (!gipa) {
		printf("vnprobe: no vk_icdGetInstanceProcAddr\nFAIL\n");
		return 1;
	}

	PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "vnprobe", 1, NULL, 0, VK_API_VERSION_1_2 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app };
	VkResult r = ci(&ici, NULL, &inst);
	printf("vnprobe: vkCreateInstance %d after %.1f ms\n", r, now_ms() - t0);
	if (r != VK_SUCCESS) {
		printf("FAIL\n");
		return 1;
	}

	PFN_vkEnumeratePhysicalDevices epd = (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
	PFN_vkGetPhysicalDeviceProperties gpp = (PFN_vkGetPhysicalDeviceProperties)gipa(inst, "vkGetPhysicalDeviceProperties");
	r = epd(inst, &n, pd);
	printf("vnprobe: vkEnumeratePhysicalDevices %d, %u devices after %.1f ms\n", r, n, now_ms() - t0);
	for (uint32_t i = 0; r >= 0 && i < n; i++) {
		VkPhysicalDeviceProperties p;

		gpp(pd[i], &p);
		printf("vnprobe: device %u %s api %u.%u.%u\n", i, p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
		    VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion));
	}
	if (r < 0 || !n) {
		printf("FAIL\n");
		return 1;
	}

	// device 0, then one host visible allocation mapped, written and read back
	PFN_vkGetPhysicalDeviceMemoryProperties gmp =
	    (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(inst, "vkGetPhysicalDeviceMemoryProperties");
	PFN_vkCreateDevice cd = (PFN_vkCreateDevice)gipa(inst, "vkCreateDevice");
	PFN_vkGetDeviceProcAddr gdpa = (PFN_vkGetDeviceProcAddr)gipa(inst, "vkGetDeviceProcAddr");
	VkPhysicalDeviceMemoryProperties mp;
	float prio = 1;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci };
	VkDevice dev;

	gmp(pd[0], &mp);
	r = cd(pd[0], &dci, NULL, &dev);
	printf("vnprobe: vkCreateDevice %d after %.1f ms\n", r, now_ms() - t0);
	if (r != VK_SUCCESS) {
		printf("FAIL\n");
		return 1;
	}
	PFN_vkAllocateMemory am = (PFN_vkAllocateMemory)gdpa(dev, "vkAllocateMemory");
	PFN_vkMapMemory mm = (PFN_vkMapMemory)gdpa(dev, "vkMapMemory");
	int bad = 0;
	for (uint32_t t = 0; t < mp.memoryTypeCount; t++) {
		VkMemoryPropertyFlags f = mp.memoryTypes[t].propertyFlags;
		VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, 65536, t };
		VkDeviceMemory mem;
		volatile uint32_t *p = NULL;

		if (!(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
			continue;
		r = am(dev, &mai, NULL, &mem);
		VkResult r2 = r == VK_SUCCESS ? mm(dev, mem, 0, 65536, 0, (void **)&p) : r;
		int ok = r2 == VK_SUCCESS && p;
		if (ok) {
			for (uint32_t i = 0; i < 16384; i++)
				p[i] = i * 2654435761u;
			for (uint32_t i = 0; i < 16384; i++)
				ok &= p[i] == i * 2654435761u;
		}
		printf("vnprobe: memory type %u flags 0x%x heap %u: allocate %d map %d rw %s\n", t, f,
		    mp.memoryTypes[t].heapIndex, r, r2, ok ? "ok" : "BAD");
		bad |= !ok;
	}
	printf(bad ? "FAIL\n" : "PASS\n");
	return bad;
}
