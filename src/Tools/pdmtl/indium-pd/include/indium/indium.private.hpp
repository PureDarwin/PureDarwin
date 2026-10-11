// indium-pd's stand-in for Indium's private header: Indium's tests take the VkInstance from here to
// make their window surfaces. Indium (darlinghq/indium) is 0BSD, this file is ours
#pragma once

#include <indium/indium.hpp>
#include <vulkan/vulkan.h>

namespace Indium {
	// the executor's instance, valid after init() was given the window system's extensions
	extern VkInstance globalInstance;
	// and how to reach its functions: the executor's own loader or driver, never whichever libvulkan an app brought
	extern PFN_vkGetInstanceProcAddr globalGetInstanceProcAddr;
};
