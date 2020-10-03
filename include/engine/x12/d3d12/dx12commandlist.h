#pragma once
#include "dx12common.h"
#include "dx12memory.h"
#include "intrusiveptr.h"

namespace x12
{
	// Graphic comands interface.
	//	Can be created multiplie instance of it in Dx12CoreRenderer.
	//	Can be recorded only in one thread.
	//
	class Dx12GraphicCommandList : public ICoreGraphicCommandList
	{
		device_t* device;

		UINT descriptorSizeCBSRV;
		UINT descriptorSizeRTV;
		UINT descriptorSizeDSV;

		struct StateCache
		{
			surface_ptr surface;
			intrusive_ptr<ICoreTexture> renderTarget[8];
			intrusive_ptr<ICoreTexture> depthStencil;

			D3D12_VIEWPORT viewport;
			D3D12_RECT scissor;

			struct PSO
			{
				bool isCompute;
				psomap_checksum_t PsoChecksum; // 0 not set. Compute or graphic checksum
				GraphicPipelineState graphicDesc;
				ComputePipelineState computeDesc;
				ComPtr<ID3D12RootSignature> d3drootSignature;
				ComPtr<ID3D12PipelineState> d3dpso;
			}pso;

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
		Statistic frameStatistic;

		ID3D12CommandAllocator* d3dCommandAllocator{};
		ID3D12GraphicsCommandList* d3dCmdList{};

		bool heapBinded{false};

		std::vector<IResourceUnknown*> trakedResources;

		static int contextNum;

		//void transiteSurfaceToState(D3D12_RESOURCE_STATES newState);
		void resetStatistic();
		void resetOnlyPSOState(); // reset PSO and shader bindings
		void resetFullState();
		void setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);
		void setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso);
		void TrackResource(IResourceUnknown* resource);
		void ReleaseTrakedResources();

	public:
		// Internal

		Dx12GraphicCommandList(Dx12CoreRenderer *renderer, int32_t id_);
		~Dx12GraphicCommandList();

		Dx12CoreRenderer* renderer;

		void							Free();
		
		ID3D12GraphicsCommandList*		GetD3D12CmdList() { return d3dCmdList; }

		uint64_t						drawCalls() const { return frameStatistic.drawCalls; }
		uint64_t						triangles() const { return frameStatistic.triangles; }
		uint64_t						uniformBufferUpdates() const { return frameStatistic.uniformBufferUpdates; }
		uint64_t						stateChanges() const { return frameStatistic.stateChanges; }

		template<typename... Arguments>
		static void set_ctx_object_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, L"Graphic context #%d %s", contextNum, format);
			x12::d3d12::set_name(obj, wstr, args...);
		}

		void NotifyFrameCompleted(uint64_t completed) override;

	public:
		// API

		void BindSurface(surface_ptr& surface_) override; // TODO: bind arbitary textures
		void SetRenderTargets(ICoreTexture** textures, uint32_t count, ICoreTexture* depthStencil) override;

		void CommandsBegin() override;
		void CommandsEnd() override;
		void FrameEnd() override;

		void PushState() override;
		void PopState() override;

		void SetGraphicPipelineState(const GraphicPipelineState& gpso) override;
		void SetComputePipelineState(const ComputePipelineState& cpso) override;
		void SetVertexBuffer(ICoreVertexBuffer* vb) override;
		void SetViewport(unsigned width, unsigned heigth) override;
		void GetViewport(unsigned& width, unsigned& heigth) override;
		void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth) override;

		void Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0) override;
		void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1) override;
		void Clear();

		void CompileSet(IResourceSet* set_) override;
		void BindResourceSet(IResourceSet* set_) override;
		void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) override;

		void EmitUAVBarrier(ICoreBuffer* buffer) override;

		void StartQuery(ICoreQuery* query) override;
		void StopQuery(ICoreQuery* query) override;

		void* GetNativeResource() override;
	};


	class Dx12CopyCommandList : public ICoreCopyCommandList
	{
		ID3D12CommandAllocator* d3dCommandAllocator{};
		ID3D12GraphicsCommandList* d3dCommandList{};
		device_t* device;

		static int contextNum;

		template<typename... Arguments>
		static void set_ctx_object_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, L"Copy context #%d %s", contextNum, format);
			x12::d3d12::set_name(obj, wstr, args...);
		}

	public:
		Dx12CopyCommandList();
		~Dx12CopyCommandList();

		virtual ID3D12GraphicsCommandList* GetD3D12CmdList() { return d3dCommandList; }

		void FrameEnd() override;
		void Free() override;
		void CommandsBegin() override;
		void CommandsEnd() override;
	};
}

