#include "pch.h"
#include "dx12memory.h"
#include "dx12render.h"

static std::mutex pagesMutex;
static std::vector<x12::memory::dynamic::Page*> allocatedPages; // all pages fixed size descriptorsInPage * ConstBufferSize
static std::vector<x12::memory::dynamic::Page*> avaliablePages;

void x12::memory::CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
										D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags,
										D3D12_RESOURCE_FLAGS resFlags)
{
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
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

	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&desc,
		state,
		clear,
		IID_PPV_ARGS(out)));
}

void x12::memory::dynamic::ReleasePage(x12::memory::dynamic::Page* page)
{
	std::scoped_lock lock(pagesMutex);
	avaliablePages.emplace_back(page);
}

void x12::memory::dynamic::Free()
{
	std::scoped_lock lock(pagesMutex);

	for (x12::memory::dynamic::Page* p : avaliablePages)
		delete p;

	avaliablePages.clear();
}

size_t x12::memory::dynamic::GetUsedVideoMemory()
{
	std::scoped_lock lock(pagesMutex);
	return allocatedPages.size() * PageSize;
}

size_t x12::memory::dynamic::GetUsedSystemMemory()
{
	std::scoped_lock lock(pagesMutex);
	return allocatedPages.size() * PageSize;
}

size_t x12::memory::dynamic::GetAllocatedPagesCount()
{
	std::scoped_lock lock(pagesMutex);
	return allocatedPages.size();
}

x12::memory::dynamic::Alloc x12::memory::dynamic::Allocator::Allocate()
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
		currentPage = GetPage();
		offset = 0;
	}

	Alloc ret
	{
		(uint8_t*)currentPage->ptr + offset * ConstBufferSize,
		currentPage->gpuPtr + offset * ConstBufferSize,
		currentPage->descriptors.descriptor.ptr + offset * currentPage->descriptors.descriptorIncrementSize
	};

	++offset;

	return ret;
}

void x12::memory::dynamic::Allocator::Reset()
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

x12::memory::dynamic::Page::~Page()
{
	if (ptr)
	{
		d3d12resource->Unmap(0, nullptr);
		ptr = 0;
	}
	descriptors.Free();
}

x12::memory::dynamic::Page* x12::memory::dynamic::GetPage()
{
	using namespace x12::memory::dynamic;

	std::unique_lock lock(pagesMutex);

	if (!avaliablePages.empty())
	{
		auto ret = avaliablePages.back();
		avaliablePages.pop_back();
		return ret;
	}

	UINT num = (UINT)allocatedPages.size();

	Page* page = new Page();
	allocatedPages.push_back(page);

	lock.unlock();

	// Upload heap for dynamic resources
	// For upload heap no need to put a fence to make sure the data is uploaded before you make a draw call
	x12::memory::CreateCommittedBuffer(&page->d3d12resource, PageSize, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

	set_name(page->d3d12resource.Get(), L"Page #%u dynamic upload buffer %u bytes", num, (UINT)PageSize);

	// It is ok be mappped as long as you use buffer
	// because d3d11 driver does not version memory (d3d11 did) and cpu-pointer is always valid
	page->d3d12resource->Map(0, nullptr, &page->ptr);

	page->gpuPtr = page->d3d12resource->GetGPUVirtualAddress();

	// Prepare descriptors for page
	page->descriptors = GetCoreRender()->AllocateDescriptor(descriptorsInPage);
	SIZE_T ptrStart = page->descriptors.descriptor.ptr;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.SizeInBytes = alignConstnatBufferSize(ConstBufferSize);

	auto descriptorIncrementSize = page->descriptors.descriptorIncrementSize;

	for (UINT i = 0; i < descriptorsInPage; ++i)
	{
		desc.BufferLocation = page->gpuPtr + i * ConstBufferSize;
		D3D12_CPU_DESCRIPTOR_HANDLE handle{ ptrStart + i * descriptorIncrementSize };

		CR_GetD3DDevice()->CreateConstantBufferView(&desc, handle);
	}

	return page;
}

