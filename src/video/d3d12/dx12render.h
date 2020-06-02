#pragma once
#include "common.h"
#include "icorerender.h"
#include "intrusiveptr.h"
#include "dx12common.h"
#include "dx12memory.h"

namespace x12
{
	using psomap_t = std::map<psomap_checksum_t, ComPtr<ID3D12PipelineState>>;

	class Dx12CoreRenderer : public ICoreRenderer
	{
		device_t* device{nullptr};
		adapter_t* adapter{nullptr};

		std::map<HWND, surface_ptr> surfaces;
		std::vector<surface_ptr> surfacesForPresenting;

		Dx12GraphicCommandContext* graphicCommandContext;
		Dx12CopyCommandContext* copyCommandContext;

		std::mutex psoMutex;
		psomap_t psoMap;										// All Pipeline State Objects. checksum -> PSO
		uint64_t psoNum{};

		x12::descriptorheap::Allocator* descriptorAllocator;	// Descriptors for static long-lived resources
		ComPtr<ID3D12RootSignature> defaultRootSignature;		// Root signature for shaders without input resources
		bool tearingSupported;
		UINT descriptorSizeCBSRV;
		UINT descriptorSizeRTV;
		UINT descriptorSizeDSV;

		CoreRenderStat stat{};

		void ReleaseFrame(uint64_t fenceID);
		static void sReleaseFrameCallback(uint64_t fenceID);

	public:
		Dx12CoreRenderer();
		~Dx12CoreRenderer();

		// dx12 specific
		auto GetDevice() -> device_t* { return device; }
		auto GetGraphicPSO(const GraphicPipelineState& pso, psomap_checksum_t checksum)->ComPtr<ID3D12PipelineState>;
		auto GetComputePSO(const ComputePipelineState& pso, psomap_checksum_t checksum)->ComPtr<ID3D12PipelineState>;
		auto AllocateDescriptor(UINT num = 1)->x12::descriptorheap::Alloc;
		auto CBSRV_DescriptorsSize() -> UINT { return descriptorSizeCBSRV; }
		auto RTV_DescriptorsSize() -> UINT { return descriptorSizeRTV; }
		auto DSV_DescriptorsSize() -> UINT { return descriptorSizeDSV; }
		auto IsTearingSupport() -> bool { return tearingSupported; }
		auto GetDefaultRootSignature()->ComPtr<ID3D12RootSignature>;

		// API
		auto Init() -> void override;
		auto Free() -> void override;
		auto GetGraphicCommandContext() -> ICoreGraphicCommandList* override { return (ICoreGraphicCommandList*)graphicCommandContext; };
		auto GetCopyCommandContext() -> ICoreCopyCommandList* override { return (ICoreCopyCommandList*)copyCommandContext; }
		auto _FetchSurface(HWND hwnd) -> surface_ptr override;
		auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void override;
		auto GetWindowSurface(HWND hwnd) -> surface_ptr override;
		auto PresentSurfaces() -> void override;

		auto GetStat(CoreRenderStat& stat_) -> void override {stat_ = stat; };

		auto IsVSync() -> bool  override { return Vsync; }

		bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
						  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
								 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
								const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS flags = BUFFER_FLAGS::GPU_READ) override;

		bool CreateConstantBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, bool FastGPUread = false) override;

		bool CreateStructuredBuffer(ICoreBuffer** out, LPCWSTR name, size_t structureSize, size_t num,
									const void* data = nullptr, BUFFER_FLAGS flags = BUFFER_FLAGS::NONE) override;

		bool CreateRawBuffer(ICoreBuffer** out, LPCWSTR name, size_t size);

		bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
							   std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture) override;

		bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) override;
	};


	// Helpers

	namespace d3d12
	{
		inline Dx12CoreRenderer* D3D12GetCoreRender() { return static_cast<Dx12CoreRenderer*>(_coreRender); }

		inline device_t* CR_GetD3DDevice() { return d3d12::D3D12GetCoreRender()->GetDevice(); }
		inline bool							CR_IsTearingSupport() { return d3d12::D3D12GetCoreRender()->IsTearingSupport(); }
		inline bool							CR_IsVSync() { return GetCoreRender()->IsVSync(); }
		inline UINT							CR_CBSRV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->CBSRV_DescriptorsSize(); }
		inline UINT							CR_RTV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->RTV_DescriptorsSize(); }
		inline UINT							CR_DSV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->DSV_DescriptorsSize(); }
	}
}
