#pragma once
#include "common.h"
#include "descriptorheap.h"


namespace FastFrameAllocator
{
	struct Page;
	class PagePool;

	struct Alloc
	{
		void* ptr;								// The CPU-writeable address
		D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;		// The GPU-visible address
		D3D12_CPU_DESCRIPTOR_HANDLE descriptor;

		Alloc() = default;

		Alloc(void* ptr_, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr_, SIZE_T h) :
			ptr(ptr_),
			gpuPtr(gpuPtr_),
			descriptor{h}
		{}
	};

	class Allocator
	{
		Page* currentPage{};
		std::vector<Page*> retiredPages;
		uint32_t offset{};
		PagePool *pool;

	public:
		Allocator(PagePool* pool_) { pool = pool_; }
		Alloc Allocate();
		void FreeMemory();
	};

	struct Page
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> d3d12resource;
		void* ptr{};
		D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
		DescriptorHeap::Alloc descriptors;

		Page() = default;
		~Page();
	};

	class PagePool
	{
		std::queue<Page*> avaliablePages;
		UINT bufferSize;

	public:
		PagePool(UINT bufferSize_) :
			bufferSize(bufferSize_)
		{}

		Page* getPage();
		void releasePage(Page* page);

		~PagePool();
	};
}

