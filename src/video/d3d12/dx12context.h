#pragma once
#include "dx12common.h"
#include "dx12memory.h"
#include "intrusiveptr.h"

namespace x12
{
	// Prebuild combination of static resources.
	//	that binds to context fast.
	//	Dynamic resources can be updated through CommandContext.
	//
	struct Dx12ResourceSet : IResourceSet
	{
		using resource_index = std::pair<size_t, int>;

		struct BindedResource : ResourceDefinition
		{
			intrusive_ptr<Dx12CoreBuffer> buffer;
			intrusive_ptr<Dx12CoreTexture> texture;

			BindedResource& operator=(const ResourceDefinition& r)
			{
				static_cast<ResourceDefinition&>(*this) = r;
				return *this;
			}
		};

		std::unordered_map<std::string, std::pair<size_t, int>> resourcesMap; // {parameter index, table index}. (table index=-1 if inline)

		bool dirty{ false };

		size_t rootParametersNum;

		// parallel arrays
		std::vector<RootSignatureParameter<BindedResource>> resources;
		std::vector<bool> resourcesDirty;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuDescriptors;

	public:
		Dx12ResourceSet(const Dx12CoreShader* shader);

		void BindConstantBuffer(const char* name, ICoreBuffer *buffer) override;
		void BindStructuredBufferSRV(const char* name, ICoreBuffer *buffer) override;
		void BindStructuredBufferUAV(const char* name, ICoreBuffer *buffer) override;
		void BindTextueSRV(const char* name, ICoreTexture *texture) override;

		size_t FindInlineBufferIndex(const char* name) override;

	private:
		inline void checkResourceIsTable(const resource_index& index);
		inline void checkResourceIsInlineDescriptor(const resource_index& index);

		resource_index& findResourceIndex(const char* name);

		template<class TRes, class TDx12Res>
		void Bind(const char* name, TRes* resource, RESOURCE_DEFINITION type)
		{
			auto dx12resource = static_cast<TDx12Res*>(resource);

			auto& index = findResourceIndex(name);
			checkResourceIsTable(index);

			Dx12ResourceSet::BindedResource& resourceSlot = resources[index.first].tableResources[index.second];

			verify(type == resourceSlot.resources && "Invalid resource type");

			if constexpr (std::is_same<TRes, ICoreBuffer>::value)
				resourceSlot.buffer = dx12resource;
			else if constexpr (std::is_same<TRes, ICoreTexture>::value)
				resourceSlot.texture = dx12resource;
			else
				verify(0);

			dirty = true;
			gpuDescriptors[index.first] = {};
			resourcesDirty[index.first] = true;
		}
	};

	// Graphic comands interface.
	//	Can be created multiplie instance of it in Dx12CoreRenderer.
	//	Can be recorded only in one thread.
	//
	class Dx12GraphicCommandContext
	{
		ID3D12CommandQueue *d3dCommandQueue{};

		ID3D12Fence* d3dFence{};
		HANDLE fenceEvent{};
		uint64_t fenceValue{1}; // fence value ready to signal

		device_t* device;

		UINT descriptorSizeCBSRV;
		UINT descriptorSizeRTV;
		UINT descriptorSizeDSV;

		/* --- State ---- */
		/*
			* PSO
			   * root signature
			   * resource set
			   * vertex buffer
			* render target
			* scissor
			* viewport
		*/

		struct StateCache
		{
			surface_ptr surface;

			D3D12_VIEWPORT viewport;
			D3D12_RECT scissor;

			struct PSO
			{
				bool isCompute;
				psomap_checksum_t PsoChecksum; // 0 not set. Compute or graphic checksum
				intrusive_ptr<Dx12CoreShader> shader;
				ComPtr<ID3D12RootSignature> d3drootSignature;
				ComPtr<ID3D12PipelineState> d3dpso;

				// Graphic only
				intrusive_ptr<Dx12CoreVertexBuffer> vb;
			}pso;

			PRIMITIVE_TOPOLOGY primitiveTopology;
			intrusive_ptr<Dx12ResourceSet> set_;

		} state;
		std::stack<StateCache> statesStack;

		struct Statistic
		{
			uint64_t drawCalls{};
			uint64_t triangles{};
			uint64_t uniformBufferUpdates{};
			uint64_t stateChanges{};
		};
		Statistic statistic;

		struct CommandList
		{
			CommandList() = default;
			~CommandList() = default;

			CommandList(const CommandList&) = delete;
			CommandList& operator=(const CommandList&) = delete;

			CommandList(CommandList&&) = delete;
			CommandList& operator=(CommandList&&) = delete;

			Dx12GraphicCommandContext *parent;
			device_t* device;

			ID3D12CommandAllocator* d3dCommandAllocator{};
			ID3D12GraphicsCommandList* d3dCmdList{};

			uint64_t fenceOldValue{0};
			std::vector<IResourceUnknown*> trakedResources;

			void Init(Dx12GraphicCommandContext* parent_, int num);
			void Free();
			void TrackResource(IResourceUnknown* resource);
			void ReleaseTrakedResources();
			void CompleteGPUFrame(uint64_t nextFenceID);
		};

		CommandList cmdLists[DeferredBuffers];
		CommandList* cmdList{nullptr};
		ID3D12GraphicsCommandList* d3dCmdList;

		std::unique_ptr<DirectX::GraphicsMemory> frameMemory;

		// GPU descriptors
		ComPtr<ID3D12DescriptorHeap> gpuDescriptorHeap;
		UINT gpuDescriptorsOffset{};
		D3D12_CPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStart{0};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStartGPU{0};

		// Query
		const UINT maxNumTimers = 3;
		const UINT maxNumQuerySlots = maxNumTimers * 2;
		double gpuTickDelta = 0.0;
		UINT queryResolveToFrameID = 0;
		std::vector<UINT64> queryTiming;
		ID3D12QueryHeap* queryHeap;
		ID3D12Resource* queryReadBackBuffer;

		static int contextNum;

		FinishFrameBroadcast finishFrameBroadcast;

		void transiteSurfaceToState(D3D12_RESOURCE_STATES newState);
		void resetStatistic();
		void resetOnlyPSOState(); // reset PSO and shader bindings
		void resetFullState();
		void setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);
		void setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);

	public:
		Dx12GraphicCommandContext(FinishFrameBroadcast finishFrameCallback_);
		~Dx12GraphicCommandContext();

		UINT frameIndex{}; // 0, 1, 2

		void Free();
		ID3D12CommandQueue* GetD3D12CmdQueue() { return d3dCommandQueue; }
		ID3D12GraphicsCommandList* GetD3D12CmdList() { return d3dCmdList; }
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> newGPUHandle(UINT num = 1);

		DirectX::GraphicsMemory* memory() { return frameMemory.get(); }

		inline uint64_t CurentFrame() const { return fenceValue; }

		uint64_t drawCalls() const { return statistic.drawCalls; }
		uint64_t triangles() const { return statistic.triangles; }
		uint64_t uniformBufferUpdates() const { return statistic.uniformBufferUpdates; }
		uint64_t stateChanges() const { return statistic.stateChanges; }

		template<typename... Arguments>
		static void set_ctx_object_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, L"Graphic context #%d %s", contextNum, format);
			x12::impl::set_name(obj, wstr, args...);
		}

	public:
		// API

		void BindSurface(const surface_ptr& surface_); // TODO: bind arbitary textures

		void CommandsBegin();
		void CommandsEnd();
		void FrameEnd();

		void Submit();

		void WaitGPUFrame();
		void WaitGPUAll();

		void PushState();
		void PopState();

		void SetGraphicPipelineState(const GraphicPipelineState& gpso);
		void SetComputePipelineState(const ComputePipelineState& cpso);
		void SetVertexBuffer(ICoreVertexBuffer* vb);
		void SetViewport(unsigned width, unsigned heigth);
		void GetViewport(unsigned& width, unsigned& heigth);
		void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth);

		void Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0);
		void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1);
		void Clear();

		void BuildResourceSet(IResourceSet* set_);
		void BindResourceSet(IResourceSet* set_);
		void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size);

		void EmitUAVBarrier(ICoreBuffer* buffer);

		void TimerBegin(uint32_t timerID);
		void TimerEnd(uint32_t timerID);
		auto TimerGetTimeInMs(uint32_t timerID) -> float;
	};

	class Dx12CopyCommandContext
	{
		ID3D12CommandAllocator* d3dCommandAllocator{};
		ID3D12GraphicsCommandList* d3dCommandList{};
		ID3D12CommandQueue* d3dCommandQueue{};
		ID3D12Fence* d3dFence{};
		HANDLE fenceEvent{};
		uint64_t fenceValue{1}; // fence value ready to signal
		device_t* device;

		static int contextNum;

		template<typename... Arguments>
		static void set_ctx_object_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, L"Copy context #%d %s", contextNum, format);
			x12::impl::set_name(obj, wstr, args...);
		}

	public:
		Dx12CopyCommandContext();
		~Dx12CopyCommandContext();

		ID3D12CommandQueue* GetD3D12CmdQueue() { return d3dCommandQueue; }
		ID3D12GraphicsCommandList* GetD3D12CmdList() { return d3dCommandList; }

		void Free();
		void CommandsBegin();
		void CommandsEnd();
		void Submit();
		void WaitGPUAll();
	};
}

