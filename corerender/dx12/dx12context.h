#pragma once
#include "common.h"
#include "dx12common.h"
#include "icorerender.h"
#include "intrusiveptr.h"

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

	struct SlotData
	{
		RESOURCE_BIND_FLAGS bindFlags;
		RESOURCE_BIND_FLAGS bindDirtyFlags;

		Dx12UniformBuffer* CBV;

		union
		{
			Dx12CoreTexture* texture;
			Dx12CoreBuffer* buffer;
		} SRV;

		union
		{
			Dx12CoreTexture* texture;
			Dx12CoreBuffer* buffer;
		} UAV;
	};

	// State
	surface_ptr surface;
	bool needReBindSurface{true}; // ex.: we need bind surfaces after calls: d3dcmdlist::Close(), d3dcmdlist::Reset()

	struct StateCache
	{
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissor;

		struct PSO
		{
			ComPtr<ID3D12RootSignature> rootSignature;
			intrusive_ptr<Dx12CoreShader> shader;
			intrusive_ptr<Dx12CoreVertexBuffer> vb;

			bool isCompute;
			psomap_checksum_t graphicPsoChecksum; // 0 not set
			psomap_checksum_t computePsoChecksum; // 0 not set
			ComPtr<ID3D12PipelineState> d3dpso;
		}pso;

		PRIMITIVE_TOPOLOGY primitiveTopology;
		uint8_t shaderTypesUsedBits;
		SlotData binds[(int)SHADER_TYPE::NUM][MaxResourcesPerShader];
	} 
	state;
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
		x12::fastdescriptorallocator::Allocator *fastAllocator;

		// GPU-visible descriptors
		ID3D12DescriptorHeap* gpuDescriptorHeap{};
		UINT gpuDescriptorsOffset{};
		D3D12_CPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStart{0};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHeapStartGPU{0};

		inline D3D12_CPU_DESCRIPTOR_HANDLE newGPUHandle();

		void Init(Dx12GraphicCommandContext* parent_);
		void Free();
		void TrackResource(IResourceUnknown* resource);
		void ReleaseTrakedResources();
		void CompleteGPUFrame(uint64_t nextFenceID);
		void CommandsBegin();
	};

	CommandList cmdLists[DeferredBuffers];
	CommandList* cmdList{nullptr};
	ID3D12GraphicsCommandList* d3dCmdList;

	// Query
	const UINT maxNumTimers = 3;
	const UINT maxNumQuerySlots = maxNumTimers * 2;
	double gpuTickDelta = 0.0;
	UINT queryResolveToFrameID = 0;
	std::vector<UINT64> queryTiming;
	ID3D12QueryHeap* queryHeap;
	ID3D12Resource* queryReadBackBuffer;

	FinishFrameBroadcast finishFrameBroadcast;

	void resetStatistic();
	void resetPSOState();
	void resetFullState();
	void bindResources();

	void setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);
	void setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);

public:
	Dx12GraphicCommandContext(FinishFrameBroadcast finishFrameCallback_);
	~Dx12GraphicCommandContext();

	UINT frameIndex{}; // 0, 1, 2

	void Free();
	ID3D12CommandQueue* GetD3D12CmdQueue() { return d3dCommandQueue; }
	ID3D12GraphicsCommandList* GetD3D12CmdList() { return d3dCmdList; }

	inline uint64_t CurentFrame() const { return fenceValue; }

	uint64_t drawCalls() const { return statistic.drawCalls; }
	uint64_t triangles() const { return statistic.triangles; }
	uint64_t uniformBufferUpdates() const { return statistic.uniformBufferUpdates; }
	uint64_t stateChanges() const { return statistic.stateChanges; }

	void bindSurface();

public:
	// API

	void CommandsBegin(surface_ptr surface = surface_ptr{});
	void CommandsEnd();
	void FrameEnd();

	void Submit();

	void WaitGPUFrame();
	void WaitGPUAll();

	void PushState();
	void PopState();

	void SetGraphicPipelineState(const GraphicPipelineState& gpso);
	void SetComputePipelineState(const ComputePipelineState& cpso);
	void SetVertexBuffer(Dx12CoreVertexBuffer* vb);
	void SetViewport(unsigned width, unsigned heigth);
	void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth);

	void Draw(Dx12CoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0);
	void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1);
	void Clear();

	void BindUniformBuffer(int slot, Dx12UniformBuffer* buffer, SHADER_TYPE shaderType);
	void BindTexture(int slot, Dx12CoreTexture* buffer, SHADER_TYPE shaderType);
	void BindSRVStructuredBuffer(int slot, Dx12CoreBuffer* buffer, SHADER_TYPE shaderType);
	void BindUAVStructuredBuffer(int slot, Dx12CoreBuffer* buffer, SHADER_TYPE shaderType);

	void UpdateUniformBuffer(Dx12UniformBuffer* buffer, const void* data, size_t offset, size_t size);

	void EmitUAVBarrier(Dx12CoreBuffer* buffer);

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

