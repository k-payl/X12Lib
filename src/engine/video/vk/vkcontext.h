#pragma once
#include "vkcommon.h"

namespace x12
{
	class VkGraphicCommandList : public ICoreGraphicCommandList
	{
		std::vector<VkCommandBuffer>	commandBuffers;
		UINT							frameIndex{}; // 0, 1, 2
		UINT							swapchainIndex;

		std::vector<VkFence>			fencesRenderFinished;
		std::vector<VkSemaphore>		samaphoresImageAvailable;
		std::vector<VkSemaphore>		semaphoresRenderFinished;

		struct StateCache
		{
			surface_ptr surface;

			//D3D12_VIEWPORT viewport;
			//D3D12_RECT scissor;

			//struct PSO
			//{
			//	bool isCompute;
			//	psomap_checksum_t PsoChecksum; // 0 not set. Compute or graphic checksum
			//	GraphicPipelineState graphicDesc;
			//	ComputePipelineState computeDesc;
			//	ComPtr<ID3D12RootSignature> d3drootSignature;
			//	ComPtr<ID3D12PipelineState> d3dpso;
			//}pso;

			//intrusive_ptr<Dx12ResourceSet> set_;

		} state;
		std::stack<StateCache> statesStack;


	public:
		VkGraphicCommandList();
		~VkGraphicCommandList();
		void BindSurface(surface_ptr& surface_) override; // TODO: bind arbitary textures
		void CommandsBegin() override;
		void CommandsEnd() override;
		void FrameEnd() override;
		void Free() override;
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
		void Clear() override;
		void CompileSet(IResourceSet* set_) override;
		void BindResourceSet(IResourceSet* set_) override;
		void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) override;
		void EmitUAVBarrier(ICoreBuffer* buffer) override;
		void StartQuery(ICoreQuery* query) override;
		void StopQuery(ICoreQuery* query) override;
		void* GetNativeResource() override;
	};
}
