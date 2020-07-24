#include "pch.h"
#include "vkcontext.h"
#include "vkrender.h"

x12::VkGraphicCommandList::VkGraphicCommandList()
{
	commandBuffers.resize(DeferredBuffers);

	VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandPool = x12::vk::GetCommandPool();
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	VK_CHECK(vkAllocateCommandBuffers(x12::vk::GetDevice(), &allocateInfo, commandBuffers.data()));

	// Sync objects
	samaphoresImageAvailable.resize(DeferredBuffers);
	semaphoresRenderFinished.resize(DeferredBuffers);
	fencesRenderFinished.resize(DeferredBuffers);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < DeferredBuffers; i++)
	{
		VK_CHECK(vkCreateSemaphore(vk::GetDevice(), &semaphoreInfo, nullptr, &samaphoresImageAvailable[i]));
		VK_CHECK(vkCreateSemaphore(vk::GetDevice(), &semaphoreInfo, nullptr, &semaphoresRenderFinished[i]));
		VK_CHECK(vkCreateFence(vk::GetDevice(), &fenceInfo, nullptr, &fencesRenderFinished[i]));
	}
}

x12::VkGraphicCommandList::~VkGraphicCommandList()
{
	vkFreeCommandBuffers(x12::vk::GetDevice(), x12::vk::GetCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

	for (size_t i = 0; i < DeferredBuffers; i++)
	{
		vkDestroySemaphore(vk::GetDevice(), semaphoresRenderFinished[i], nullptr);
		vkDestroySemaphore(vk::GetDevice(), samaphoresImageAvailable[i], nullptr);
		vkDestroyFence(vk::GetDevice(), fencesRenderFinished[i], nullptr);
	}
}

void x12::VkGraphicCommandList::BindSurface(x12::surface_ptr& surface_)
{
	VkWindowSurface* s = static_cast<VkWindowSurface*>(surface_.get());

	VkResult result = vkAcquireNextImageKHR(vk::GetDevice(), s->swapchain,
											std::numeric_limits<uint64_t>::max(), samaphoresImageAvailable[frameIndex],
											VK_NULL_HANDLE, &s->swapchainFrameIndex);

	if (result != VK_SUCCESS)
	{
		//windowResize(core);
		notImplemented();
	}

}
void x12::VkGraphicCommandList::CommandsBegin()
{
	VkCommandBufferResetFlags flags = {};
	VK_CHECK(vkResetCommandBuffer(commandBuffers[frameIndex], flags));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VK_CHECK(vkBeginCommandBuffer(commandBuffers[frameIndex], &beginInfo));
}
void x12::VkGraphicCommandList::CommandsEnd()
{
	VK_CHECK(vkEndCommandBuffer(commandBuffers[frameIndex]));
}
void x12::VkGraphicCommandList::FrameEnd() {}

void x12::VkGraphicCommandList::Free()
{
}

/*
void x12::VkGraphicCommandList::Submit()
{
	
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &samaphoresImageAvailable[frameIndex];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphoresRenderFinished[frameIndex];

	VK_CHECK(vkQueueSubmit(vk::GetGraphicQueue(), 1, &submitInfo, fencesRenderFinished[frameIndex]));

	VkWindowSurface* surface = static_cast<VkWindowSurface*>(state.surface.get());
	surface->UpdateSubmitedSemaphore(semaphoresRenderFinished[frameIndex]);
}
void x12::VkGraphicCommandList::WaitGPUFrame()
{
	// Shift to next frame
	frameIndex = (frameIndex + 1u) % DeferredBuffers;

	vkWaitForFences(vk::GetDevice(), 1, &fencesRenderFinished[frameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(vk::GetDevice(), 1, &fencesRenderFinished[frameIndex]);
}
*/
void x12::VkGraphicCommandList::PushState() {}
void x12::VkGraphicCommandList::PopState() {}
void x12::VkGraphicCommandList::SetGraphicPipelineState(const GraphicPipelineState& gpso) {}
void x12::VkGraphicCommandList::SetComputePipelineState(const ComputePipelineState& cpso) {}
void x12::VkGraphicCommandList::SetVertexBuffer(ICoreVertexBuffer* vb) {}
void x12::VkGraphicCommandList::SetViewport(unsigned width, unsigned heigth) {}
void x12::VkGraphicCommandList::GetViewport(unsigned& width, unsigned& heigth) {}
void x12::VkGraphicCommandList::SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth) {}
void x12::VkGraphicCommandList::Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount, uint32_t vertexOffset) {}
void x12::VkGraphicCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z) {}
void x12::VkGraphicCommandList::Clear() {}
void x12::VkGraphicCommandList::CompileSet(IResourceSet* set_) {}
void x12::VkGraphicCommandList::BindResourceSet(IResourceSet* set_) {}
void x12::VkGraphicCommandList::UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) {}
void x12::VkGraphicCommandList::EmitUAVBarrier(ICoreBuffer* buffer) {}

void x12::VkGraphicCommandList::StartQuery(ICoreQuery* query)
{
}

void x12::VkGraphicCommandList::StopQuery(ICoreQuery* query)
{
}

