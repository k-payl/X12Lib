#pragma once
#include "common.h"
#include "icorerender.h"

inline constexpr size_t MaxBindedResourcesPerFrame = 100'000;
inline constexpr unsigned MaxResourcesPerShader = 32;

inline constexpr unsigned D3D12MaxRootParameters = 16;


class Dx12GraphicCommandContext
{
	ID3D12CommandQueue* d3dCommandQueue{};
	ID3D12Fence* d3dFence{};
	HANDLE fenceEvent{};
	uint64_t fenceValue{1}; // fence value ready to signal
	Dx12WindowSurface* surface;

	UINT descriptorSizeCBSRV;
	UINT descriptorSizeRTV;
	UINT descriptorSizeDSV;

	struct State
	{
		Dx12CoreShader *shader;
		Dx12CoreVertexBuffer *vb;
		ID3D12DescriptorHeap* d3dDescriptorHeap{};

		struct Binding
		{
			Dx12UniformBuffer* buffer;
			int dirty;
		};

		struct ShaderBindings
		{
			Binding resources[MaxResourcesPerShader];
			int dirty{0};
			int maxSlotBinded{-1};
		};

		ShaderBindings bind[(int)SHADER_TYPE::NUM];
	} 
	state;
	void resetState();

	struct Statistic
	{
		uint64_t drawCalls{0};
		uint64_t triangles{0};
		uint64_t uniformBufferUpdates{0};
	}
	statistic;
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

	void _BindResources();

public:
	Dx12GraphicCommandContext(Dx12WindowSurface* surface_, FinishFrameBroadcast finishFrameCallback_);
	~Dx12GraphicCommandContext();

	UINT frameIndex{}; // 0, 1, 2

	void Free();
	ID3D12CommandQueue* GetD3D12CmdQueue() { return d3dCommandQueue; }

	inline uint64_t CurentFrame() const { return fenceValue; }

public:
	// API

	void Begin();
	void End();
	void Submit();
	void Present();

	void WaitGPUFrame();
	void WaitGPUAll();

	void SetPipelineState(const PipelineState& desc);
	void SetVertexBuffer(Dx12CoreVertexBuffer* vb);
	void GetBufferSize(unsigned& width, unsigned& heigth);
	void SetViewport(unsigned width, unsigned heigth);
	void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth);
	void Draw(Dx12CoreVertexBuffer* vb);

	void BindUniformBuffer(SHADER_TYPE shader, int slot, Dx12UniformBuffer* buffer);
	void UpdateUnifromBuffer(Dx12UniformBuffer* buffer, const void* data, size_t offset, size_t size);

	void TimerBegin(uint32_t timerID);
	void TimerEnd(uint32_t timerID);
	auto TimerGetTimeInMs(uint32_t timerID) -> float;

	// TODO: do binding arbitary textures as RT
	void SetBuiltinRenderTarget();
	void ClearBuiltinRenderTarget(vec4 color);
	void ClearBuiltinRenderDepthBuffer();
};

class Dx12CopyCommandContext
{
	ID3D12CommandAllocator* d3dCommandAllocator{};
	ID3D12GraphicsCommandList* d3dCommandList{};
	ID3D12CommandQueue* d3dCommandQueue{};
	ID3D12Fence* d3dFence{};
	HANDLE fenceEvent{};
	uint64_t fenceValue{1}; // fence value ready to signal

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

