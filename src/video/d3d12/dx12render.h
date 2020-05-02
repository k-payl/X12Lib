#pragma once
#include "common.h"
#include "icorerender.h"
#include "intrusiveptr.h"
#include "dx12common.h"
#include "dx12memory.h"

namespace x12
{
	using psomap_t = std::map<psomap_checksum_t, ComPtr<ID3D12PipelineState>> ;

	class Dx12CoreRenderer
	{
		device_t *device{nullptr};
		adapter_t *adapter{nullptr};

		uint64_t frame{};

		std::map<HWND, surface_ptr> surfaces;
		std::vector<surface_ptr> surfacesForPresenting;

		Dx12GraphicCommandContext* graphicCommandContext;
		Dx12CopyCommandContext* copyCommandContext;

		std::mutex psoMutex;
		psomap_t psoMap;										// All Pipeline State Objects. checksum -> PSO
		uint64_t psoNum{};

		x12::descriptorheap::Allocator* descriptorAllocator;	// Descriptors for static long-lived resources
		ComPtr<ID3D12RootSignature> defaultRootSignature;		// Root signature for shaders without input resources
		bool Vsync{false};
		bool tearingSupported;
		UINT descriptorSizeCBSRV;
		UINT descriptorSizeRTV;
		UINT descriptorSizeDSV;

		void ReleaseFrame(uint64_t fenceID);
		static void sReleaseFrameCallback(uint64_t fenceID);

	public:
		Dx12CoreRenderer();
		~Dx12CoreRenderer();
	
		auto Init() -> void;
		auto Free() -> void;

		auto GetDevice() -> device_t* { return device; }
		auto GetGraphicPSO(const GraphicPipelineState& pso, psomap_checksum_t checksum) -> ComPtr<ID3D12PipelineState>;
		auto GetComputePSO(const ComputePipelineState& pso, psomap_checksum_t checksum) -> ComPtr<ID3D12PipelineState>;
		auto AllocateDescriptor(UINT num = 1) -> x12::descriptorheap::Alloc;

		// Contexts
		auto GetGraphicCommmandContext() const -> Dx12GraphicCommandContext* { return graphicCommandContext; };
		auto GetCopyCommandContext() const -> Dx12CopyCommandContext* { return copyCommandContext; }

		// Surfaces
		auto _FetchSurface(HWND hwnd) -> surface_ptr;
		auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void;
		auto GetWindowSurface(HWND hwnd) -> surface_ptr;
		auto PresentSurfaces() -> void;

		// Statistic
		auto UniformBufferUpdates()->uint64_t;
		auto StateChanges()->uint64_t;
		auto Triangles()->uint64_t;
		auto DrawCalls()->uint64_t;
		size_t committedMemory{};     // Bytes of memory currently committed/in-flight
		size_t totalMemory{};         // Total bytes of memory used by the allocators
		size_t totalPages{};          // Total page count
		size_t peakCommitedMemory{};  // Peak commited memory value since last reset
		size_t peakTotalMemory{};     // Peak total bytes
		size_t peakTotalPages{};      // Peak total page count

		// Constants
		auto CBSRV_DescriptorsSize() -> UINT { return descriptorSizeCBSRV; }
		auto RTV_DescriptorsSize() -> UINT { return descriptorSizeRTV; }
		auto DSV_DescriptorsSize() -> UINT { return descriptorSizeDSV; }
		auto IsTearingSupport() -> bool { return tearingSupported; }
		auto IsVSync() -> bool { return Vsync; }
		auto GetDefaultRootSignature() -> ComPtr<ID3D12RootSignature>;

		// Resurces
		bool CreateShader(ICoreShader **out, LPCWSTR name, const char* vertText, const char* fragText,
								const ConstantBuffersDesc *variabledesc = nullptr, uint32_t varNum = 0);

		bool CreateComputeShader(ICoreShader **out, LPCWSTR name, const char* text,
								const ConstantBuffersDesc *variabledesc = nullptr, uint32_t varNum = 0);

		bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
								const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS flags = BUFFER_FLAGS::GPU_READ);

		bool CreateConstantBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, bool FastGPUread = false);

		bool CreateStructuredBuffer(ICoreBuffer**out, LPCWSTR name, size_t structureSize, size_t num,
									const void* data = nullptr, BUFFER_FLAGS flags = BUFFER_FLAGS::NONE);

		bool CreateRawBuffer(Dx12CoreBuffer **out, LPCWSTR name, size_t size);

		bool CreateTexture(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> data, int32_t width, int32_t height,
						   TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags);

		bool CreateTextureFrom(ICoreTexture **out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
						   std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture);

		bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader);
	};


	// helpers

	extern Dx12CoreRenderer*			_coreRender;
	inline Dx12CoreRenderer*			GetCoreRender() { return _coreRender; }

	inline device_t*					CR_GetD3DDevice() { return GetCoreRender()->GetDevice(); }
	inline bool							CR_IsTearingSupport() { return GetCoreRender()->IsTearingSupport(); }
	inline bool							CR_IsVSync() { return GetCoreRender()->IsVSync(); }
	inline UINT							CR_CBSRV_DescriptorsSize() { return GetCoreRender()->CBSRV_DescriptorsSize(); }
	inline UINT							CR_RTV_DescriptorsSize() { return GetCoreRender()->RTV_DescriptorsSize(); }
	inline UINT							CR_DSV_DescriptorsSize() { return GetCoreRender()->DSV_DescriptorsSize(); }
}
