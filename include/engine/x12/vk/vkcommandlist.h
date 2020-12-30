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
		} state;

	public:
		VkGraphicCommandList();
		~VkGraphicCommandList();

		X12_API void BindSurface(surface_ptr& surface_) override; // TODO: bind arbitary textures
		X12_API void SetRenderTargets(x12::ICoreTexture**, uint32_t, x12::ICoreTexture*) override;
		X12_API void CommandsBegin() override;
		X12_API void CommandsEnd() override;
		X12_API void FrameEnd() override;
		X12_API void Free() override;
		X12_API void SetGraphicPipelineState(const GraphicPipelineState& gpso) override;
		X12_API void SetComputePipelineState(const ComputePipelineState& cpso) override;
		X12_API void SetVertexBuffer(ICoreVertexBuffer* vb) override;
		X12_API void SetViewport(unsigned width, unsigned heigth) override;
		X12_API void GetViewport(unsigned& width, unsigned& heigth) override;
		X12_API void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth) override;
		X12_API void Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0) override;
		X12_API void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1) override;
		X12_API void Clear() override;
		X12_API void CompileSet(IResourceSet* set_) override;
		X12_API void BindResourceSet(IResourceSet* set_) override;
		X12_API void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) override;
		X12_API void EmitUAVBarrier(ICoreBuffer* buffer) override;
		X12_API void StartQuery(ICoreQuery* query) override;
		X12_API void StopQuery(ICoreQuery* query) override;
		X12_API void* GetNativeResource() override;

		VkCommandBuffer* CommandBuffer() { return &commandBuffers[frameIndex]; }
	};
}
