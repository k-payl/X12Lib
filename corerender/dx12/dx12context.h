#pragma once
#include "common.h"
#include "dx12common.h"
#include "icorerender.h"


class Dx12GraphicCommandContext
{
	ID3D12CommandQueue *d3dCommandQueue{};
	ID3D12Fence* d3dFence{};
	HANDLE fenceEvent{};
	uint64_t fenceValue{1}; // fence value ready to signal
	Dx12WindowSurface *surface{};
	device_t* device;

	UINT descriptorSizeCBSRV;
	UINT descriptorSizeRTV;
	UINT descriptorSizeDSV;

	struct State
	{
		Dx12CoreShader *shader;
		Dx12CoreVertexBuffer *vb;

		uint64_t psoChecksum; // 0 not set
		PRIMITIVE_TOPOLOGY primitiveTopology;

		struct SlotResource
		{
			RESOURCE_BIND_FLAGS dirtyFlags; // resources to set before draw call

			// TODO:
			// CBV register b
			// SRV register t
			// UAV register u
			Dx12UniformBuffer *CBV;

			union
			{
				Dx12CoreTexture* texture;
				Dx12CoreStructuredBuffer* structuredBuffer;
			} SRV;
		};

		struct ShaderResources
		{
			SlotResource resources[MaxResourcesPerShader];
			int dirty{0};
		};

		ShaderResources bind[(int)SHADER_TYPE::NUM];
	} 
	state;
	void resetState();

	struct Statistic
	{
		uint64_t drawCalls{};
		uint64_t triangles{};
		uint64_t uniformBufferUpdates{};
		uint64_t stateChanges{};
	};
	Statistic statistic;
	void resetStatistic();

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
		FastFrameAllocator::Allocator *fastAllocator;

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
		void Begin();
	};

	CommandList cmdLists[DeferredBuffers];
	CommandList* cmdList{nullptr};

	// Query
	const UINT maxNumTimers = 3;
	const UINT maxNumQuerySlots = maxNumTimers * 2;
	double gpuTickDelta = 0.0;
	UINT queryResolveToFrameID = 0;
	std::vector<UINT64> queryTiming;
	ID3D12QueryHeap* queryHeap;
	ID3D12Resource* queryReadBackBuffer;

	FinishFrameBroadcast finishFrameBroadcast;

	void bindResources();

public:
	Dx12GraphicCommandContext(FinishFrameBroadcast finishFrameCallback_);
	~Dx12GraphicCommandContext();

	UINT frameIndex{}; // 0, 1, 2

	void Free();
	ID3D12CommandQueue* GetD3D12CmdQueue() { return d3dCommandQueue; }

	inline uint64_t CurentFrame() const { return fenceValue; }

	uint64_t drawCalls() const { return statistic.drawCalls; }
	uint64_t triangles() const { return statistic.triangles; }
	uint64_t uniformBufferUpdates() const { return statistic.uniformBufferUpdates; }
	uint64_t stateChanges() const { return statistic.stateChanges; }

public:
	// API

	void Begin(Dx12WindowSurface* surface);
	void End();

	void Submit();

	void WaitGPUFrame();
	void WaitGPUAll();

	void SetPipelineState(const PipelineState& desc);
	void SetVertexBuffer(Dx12CoreVertexBuffer* vb);
	void SetViewport(unsigned width, unsigned heigth);
	void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth);
	void Draw(Dx12CoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0);

	void BindUniformBuffer(int slot, Dx12UniformBuffer* buffer, SHADER_TYPE shaderType);
	void BindTexture(int slot, Dx12CoreTexture* buffer, SHADER_TYPE shaderType);
	void BindStructuredBuffer(int slot, Dx12CoreStructuredBuffer* buffer, SHADER_TYPE shaderType);

	void UpdateUniformBuffer(Dx12UniformBuffer* buffer, const void* data, size_t offset, size_t size);

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
	void Begin();
	void End();
	void Submit();
	void WaitGPUAll();
};

