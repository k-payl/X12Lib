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

		} queues[QUEUE_NUM];

		std::map<HWND, surface_ptr> surfaces;
		std::set<surface_ptr> surfacesForPresenting;

		std::vector<std::unique_ptr<Dx12GraphicCommandList>> commandLists;

		struct CmdListState
		{
			int32_t id{-1};
			uint64_t submitedValue{};
			bool isBusy{};
		};
		std::vector<CmdListState> commandListsStates;

		Dx12CopyCommandList* copyCommandContext;
		std::unique_ptr<DirectX::GraphicsMemory> frameMemory;

		std::mutex psoMutex;
		psomap_t psoMap; // All Pipeline State Objects. checksum -> PSO
		uint64_t psoNum{};

		x12::descriptorheap::Allocator* descriptorAllocator; // Pre created descriptors for long-lived resources. Keeps a lot of ID3D12DescriptorHeap's
		
		ComPtr<ID3D12DescriptorHeap> gpuDescriptorHeap; // Descriptor heap using at rendering. TODO: make deallocation
		const size_t NumStaticGpuHadles = 1'024;
		UINT gpuDescriptorsOffset{};
		D3D12_CPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStart{0};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStartGPU{0};

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
		auto Init() -> void override;
		auto Free() -> void override;

		auto GetGraphicCommandList()->ICoreGraphicCommandList* override;
		auto GetGraphicCommandList(int32_t id)->ICoreGraphicCommandList* override;
		auto ReleaseGraphicCommandList(int32_t id)->void override;
		auto GetCopyCommandContext() -> ICoreCopyCommandList* override { return (ICoreCopyCommandList*)copyCommandContext; }

		auto _FetchSurface(HWND hwnd) -> surface_ptr override;
		auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void override;
		auto GetWindowSurface(HWND hwnd) -> surface_ptr override;
		auto PresentSurfaces() -> void override;
		auto _ReleaseGraphicQueueResources() -> void;
		auto FrameEnd() -> void override;

		auto ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void override;
		auto WaitGPU() -> void override;
		auto WaitGPUAll() -> void override;

		auto GetStat(CoreRenderStat& stat_) -> void override {stat_ = stat; };

		auto IsVSync() -> bool  override { return Vsync; }

		bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
						  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
								 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) override;

		bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
								const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem) override;

		bool CreateConstantBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, bool FastGPUread = false) override;

		bool CreateStructuredBuffer(ICoreBuffer** out, LPCWSTR name, size_t structureSize, size_t num,
									const void* data, BUFFER_FLAGS flags) override;

		bool CreateRawBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags) override;

		bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
							   std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture) override;

		bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) override;

		bool CreateQuery(ICoreQuery** out) override;

		void* GetNativeDevice() override { return device; }

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
		ID3D12CommandQueue* CommandQueue() const { return queues[QUEUE_GRAPHIC].d3dCommandQueue; }
		float GpuTickDelta() const { return gpuTickDelta; }
		DirectX::GraphicsMemory* FrameMemory() { return frameMemory.get(); }
		ID3D12DescriptorHeap** DescriptorHeap() { return gpuDescriptorHeap.GetAddressOf(); }
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> allocateStaticGPUHandles(UINT num);
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
