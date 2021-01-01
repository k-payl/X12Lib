#include "vkvertexbuffer.h"
#include "vkrender.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

void x12::VkCoreVertexBuffer::Init(LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem)
{
	const bool hasIndexBuffer = idxData != nullptr && idxDesc != nullptr;

	{
		VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bufferInfo.size = VbSizeFromDesc(vbDesc);
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // TODO: avoid cpu

		VmaAllocationInfo allocationInfo{};

		vmaCreateBuffer(vk::GetAllocator(), &bufferInfo, &allocInfo, &vertexBuffer, &vertexBufferAllocation, &allocationInfo);

		vertexBufferMemory = allocationInfo.deviceMemory;
	}

	UINT idxDataSize = 0;

	if (hasIndexBuffer)
	{
		idxDataSize = formatInBytes(idxDesc->format) * idxDesc->vertexCount;

		VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bufferInfo.size = idxDataSize;
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // TODO: avoid cpu

		VmaAllocationInfo allocationInfo{};

		vmaCreateBuffer(vk::GetAllocator(), &bufferInfo, &allocInfo, &indexBuffer, &indexBufferAllocation, &allocationInfo);

		indexBufferMemory = allocationInfo.deviceMemory;
	}

	SetData(vbData, VbSizeFromDesc(vbDesc), 0, idxData, idxDataSize, 0);
}

void x12::VkCoreVertexBuffer::SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset)
{

	setData(vertexBuffer, vertexBufferMemory, vbData, vbSize, vbOffset);

	if (indexBuffer)
		setData(indexBuffer, indexBufferMemory, idxData, idxSize, idxOffset);
}

x12::VkCoreVertexBuffer::~VkCoreVertexBuffer()
{
	vmaDestroyBuffer(vk::GetAllocator(), vertexBuffer, vertexBufferAllocation);

	if (indexBuffer)
		vmaDestroyBuffer(vk::GetAllocator(), indexBuffer, indexBufferAllocation);
}

void x12::VkCoreVertexBuffer::setData(VkBuffer buffer, VkDeviceMemory memory, const void* data, size_t size, size_t offset)
{
	void* Dstdata;
	vkMapMemory(vk::GetDevice(), memory, 0, size, 0, &Dstdata);
	memcpy((uint8_t*)Dstdata + offset, data, size);
	vkUnmapMemory(vk::GetDevice(), memory);

}
