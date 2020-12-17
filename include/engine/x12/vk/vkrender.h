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

		VkGraphicCommandList* graphicCommandContext;

		std::map<HWND, surface_ptr> surfaces;
		std::vector<surface_ptr> surfacesForPresenting;

		CoreRenderStat stat;

		VmaAllocator allocator{};

	public:
		VkCoreRenderer();
		~VkCoreRenderer();

		VkInstance Instance() const { return instance; }
		VkPhysicalDevice PhysicalDevice() const { return physicalDevice; }
		VkDevice Device() const { return device; }
		VkCommandPool CommandPool() const { return commandPool; }
		VkQueue GraphicQueue() const { return graphicsQueue; }
		const uint32_t& FamilyIndex() const { return familyIndex; }
		VmaAllocator& Allocator() { return allocator; }

		auto Init() -> void override;
		auto Free() -> void override;

		auto GetGraphicCommandList()->ICoreGraphicCommandList* override;
		auto GetGraphicCommandList(int32_t id)->ICoreGraphicCommandList* override;
		auto GetCopyCommandContext()->ICoreCopyCommandList* override;

		// Surfaces
		auto _FetchSurface(HWND hwnd)->surface_ptr override;
		auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void override;
		auto GetWindowSurface(HWND hwnd)->surface_ptr override;
		auto PresentSurfaces() -> void override;
		auto FrameEnd() -> void override {};

		auto ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void override;
		auto WaitGPU() -> void override;
		auto WaitGPUAll() -> void override;

		auto GetStat(CoreRenderStat& stat_) -> void override { stat_ = stat; }

		auto IsVSync() -> bool override { return false; } // TODO

		// Resurces
		bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
								  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
										 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
										const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem) override;

		bool CreateBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags, MEMORY_TYPE mem, const void* data, size_t num) override;

		bool CreateTexture(ICoreTexture** out, LPCWSTR name, const uint8_t* data, size_t size, int32_t width, int32_t height, uint32_t mipCount,
						   TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags) override;

		bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, ID3D12Resource* d3dexistingtexture) override;
		bool CreateTextureFrom(x12::ICoreTexture**, LPCWSTR, std::vector<D3D12_SUBRESOURCE_DATA, std::allocator<D3D12_SUBRESOURCE_DATA>>, ID3D12Resource*) override;
		bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) override;

		bool CreateQuery(ICoreQuery** out) override;

		void* GetNativeDevice() override { return device; }

		void* GetNativeGraphicQueue() override { return nullptr; }
	};

	namespace vk
	{
		inline VkCoreRenderer* VkGetCoreRender() { return static_cast<VkCoreRenderer*>(_coreRender); }
		inline VkDevice GetDevice() { return VkGetCoreRender()->Device(); }
		inline VkPhysicalDevice GetPhysicalDevice() { return VkGetCoreRender()->PhysicalDevice(); }
		inline VkInstance GetInstance() { return VkGetCoreRender()->Instance(); }
		inline VkCommandPool GetCommandPool() { return VkGetCoreRender()->CommandPool(); }
		inline VkQueue GetGraphicQueue() { return VkGetCoreRender()->GraphicQueue(); }
		inline VmaAllocator& GetAllocator() { return VkGetCoreRender()->Allocator(); }
	}
}
