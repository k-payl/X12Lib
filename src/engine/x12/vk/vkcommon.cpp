#include "pch.h"
#include "vkcommon.h"
#include "vkrender.h"

using namespace x12;

VkInstance x12::vk::createInstance()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const VkLayerProperties& layerProperties : availableLayers)
	{
		//std::count << layerProperties.layerName << std::endl;
	}

	VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.pApplicationInfo = &appInfo;

#ifdef _DEBUG
	const char* debugLayers[] =
	{
		"VK_LAYER_KHRONOS_validation",
		//"VK_LAYER_LUNARG_api_dump"
	};

	createInfo.ppEnabledLayerNames = debugLayers;
	createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
#endif

	const char* extensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef VK_USE_PLATFORM_WIN32_KHR
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif

#ifdef _DEBUG
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	};

	createInfo.ppEnabledExtensionNames = extensions;
	createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

	VkInstance instance = 0;
	VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

	return instance;
}

uint32_t x12::vk::getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice)
{
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, 0);

	std::vector<VkQueueFamilyProperties> queues(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

	for (uint32_t i = 0; i < queueCount; ++i)
		if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			return i;

	return VK_QUEUE_FAMILY_IGNORED;
}

uint32_t x12::vk::getPresentFamilyIndex(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, 0);

	std::vector<VkQueueFamilyProperties> queues(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

	for (uint32_t i = 0; i < queueCount; ++i)
	{
		auto& queueFamily = queues[i];

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) {
			return i;
		}
	}

	return VK_QUEUE_FAMILY_IGNORED;
}

static bool supportsPresentation(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	return !!vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, familyIndex);
#else
	return true;
#endif
}

uint32_t x12::vk::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

VkPhysicalDevice x12::vk::pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount)
{
	VkPhysicalDevice preferred = 0;
	VkPhysicalDevice fallback = 0;

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		//
		// Get all caps
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

		printf("GPU%d: %s\n", i, props.deviceName);

		uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevices[i]);
		if (familyIndex == VK_QUEUE_FAMILY_IGNORED)
			continue;

		if (!supportsPresentation(physicalDevices[i], familyIndex))
			continue;

		if (props.apiVersion < VK_API_VERSION_1_1)
			continue;

		if (!preferred && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			preferred = physicalDevices[i];
		}

		if (!fallback)
		{
			fallback = physicalDevices[i];
		}
	}

	VkPhysicalDevice result = preferred ? preferred : fallback;

	if (result)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(result, &props);

		printf("Selected GPU %s\n", props.deviceName);
	}
	else
	{
		printf("ERROR: No GPUs found\n");
	}

	return result;
}

VkDevice x12::vk::createDevice(VkInstance instance, VkPhysicalDevice physicalDevice)
{
	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceInfo.queueCreateInfoCount = 1;

	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = getGraphicsFamilyIndex(physicalDevice);
	queueInfo.queueCount = 1;
	float queuePriorities[] = { 1.0f };
	queueInfo.pQueuePriorities = queuePriorities;

	deviceInfo.pQueueCreateInfos = &queueInfo;

	std::vector<const char*> extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	deviceInfo.ppEnabledExtensionNames = extensions.data();
	deviceInfo.enabledExtensionCount = uint32_t(extensions.size());

	VkDevice device = 0;
	VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, 0, &device));

	return device;
}

VkSurfaceKHR vk::createSurface(VkInstance instance, HWND window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	createInfo.hinstance = GetModuleHandle(0);
	createInfo.hwnd = window;

	VkSurfaceKHR surface = 0;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, 0, &surface));
	return surface;
#else
	#error Unsupported platform
#endif
}

VkFormat x12::vk::getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	uint32_t formatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

	if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		return VK_FORMAT_R8G8B8A8_UNORM;

	for (uint32_t i = 0; i < formatCount; ++i)
		if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			return formats[i].format;

	return formats[0].format;
}

void x12::VkWindowSurface::Init(HWND hwnd_, ICoreRenderer* render)
{
	hwnd = hwnd_;

	RECT r;
	GetClientRect(hwnd_, &r);
	width = r.right - r.left;
	height = r.bottom - r.top;

	ResizeBuffers(width, height);

	presentFamilyIndex = vk::getPresentFamilyIndex(vk::GetPhysicalDevice(), surface);
	assert(presentFamilyIndex != VK_QUEUE_FAMILY_IGNORED);
	//assert(presentFamilyIndex == familyIndex); // check two families are the same. TODO: support different queues
}

x12::VkWindowSurface::~VkWindowSurface()
{
	destroySwapchainObjects();
	vkDestroySurfaceKHR(vk::GetInstance(), surface, nullptr);
}

void x12::VkWindowSurface::destroySwapchainObjects()
{
	for (auto imageView : swapChainImageViews)
		vkDestroyImageView(vk::GetDevice(), imageView, nullptr);
	swapChainImageViews.clear();

	if (swapchain)
		vkDestroySwapchainKHR(vk::GetDevice(), swapchain, nullptr);

	swapchain = VK_NULL_HANDLE;
}

void x12::VkWindowSurface::ResizeBuffers(unsigned width_, unsigned height_)
{
	destroySwapchainObjects();

	VkCoreRenderer* renderer = vk::VkGetCoreRender();

	surface = vk::createSurface(vk::GetInstance(), hwnd);
	assert(surface);

	VkSurfaceCapabilitiesKHR surfaceCaps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->PhysicalDevice(), surface, &surfaceCaps));

	uint32_t width = surfaceCaps.currentExtent.width;
	uint32_t height = surfaceCaps.currentExtent.height;
	swapchainExtent = surfaceCaps.currentExtent;

	swapchainFormat = vk::getSwapchainFormat(renderer->PhysicalDevice(), surface);

	VkCompositeAlphaFlagBitsKHR surfaceComposite =
		(surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = surface;
	createInfo.minImageCount = engine::DeferredBuffers > surfaceCaps.minImageCount ? engine::DeferredBuffers : surfaceCaps.minImageCount;
	createInfo.imageFormat = swapchainFormat;
	createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	createInfo.imageExtent.width = width;
	createInfo.imageExtent.height = height;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.queueFamilyIndexCount = 1;
	createInfo.pQueueFamilyIndices = &renderer->FamilyIndex();
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	createInfo.compositeAlpha = surfaceComposite;
	createInfo.presentMode = GetCoreRender()->IsVSync() ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
	createInfo.oldSwapchain = 0;

	VK_CHECK(vkCreateSwapchainKHR(vk::GetDevice(), &createInfo, 0, &swapchain));
	assert(swapchain);

	uint32_t imageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(vk::GetDevice(), swapchain, &imageCount, 0));

	swapChainImages.resize(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(vk::GetDevice(), swapchain, &imageCount, swapChainImages.data()));

	swapChainImageViews.resize(engine::DeferredBuffers);
	for (size_t i = 0; i < engine::DeferredBuffers; i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(renderer->Device(), &createInfo, nullptr, &swapChainImageViews[i]));
	}
}

void x12::VkWindowSurface::Present()
{
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &waitSubmitedSempahore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &swapchainFrameIndex;

	VkResult present = vkQueuePresentKHR(vk::GetGraphicQueue(), &presentInfo);

	if (present != VK_SUCCESS/* || framebufferResize*/)
	{
		//framebufferResize = false;


		RECT r;
		GetClientRect(hwnd, &r);
		width = r.right - r.left;
		height = r.bottom - r.top;

		ResizeBuffers(width, height);
	}

	waitSubmitedSempahore = VK_NULL_HANDLE;
}

void* x12::VkWindowSurface::GetNativeResource(int i)
{
	return swapChainImages[i];
}
