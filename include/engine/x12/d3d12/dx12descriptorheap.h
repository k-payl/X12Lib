#pragma once
#include "common.h"

namespace x12::descriptorheap
{
	class Page;
	class Allocator;

	struct Alloc
	{
		D3D12_CPU_DESCRIPTOR_HANDLE descriptor{0};
		uint32_t numDescriptors{};
		uint32_t descriptorIncrementSize{};
		std::shared_ptr<Page> page;

		Alloc() = default;
		Alloc(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t numDescriptors, uint32_t descriptorSize, std::shared_ptr<Page> page);

		~Alloc();

		Alloc(const Alloc&) = delete;
		Alloc& operator=(const Alloc&) = delete;

		Alloc(Alloc&& allocation);
		Alloc& operator=(Alloc&& other);

		bool IsNull() const { return descriptor.ptr == 0; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const;
		void Free();
	};

	//
	// Free-list allocator of descriptors.
	// Use for long lived static resources: textures, meshes, structured buffers
	//
	// Details: Allocator::Free() don't return the descriptors to the free list until the frame has completed
	class Allocator
	{
		friend class Page;

		D3D12_DESCRIPTOR_HEAP_TYPE heapType;

		uint32_t numDescriptorsPerPage{256};
		std::vector<std::shared_ptr<Page>> allPages;
		std::set<size_t> availablePages;

		std::function<uint64_t()> frameFn;

		std::shared_ptr<Page> CreateAllocatorPage();

	public:
		Allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, std::function<uint64_t()> frameFn_);

		Alloc Allocate(uint32_t numDescriptors = 1);
		void ReclaimMemory(uint64_t fenceID);
	};

	class Page : public std::enable_shared_from_this<Page>
	{
		struct FreeBlockInfo;
		using FreeListByOffset = std::map<uint32_t, FreeBlockInfo>;
		using FreeListBySize = std::multimap<uint32_t, FreeListByOffset::iterator>;

		struct FreeBlockInfo
		{
			FreeBlockInfo(uint32_t size)
				: Size(size)
			{}

			uint32_t Size;
			FreeListBySize::iterator FreeListBySizeIt;
		};

		struct StaleDescriptorInfo
		{
			StaleDescriptorInfo(uint32_t offset, uint32_t size, uint64_t frame)
				: Offset(offset)
				, Size(size)
				, FrameNumber(frame)
			{}

			uint32_t Offset;
			uint32_t Size;
			uint64_t FrameNumber;
		};

		FreeListByOffset freeListByOffset;
		FreeListBySize freeListBySize;

		std::queue<StaleDescriptorInfo> retiredDescriptorsQueue;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> d3d12DescriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE heapType;
		CD3DX12_CPU_DESCRIPTOR_HANDLE baseDescriptor;
		uint32_t numDescriptorsInPage;
		uint32_t numFreeHandles;

		Allocator& parent;

		uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);
		void AddNewBlock(uint32_t offset, uint32_t numDescriptors);
		void FreeBlock(uint32_t offset, uint32_t numDescriptors);

	public:
		Page(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, Allocator& parent_);

		D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const;
		bool HasSpace(uint32_t numDescriptors) const;
		uint32_t NumFreeHandles() const;
		Alloc Allocate(uint32_t numDescriptors);
		void Free(const Alloc& descriptorHandle);
		void ReleaseRetiredDescriptors(uint64_t frameNumber);
	};

}
