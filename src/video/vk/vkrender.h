#pragma once
#include "vkcommon.h"

namespace x12
{
	class VkCoreRenderer : public ICoreRenderer
	{
		VkInstance				instance{ VK_NULL_HANDLE };
		VkPhysicalDevice		physicalDevice{ VK_NULL_HANDLE };
		VkDevice				device{ VK_NULL_HANDLE };

		uint32_t				familyIndex{0};
		VkQueue					graphicsQueue{ VK_NULL_HANDLE };
		//VkQueue					presentQueue{ VK_NULL_HANDLE };

		//int						currentFrame{ 0 };

		VkCommandPool			commandPool{ VK_NULL_HANDLE };
		//std::vector<VkCommandBuffer>	commandBuffers;

		//VkSurfaceKHR			surface{ VK_NULL_HANDLE };
		//VkSwapchainKHR			swapchain{ VK_NULL_HANDLE };
		//VkExtent2D				swapchainExtent{};
		//VkFormat				swapchainFormat{};
		//std::vector<VkImageView>		swapChainImageViews;
		//std::vector<VkImage>			swapChainImages;

		//std::vector<VkFence>			fencesRenderFinished;

		//std::vector<VkSemaphore>		samaphoresImageAvailable;
		//std::vector<VkSemaphore>		semaphoresRenderFinished;

		VkGraphicCommandContext* graphicCommandContext;

		std::map<HWND, surface_ptr> surfaces;
		std::vector<surface_ptr> surfacesForPresenting;

		CoreRenderStat stat;

	public:
		VkCoreRenderer();
		~VkCoreRenderer();

		VkInstance Instance() const { return instance; }
		VkPhysicalDevice PhysicalDevice() const { return physicalDevice; }
		VkDevice Device() const { return device; }
		VkCommandPool CommandPool() const { return commandPool; }
		VkQueue GraphicQueue() const { return graphicsQueue; }
		const uint32_t& FamilyIndex() const { return familyIndex; }

		auto Init() -> void override;
		auto Free() -> void override;
		auto GetGraphicCommandContext()->ICoreGraphicCommandList* override;
		auto GetCopyCommandContext()->ICoreCopyCommandList* override;

		// Surfaces
		auto _FetchSurface(HWND hwnd)->surface_ptr override;
		auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void override;
		auto GetWindowSurface(HWND hwnd)->surface_ptr override;
		auto PresentSurfaces() -> void override;

		auto GetStat(CoreRenderStat& stat_) -> void override { stat_ = stat; }

		auto IsVSync() -> bool override { return false; } // TODO

		// Resurces
		bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
								  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
										 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
										const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS flags = BUFFER_FLAGS::GPU_READ) override;

		bool CreateConstantBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, bool FastGPUread = false) override;

		bool CreateStructuredBuffer(ICoreBuffer** out, LPCWSTR name, size_t structureSize, size_t num,
											const void* data = nullptr, BUFFER_FLAGS flags = BUFFER_FLAGS::NONE) override;

		bool CreateRawBuffer(ICoreBuffer** out, LPCWSTR name, size_t size) override;

		//bool CreateTexture(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> data, int32_t width, int32_t height,
		//				   TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags) override;

		bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
									   std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture) override;

		bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) override;
	};

	namespace vk
	{
		inline VkCoreRenderer* VkGetCoreRender() { return static_cast<VkCoreRenderer*>(_coreRender); }
		inline VkDevice GetDevice() { return VkGetCoreRender()->Device(); }
		inline VkPhysicalDevice GetPhysicalDevice() { return VkGetCoreRender()->PhysicalDevice(); }
		inline VkInstance GetInstance() { return VkGetCoreRender()->Instance(); }
		inline VkCommandPool GetCommandPool() { return VkGetCoreRender()->CommandPool(); }
		inline VkQueue GetGraphicQueue() { return VkGetCoreRender()->GraphicQueue(); }
	}
}
