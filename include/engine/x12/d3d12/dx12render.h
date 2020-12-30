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

		enum
		{
			QUEUE_GRAPHIC,
			QUEUE_COPY,
			QUEUE_NUM,
		};

		struct Queue
		{
			ID3D12CommandQueue* d3dCommandQueue{};
			ID3D12Fence* d3dFence{};
			HANDLE fenceEvent{};
			uint64_t nextFenceValue{1}; // fence value ready to signal
			uint64_t submitedValues[engine::DeferredBuffers]{};
			uint64_t completedValue{};

		private:
			uint64_t Signal(ID3D12CommandQueue* d3dCommandQueue, ID3D12Fence* d3dFence, uint64_t& nextFenceValue)
			{
				uint64_t fenceValueForSignal = nextFenceValue;
				throwIfFailed(d3dCommandQueue->Signal(d3dFence, fenceValueForSignal));
				uint64_t queueCompleted = d3dFence->GetCompletedValue();
				++nextFenceValue;
				return fenceValueForSignal;
			}

		public:
			void Signal(UINT frameIndex)
			{
				submitedValues[frameIndex] = nextFenceValue;
				Signal(d3dCommandQueue, d3dFence, nextFenceValue);
			}

			bool IsCompleted(uint64_t value)
			{
				return value <= completedValue;
			}

		} queues[QUEUE_NUM];

		std::map<HWND, surface_ptr> surfaces;
		std::set<surface_ptr> surfacesForPresenting;

		std::vector<Dx12GraphicCommandList*> commandLists;

		Dx12CopyCommandList* copyCommandContext;
		std::unique_ptr<DirectX::GraphicsMemory> frameMemory;

		std::mutex psoMutex;
		psomap_t psoMap; // All Pipeline State Objects. checksum -> PSO
		uint64_t psoNum{};

		x12::descriptorheap::Allocator* SRVdescriptorAllocator; // Pre created descriptors for long-lived resources. Keeps a lot of ID3D12DescriptorHeap's
		x12::descriptorheap::Allocator* RTVdescriptorAllocator; // Pre created descriptors for long-lived resources. Keeps a lot of ID3D12DescriptorHeap's
		x12::descriptorheap::Allocator* DSVdescriptorAllocator; // Pre created descriptors for long-lived resources. Keeps a lot of ID3D12DescriptorHeap's

		struct DescriptorHeap
		{
			ComPtr<ID3D12DescriptorHeap> heap; // Descriptor heap using at rendering. TODO: make deallocation
			UINT offset_{};
			D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{};
			D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{};

			void Init(D3D12_DESCRIPTOR_HEAP_TYPE type, bool gpuVisible);
			void Free()
			{
				heap = nullptr;
			}
		};

		DescriptorHeap srv;

		ComPtr<ID3D12RootSignature> defaultRootSignature; // Root signature for shaders without input resources

		UINT descriptorSizeCBSRV;
		UINT descriptorSizeRTV;
		UINT descriptorSizeDSV;

		bool tearingSupported;
		float gpuTickDelta{};
		CoreRenderStat stat{};

		ICoreGraphicCommandList* CreateCommandList(int32_t id);

	public:
		Dx12CoreRenderer();
		~Dx12CoreRenderer();

		// API
		X12_API auto Init() -> void override;
		X12_API auto Free() -> void override;

		X12_API auto GetGraphicCommandList()->ICoreGraphicCommandList* override;
		X12_API auto GetGraphicCommandList(int32_t id)->ICoreGraphicCommandList* override;
		X12_API auto GetCopyCommandContext() -> ICoreCopyCommandList* override { return (ICoreCopyCommandList*)copyCommandContext; }

		X12_API auto _FetchSurface(HWND hwnd) -> surface_ptr override;
		X12_API auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void override;
		X12_API auto GetWindowSurface(HWND hwnd) -> surface_ptr override;
		X12_API auto PresentSurfaces() -> void override;
		X12_API auto RefreshFencesStatus() -> void;
		X12_API auto FrameEnd() -> void override;

		X12_API auto ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void override;
		X12_API auto WaitGPU() -> void override;
		X12_API auto WaitGPUAll() -> void override;

		X12_API auto GetStat(CoreRenderStat& stat_) -> void override {stat_ = stat; };

		X12_API auto IsVSync() -> bool  override { return Vsync; }

		X12_API bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText,
			const char* fragText, const ConstantBuffersDesc* variabledesc = nullptr,
			uint32_t varNum = 0) override;

		X12_API bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
			const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		X12_API bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData,
			const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc,
			MEMORY_TYPE mem) override;

		X12_API bool CreateBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags,
			MEMORY_TYPE mem, const void* data, size_t bum) override;

		X12_API bool CreateTexture(ICoreTexture** out, LPCWSTR name, const uint8_t* data,
			size_t size, int32_t width, int32_t height, uint32_t mipCount,
			TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags) override;

		X12_API bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name,
			std::vector<D3D12_SUBRESOURCE_DATA> subresources,
			ID3D12Resource* d3dexistingtexture) override;

		X12_API bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name,
			ID3D12Resource* existingTexture) override;

		X12_API bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) override;

		X12_API bool CreateQuery(ICoreQuery** out) override;

		X12_API void* GetNativeDevice() override { return device; }

		X12_API void* GetNativeGraphicQueue() override { return queues[QUEUE_GRAPHIC].d3dCommandQueue; }

		// API-end

		// d3d12 specific
		auto GetDevice() -> device_t* { return device; }
		auto GetGraphicPSO(const GraphicPipelineState& pso, psomap_checksum_t checksum)->ComPtr<ID3D12PipelineState>;
		auto GetComputePSO(const ComputePipelineState& pso, psomap_checksum_t checksum)->ComPtr<ID3D12PipelineState>;
		auto IsTearingSupport() -> bool { return tearingSupported; }
		auto GetDefaultRootSignature()->ComPtr<ID3D12RootSignature>;
		auto CommandQueue() -> ID3D12CommandQueue* const { return queues[QUEUE_GRAPHIC].d3dCommandQueue; }
		auto GpuTickDelta() -> float const { return gpuTickDelta; }
		auto FrameMemory() -> DirectX::GraphicsMemory* { return frameMemory.get(); }

		auto AllocateStaticDescriptor(UINT num = 1) -> x12::descriptorheap::Alloc;
		auto AllocateStaticRTVDescriptor(UINT num = 1)->x12::descriptorheap::Alloc;
		auto AllocateStaticDSVDescriptor(UINT num = 1)->x12::descriptorheap::Alloc;
		auto AllocateSRVDescriptor(UINT num = 1) -> std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>;
		auto DescriptorHeapPtr() -> ID3D12DescriptorHeap** { return srv.heap.GetAddressOf(); }

		auto SRV_DescriptorsSize() -> UINT { return descriptorSizeCBSRV; }
		auto RTV_DescriptorsSize() -> UINT { return descriptorSizeRTV; }
		auto DSV_DescriptorsSize() -> UINT { return descriptorSizeDSV; }
	};


	// Helpers
	namespace d3d12
	{
		inline Dx12CoreRenderer* D3D12GetCoreRender() { return static_cast<Dx12CoreRenderer*>(_coreRender); }

		inline device_t* CR_GetD3DDevice() { return d3d12::D3D12GetCoreRender()->GetDevice(); }
		inline bool							CR_IsTearingSupport() { return d3d12::D3D12GetCoreRender()->IsTearingSupport(); }
		inline bool							CR_IsVSync() { return GetCoreRender()->IsVSync(); }
		inline UINT							CR_CBSRV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->SRV_DescriptorsSize(); }
		inline UINT							CR_RTV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->RTV_DescriptorsSize(); }
		inline UINT							CR_DSV_DescriptorsSize() { return d3d12::D3D12GetCoreRender()->DSV_DescriptorsSize(); }
	}
}
