#pragma once
#include "vkcommon.h"

namespace x12
{
	struct VkCoreBuffer final : public ICoreBuffer
	{
	private:
		BUFFER_FLAGS flags{};
		MEMORY_TYPE memoryType{};
		size_t size{};
		void* ptr{};
		std::wstring name;

		struct Buffer
		{
			VkBuffer vkbuffer{};
			VmaAllocation vertexBufferAllocation{};
			VkDeviceMemory vkDeviceMemory{ VK_NULL_HANDLE };
			~Buffer();
		};

		Buffer buffer;

		void Map();
		void Unmap();

	public:
		VkCoreBuffer(size_t size, const void* data, MEMORY_TYPE memory_type, BUFFER_FLAGS flags, LPCWSTR name);

		X12_API void GetData(void* data) override;
		X12_API void SetData(const void* data, size_t size) override;
	};
}
