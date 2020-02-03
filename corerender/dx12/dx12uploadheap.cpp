#include "pch.h"
#include "dx12uploadheap.h"
#include "dx12render.h"

static UINT descriptorsInPage = 500;

namespace FastFrameAllocator
{
	auto destroyPages = [](std::queue<Page*>& pages)
	{
		while (!pages.empty())
		{
			delete pages.front();
			pages.pop();
		}
	};
}

FastFrameAllocator::Page* FastFrameAllocator::PagePool::getPage()
{
	if (!avaliablePages.empty())
	{
		auto *ret = avaliablePages.front();
		avaliablePages.pop();
		return ret;
	}

	Page* page = new Page();

	UINT alignedSize = alignConstnatBufferSize(bufferSize);
	UINT pageSize = alignedSize * descriptorsInPage;

	// Upload heap for dynamic resources
	// For upload heap no need to put a fence to make sure the data is uploaded before you make a draw call
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(pageSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&page->d3d12resource)));

	// It is ok be mappped as long as you use buffer
	// because d3d11 driver does not version memory (d3d11 did) and cpu-pointer is always valid
	page->d3d12resource->Map(0, nullptr, &page->ptr);
	
	page->gpuPtr = page->d3d12resource->GetGPUVirtualAddress();

	// Prepare descriptors for page
	page->descriptors = GetCoreRender()->AllocateDescriptor(descriptorsInPage);
	SIZE_T ptrStart = page->descriptors.descriptor.ptr;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.SizeInBytes = alignConstnatBufferSize( bufferSize);

	auto descriptorIncrementSize = page->descriptors.descriptorIncrementSize;

	for (UINT i = 0; i < descriptorsInPage; ++i)
	{
		desc.BufferLocation = page->gpuPtr + i * 256;
		D3D12_CPU_DESCRIPTOR_HANDLE handle{ ptrStart + i * descriptorIncrementSize };

		CR_GetD3DDevice()->CreateConstantBufferView(&desc, handle);
	}

	return page;
}

void FastFrameAllocator::PagePool::releasePage(Page* page)
{
	avaliablePages.emplace(page);
}

FastFrameAllocator::PagePool::~PagePool()
{
	destroyPages(avaliablePages);
}

FastFrameAllocator::Alloc FastFrameAllocator::Allocator::Allocate()
{
	if (offset >= descriptorsInPage)
	{
		if (currentPage)
		{
			retiredPages.push_back(currentPage);
			currentPage = nullptr;
			offset = 0;
		}
	}

	if (currentPage == nullptr)
	{
		currentPage = pool->getPage();
		offset = 0;
	}

	Alloc ret
	{
		(uint8_t*)currentPage->ptr + offset * 256,
		currentPage->gpuPtr + offset * 256,
		currentPage->descriptors.descriptor.ptr + offset * currentPage->descriptors.descriptorIncrementSize
	};

	++offset;

	return ret;
}

void FastFrameAllocator::Allocator::FreeMemory()
{
	for(Page *p : retiredPages)
		pool->releasePage(p);

	retiredPages.clear();

	if (currentPage)
	{
		pool->releasePage(currentPage);
		currentPage = nullptr;
		offset = 0;
	}
}

FastFrameAllocator::Page::~Page()
{
	if (ptr)
	{
		d3d12resource->Unmap(0, nullptr);
		ptr = 0;
	}
	descriptors.Free();
}
