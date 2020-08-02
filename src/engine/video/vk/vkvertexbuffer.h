#pragma once
#include "vkcommon.h"

namespace x12
{
	struct VkCoreVertexBuffer : public ICoreVertexBuffer
	{
		void Init(LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS usage = BUFFER_FLAGS::GPU_READ);
		void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset) override;

		~VkCoreVertexBuffer();

	private:
		void setData(VkBuffer buffer, VkDeviceMemory memory, const void* data, size_t size, size_t offset);

		VkBuffer vertexBuffer{};
		VmaAllocation vertexBufferAllocation{};
		VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};

		VkBuffer indexBuffer{};
		VmaAllocation indexBufferAllocation{};
		VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
	};
}
