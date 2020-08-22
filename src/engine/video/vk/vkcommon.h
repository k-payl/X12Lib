#pragma once
#include "common.h"
#include "icorerender.h"

#define VK_USE_PLATFORM_WIN32_KHR

#if _DEBUG
	#define VK_CHECK(call) \
			assert(call == VK_SUCCESS);
#else
	#define VK_CHECK(call) call
#endif

namespace x12
{
	struct VkWindowSurface : public IWidowSurface
	{
		VkSurfaceKHR				surface{ VK_NULL_HANDLE };
		VkSwapchainKHR				swapchain{ VK_NULL_HANDLE };
		VkExtent2D					swapchainExtent{};
		VkFormat					swapchainFormat{};
		std::vector<VkImageView>	swapChainImageViews;
		std::vector<VkImage>		swapChainImages;
		HWND						hwnd;
		uint32_t					presentFamilyIndex;
		VkSemaphore					waitSubmitedSempahore;
		uint32_t					swapchainFrameIndex;

		~VkWindowSurface();

		void destroySwapchainObjects();
		void Init(HWND hwnd, ICoreRenderer* render) override;
		void ResizeBuffers(unsigned width_, unsigned height_) override;
		void Present() override;
		void* GetNativeResource(int i) override;

		void UpdateSubmitedSemaphore(VkSemaphore sem)
		{
			waitSubmitedSempahore = sem;
		}
	};

	namespace vk
	{
		VkInstance createInstance();

		uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice);
		uint32_t getPresentFamilyIndex(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);


		VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount);

		VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice);

		VkSurfaceKHR createSurface(VkInstance instance, HWND window);

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);
		//struct Swapchain
		//{
		//	VkSwapchainKHR swapchain;
		//	VkFormat format;
		//	std::vector<VkImage> images;
		//	VkExtent2D extent;
		//};

		VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
		//VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR surfaceCaps, uint32_t familyIndex, VkFormat format, uint32_t width, uint32_t height, uint32_t images, VkSwapchainKHR oldSwapchain);
	}
}
