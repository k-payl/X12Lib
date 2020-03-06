#include "pch.h"
#include "dx12memory.h"
#include "dx12render.h"

static std::mutex pagesMutex;
static std::vector<x12::memory::fast::Page*> allocatedPages;
static std::vector<x12::memory::fast::Page*> avaliablePages;

void x12::memory::CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
										D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags,
										D3D12_RESOURCE_FLAGS resFlags)
{
	return ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&CD3DX12_RESOURCE_DESC::Buffer(size, resFlags),
		state,
		nullptr,
		IID_PPV_ARGS(out)));
}

void x12::memory::CreateCommitted2DTexture(ID3D12Resource** out, UINT width, UINT height,
										   UINT mipCount, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resFlags,
										   D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear, D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = static_cast<UINT16>(mipCount);
	desc.DepthOrArraySize = 1;
	desc.Format = format;
	desc.Flags = resFlags;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	return ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&desc,
		state,
		clear,
		IID_PPV_ARGS(out)));
}

void x12::memory::fast::ReleasePage(x12::memory::fast::Page* page)
{
	std::scoped_lock lock(pagesMutex);
	avaliablePages.emplace_back(page);
}

void x12::memory::fast::Free()
{
	std::scoped_lock lock(pagesMutex);

	for (x12::memory::fast::Page* p : avaliablePages)
		delete p;

	avaliablePages.clear();
}

x12::memory::fast::Alloc x12::memory::fast::Allocator::Allocate()
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
		currentPage = GetPage(256);
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

void x12::memory::fast::Allocator::Reset()
{
	for (Page* p : retiredPages)
		ReleasePage(p);

	retiredPages.clear();

	if (currentPage)
	{
		ReleasePage(currentPage);
		currentPage = nullptr;
		offset = 0;
	}
}

x12::memory::fast::Page::~Page()
{
	if (ptr)
	{
		d3d12resource->Unmap(0, nullptr);
		ptr = 0;
	}
	descriptors.Free();
}


x12::memory::fast::Page* x12::memory::fast::GetPage(UINT size)
{
	using namespace x12::memory::fast;

	std::unique_lock lock(pagesMutex);

	if (!avaliablePages.empty())
	{
		auto ret = avaliablePages.back();
		avaliablePages.pop_back();
		return ret;
	}

	Page* page = new Page();
	allocatedPages.push_back(page);

	lock.unlock();

	UINT alignedSize = alignConstnatBufferSize(size);
	UINT pageSize = alignedSize * descriptorsInPage;

	// Upload heap for dynamic resources
	// For upload heap no need to put a fence to make sure the data is uploaded before you make a draw call
	x12::memory::CreateCommittedBuffer(&page->d3d12resource, pageSize, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

	// It is ok be mappped as long as you use buffer
	// because d3d11 driver does not version memory (d3d11 did) and cpu-pointer is always valid
	page->d3d12resource->Map(0, nullptr, &page->ptr);

	page->gpuPtr = page->d3d12resource->GetGPUVirtualAddress();

	// Prepare descriptors for page
	page->descriptors = GetCoreRender()->AllocateDescriptor(descriptorsInPage);
	SIZE_T ptrStart = page->descriptors.descriptor.ptr;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.SizeInBytes = alignConstnatBufferSize(size);

	auto descriptorIncrementSize = page->descriptors.descriptorIncrementSize;

	for (UINT i = 0; i < descriptorsInPage; ++i)
	{
		desc.BufferLocation = page->gpuPtr + i * 256;
		D3D12_CPU_DESCRIPTOR_HANDLE handle{ ptrStart + i * descriptorIncrementSize };

		CR_GetD3DDevice()->CreateConstantBufferView(&desc, handle);
	}

	return page;
}

