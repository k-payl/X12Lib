#include "pch.h"
#include "dx12uploadheap.h"
#include "dx12render.h"

x12::fastdescriptorallocator::Alloc x12::fastdescriptorallocator::Allocator::Allocate()
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
		currentPage = GetCoreRender()->GetPage();
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

void x12::fastdescriptorallocator::Allocator::Reset()
{
	for(Page *p : retiredPages)
		GetCoreRender()->ReleasePage(p);

	retiredPages.clear();

	if (currentPage)
	{
		GetCoreRender()->ReleasePage(currentPage);
		currentPage = nullptr;
		offset = 0;
	}
}

x12::fastdescriptorallocator::Page::~Page()
{
	if (ptr)
	{
		d3d12resource->Unmap(0, nullptr);
		ptr = 0;
	}
	descriptors.Free();
}
