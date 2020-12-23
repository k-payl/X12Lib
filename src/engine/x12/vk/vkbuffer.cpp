#include "vkbuffer.h"
#include "vkrender.h"
#include "vkcommandlist.h"
#include "core.h"

void x12::VkCoreBuffer::Map()
{
	if (ptr || memoryType == MEMORY_TYPE::GPU_READ)
		return;

	vmaMapMemory(vk::GetAllocator(), buffer.vertexBufferAllocation, &ptr);
}

void x12::VkCoreBuffer::Unmap()
{
	if (!ptr || memoryType == MEMORY_TYPE::GPU_READ)
		return;

	vmaUnmapMemory(vk::GetAllocator(), buffer.vertexBufferAllocation);
	ptr = 0;
}

x12::VkCoreBuffer::VkCoreBuffer(size_t size_, const void* data, MEMORY_TYPE memoryType_,
	BUFFER_FLAGS flags_, LPCWSTR name_) :
	name(name_),
	flags(flags_),
	memoryType(memoryType_),
	size(size_)
{
	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = size;
	bufferInfo.usage = 0;

	if (flags_ & BUFFER_FLAGS::CONSTANT_BUFFER_VIEW)
		bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (flags_ & BUFFER_FLAGS::SHADER_RESOURCE_VIEW || flags_ & BUFFER_FLAGS::UNORDERED_ACCESS_VIEW)
		bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (memoryType & MEMORY_TYPE::READBACK)
		bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (memoryType & MEMORY_TYPE::GPU_READ) // for SetData()
		bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VmaAllocationCreateInfo allocInfo = {};

	switch (memoryType)
	{
	case x12::MEMORY_TYPE::CPU:
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		break;
	case x12::MEMORY_TYPE::GPU_READ:
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case x12::MEMORY_TYPE::READBACK:
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
		break;
	default:
		notImplemented();
		break;
	}	

	VmaAllocationInfo allocationInfo{};
	vmaCreateBuffer(vk::GetAllocator(), &bufferInfo, &allocInfo, &buffer.vkbuffer, &buffer.vertexBufferAllocation, &allocationInfo);

	buffer.vkDeviceMemory = allocationInfo.deviceMemory;

	SetData(data, size);
}

X12_API void x12::VkCoreBuffer::GetData(void* data)
{
	if (memoryType == MEMORY_TYPE::CPU || memoryType == MEMORY_TYPE::READBACK)
	{
		Map();
		memcpy(data, ptr, size);
		Unmap();
	}
	else if (memoryType == MEMORY_TYPE::GPU_READ)
	{
		Buffer stagingBuffer{};
		{
			VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

			VmaAllocationInfo allocationInfo{};
			vmaCreateBuffer(vk::GetAllocator(), &bufferInfo, &allocInfo,
				&stagingBuffer.vkbuffer, &stagingBuffer.vertexBufferAllocation, &allocationInfo);

			stagingBuffer.vkDeviceMemory = allocationInfo.deviceMemory;
		}

		auto* cmdList = GetCoreRender()->GetGraphicCommandList();
		cmdList->CommandsBegin();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(*static_cast<VkGraphicCommandList*>(cmdList)->CommandBuffer(),
			 buffer.vkbuffer, stagingBuffer.vkbuffer, 1, &copyRegion);

		cmdList->CommandsEnd();

		ICoreRenderer* renderer = engine::GetCoreRenderer();
		renderer->ExecuteCommandList(cmdList);

		renderer->WaitGPU(); // wait GPU copying

		void* mappedPtr;
		vmaMapMemory(vk::GetAllocator(), stagingBuffer.vertexBufferAllocation, &mappedPtr);
		memcpy(data, mappedPtr, size);
		vmaUnmapMemory(vk::GetAllocator(), stagingBuffer.vertexBufferAllocation);
	}
	else
		notImplemented();
}

X12_API void x12::VkCoreBuffer::SetData(const void* data, size_t size)
{
	if (!data)
		return;

	if (memoryType == MEMORY_TYPE::CPU || memoryType == MEMORY_TYPE::READBACK)
	{
		Map();
		memcpy(ptr, data, size);
		Unmap();
	}
	else if (memoryType == MEMORY_TYPE::GPU_READ)
	{
		Buffer stagingBuffer{};
		{
			VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

			VmaAllocationInfo allocationInfo{};
			vmaCreateBuffer(vk::GetAllocator(), &bufferInfo, &allocInfo,
				&stagingBuffer.vkbuffer, &stagingBuffer.vertexBufferAllocation, &allocationInfo);

			stagingBuffer.vkDeviceMemory = allocationInfo.deviceMemory;
		}

		auto* cmdList = GetCoreRender()->GetGraphicCommandList();
		cmdList->CommandsBegin();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;

		vkCmdCopyBuffer(*static_cast<VkGraphicCommandList*>(cmdList)->CommandBuffer(),
			stagingBuffer.vkbuffer, buffer.vkbuffer, 1, &copyRegion);

		cmdList->CommandsEnd();

		ICoreRenderer* renderer = engine::GetCoreRenderer();
		renderer->ExecuteCommandList(cmdList);

		renderer->WaitGPU(); // wait GPU copying
	}
	else
		notImplemented();
}

x12::VkCoreBuffer::Buffer::~Buffer()
{
	vmaDestroyBuffer(x12::vk::GetAllocator(), vkbuffer, vertexBufferAllocation);
}
